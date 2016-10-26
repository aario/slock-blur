/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "stackblur.h"

#if HAVE_BSD_AUTH
#include <login_cap.h>
#include <bsd_auth.h>
#endif

enum {
	INIT,
	INPUT,
	FAILED,
	NUMLEVELS
};

#include "config.h"

typedef struct {
	int screen;
	Window root, win;
	Pixmap pmap;
    XImage *image, *originalimage;
} Lock;

static Lock **locks;
static int nscreens;
static Bool running = True;
static Bool failure = False;
static Bool rr;
static int rrevbase;
static int rrerrbase;

static void
blurlockwindow(Display *dpy, Lock *lock, int radius)
{
    XWindowAttributes gwa;
    XGetWindowAttributes(dpy, lock->root, &gwa);
    if (lock->image != NULL) {
        free(lock->image);
        free(lock->image->data);
    }
    lock->image=malloc(sizeof(XImage));
    memcpy(lock->image,lock->originalimage,sizeof(XImage));
    unsigned long bytes2copy=sizeof(char)*lock->originalimage->bytes_per_line*lock->originalimage->height;
    lock->image->data=malloc(bytes2copy);
    memcpy(lock->image->data,lock->originalimage->data,bytes2copy);
    stackblur(lock->image,0,0,lock->image->width,lock->image->height,radius, CPU_THREADS);
	XMapRaised(dpy, lock->win);
    GC gc = XCreateGC (dpy, lock->win, 0, 0);
    XPutImage(dpy, lock->win, gc, lock->image, 0, 0, 0, 0, gwa.width, gwa.height);
    XFlush(dpy);
}

static void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

#ifdef __linux__
#include <fcntl.h>
#include <linux/oom.h>

static void
dontkillme(void)
{
	int fd;
	int length;
	char value[64];

	fd = open("/proc/self/oom_score_adj", O_WRONLY);
	if (fd < 0 && errno == ENOENT)
		return;

	/* convert OOM_SCORE_ADJ_MIN to string for writing */
	length = snprintf(value, sizeof(value), "%d\n", OOM_SCORE_ADJ_MIN);

	/* bail on truncation */
	if (length >= sizeof(value))
		die("buffer too small\n");

	if (fd < 0 || write(fd, value, length) != length || close(fd) != 0)
		die("cannot disable the out-of-memory killer for this process\n");
}
#endif

#ifndef HAVE_BSD_AUTH
/* only run as root */
static const char *
getpw(void)
{
	const char *rval;
	struct passwd *pw;

	errno = 0;
	pw = getpwuid(getuid());
	if (!pw) {
		if (errno)
			die("slock: getpwuid: %s\n", strerror(errno));
		else
			die("slock: cannot retrieve password entry\n");
	}
	rval =  pw->pw_passwd;

#if HAVE_SHADOW_H
	if (rval[0] == 'x' && rval[1] == '\0') {
		struct spwd *sp;
		sp = getspnam(getenv("USER"));
		if (!sp)
			die("slock: cannot retrieve shadow entry (make sure to suid or sgid slock)\n");
		rval = sp->sp_pwdp;
	}
#endif

	/* drop privileges */
	if (geteuid() == 0 &&
	    ((getegid() != pw->pw_gid && setgid(pw->pw_gid) < 0) || setuid(pw->pw_uid) < 0))
		die("slock: cannot drop privileges\n");
	return rval;
}
#endif

static void
#ifdef HAVE_BSD_AUTH
readpw(Display *dpy)
#else
readpw(Display *dpy, const char *pws)
#endif
{
	char buf[32], passwd[256];
	int num, screen;
	unsigned int len, level;
	KeySym ksym;
	XEvent ev;
	static int oldl = INIT;

	len = 0;
	running = True;

	/* As "slock" stands for "Simple X display locker", the DPMS settings
	 * had been removed and you can set it with "xset" or some other
	 * utility. This way the user can easily set a customized DPMS
	 * timeout. */
	while (running && !XNextEvent(dpy, &ev)) {
		if (ev.type == KeyPress) {
			buf[0] = 0;
			num = XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, 0);
			if (IsKeypadKey(ksym)) {
				if (ksym == XK_KP_Enter)
					ksym = XK_Return;
				else if (ksym >= XK_KP_0 && ksym <= XK_KP_9)
					ksym = (ksym - XK_KP_0) + XK_0;
			}
			if (IsFunctionKey(ksym) ||
			    IsKeypadKey(ksym) ||
			    IsMiscFunctionKey(ksym) ||
			    IsPFKey(ksym) ||
			    IsPrivateKeypadKey(ksym))
				continue;
			switch (ksym) {
			case XK_Return:
				passwd[len] = 0;
#ifdef HAVE_BSD_AUTH
				running = !auth_userokay(getlogin(), NULL, "auth-xlock", passwd);
#else
				running = !!strcmp(crypt(passwd, pws), pws);
#endif
				if (running) {
					XBell(dpy, 100);
					failure = True;
				}
				len = 0;
				break;
			case XK_Escape:
				len = 0;
				break;
			case XK_BackSpace:
				if (len)
					--len;
				break;
			default:
				if (num && !iscntrl((int) buf[0]) && (len + num < sizeof(passwd))) {
					memcpy(passwd + len, buf, num);
					len += num;
				}
				break;
			}
			level = len ? INPUT : (failure || failonclear ? FAILED : INIT);
			if (running && oldl != level) {
				for (screen = 0; screen < nscreens; screen++)
                    blurlockwindow(dpy,locks[screen],blurlevel[level]);
				oldl = level;
			}
		} else if (rr && ev.type == rrevbase + RRScreenChangeNotify) {
			XRRScreenChangeNotifyEvent *rre = (XRRScreenChangeNotifyEvent*)&ev;
			for (screen = 0; screen < nscreens; screen++) {
				if (locks[screen]->win == rre->window) {
					XResizeWindow(dpy, locks[screen]->win, rre->width, rre->height);
                    blurlockwindow(dpy,locks[screen],blurlevel[INIT]);
				}
			}
		} else for (screen = 0; screen < nscreens; screen++)
			XRaiseWindow(dpy, locks[screen]->win);
	}
}

static void
unlockscreen(Display *dpy, Lock *lock)
{
	if(dpy == NULL || lock == NULL)
		return;

	XUngrabPointer(dpy, CurrentTime);
	XDestroyImage(lock->image);
	XFreePixmap(dpy, lock->pmap);
	XDestroyWindow(dpy, lock->win);

	free(lock);
}

static Lock *
lockscreen(Display *dpy, int screen)
{
	char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
	unsigned int len;
	Lock *lock;
	XColor color;
	XSetWindowAttributes wa;
	Cursor invisible;

	if (dpy == NULL || screen < 0)
		return NULL;

	lock = malloc(sizeof(Lock));
	if (lock == NULL)
		return NULL;

	lock->screen = screen;

	lock->root = RootWindow(dpy, lock->screen);

	/* init */
	wa.override_redirect = 1;
	lock->win =  XCreateWindow(dpy, lock->root, 0, 0, DisplayWidth(dpy, lock->screen), DisplayHeight(dpy, lock->screen),
	                          0, DefaultDepth(dpy, lock->screen), CopyFromParent,
	                          DefaultVisual(dpy, lock->screen), CWOverrideRedirect | CWBackPixel, &wa);
	lock->pmap = XCreateBitmapFromData(dpy, lock->win, curs, 8, 8);
	invisible = XCreatePixmapCursor(dpy, lock->pmap, lock->pmap, &color, &color, 0, 0);
	XDefineCursor(dpy, lock->win, invisible);
    XWindowAttributes gwa;
    XGetWindowAttributes(dpy, lock->root, &gwa);
    lock->originalimage=XGetImage(dpy,lock->root, 0, 0, gwa.width, gwa.height, AllPlanes, ZPixmap);
    lock->image=NULL;
    blurlockwindow(dpy,lock,blurlevel[INIT]);
	if (rr)
		XRRSelectInput(dpy, lock->win, RRScreenChangeNotifyMask);
	for (len = 1000; len; len--) {
		if (XGrabPointer(dpy, lock->root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		    GrabModeAsync, GrabModeAsync, None, invisible, CurrentTime) == GrabSuccess)
			break;
		usleep(1000);
	}
	if (running && (len > 0)) {
		for (len = 1000; len; len--) {
			if (XGrabKeyboard(dpy, lock->root, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
				break;
			usleep(1000);
		}
	}

	running &= (len > 0);
	if (!running) {
		unlockscreen(dpy, lock);
		lock = NULL;
	}
	else {
		XSelectInput(dpy, lock->root, SubstructureNotifyMask);
	}

	return lock;
}

static void
usage(void)
{
	fprintf(stderr, "usage: slock [-v]\n");
	exit(1);
}

int
main(int argc, char **argv) {
#ifndef HAVE_BSD_AUTH
	const char *pws;
#endif
	Display *dpy;
	int screen;

	if ((argc == 2) && !strcmp("-v", argv[1]))
		die("slock-%s, © 2006-2015 slock engineers\n", VERSION);
	else if (argc != 1)
		usage();

#ifdef __linux__
	dontkillme();
#endif

	if (!getpwuid(getuid()))
		die("slock: no passwd entry for you\n");

#ifndef HAVE_BSD_AUTH
	pws = getpw();
#endif

	if (!(dpy = XOpenDisplay(0)))
		die("slock: cannot open display\n");
	rr = XRRQueryExtension(dpy, &rrevbase, &rrerrbase);
	/* Get the number of screens in display "dpy" and blank them all. */
	nscreens = ScreenCount(dpy);
	locks = malloc(sizeof(Lock *) * nscreens);
	if (locks == NULL)
		die("slock: malloc: %s\n", strerror(errno));
	int nlocks = 0;
	for (screen = 0; screen < nscreens; screen++) {
		if ( (locks[screen] = lockscreen(dpy, screen)) != NULL)
			nlocks++;
	}
	XSync(dpy, False);

	/* Did we actually manage to lock something? */
	if (nlocks == 0) { /* nothing to protect */
		free(locks);
		XCloseDisplay(dpy);
		return 1;
	}

	/* Everything is now blank. Now wait for the correct password. */
#ifdef HAVE_BSD_AUTH
	readpw(dpy);
#else
	readpw(dpy, pws);
#endif

	/* Password ok, unlock everything and quit. */
	for (screen = 0; screen < nscreens; screen++)
		unlockscreen(dpy, locks[screen]);

	free(locks);
	XCloseDisplay(dpy);

	return 0;
}
