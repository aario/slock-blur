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

#if HAVE_BSD_AUTH
#include <login_cap.h>
#include <bsd_auth.h>
#endif

//#define DEBUG

// Fast Gaussian Blur v1.3
// by Mario Klingemann <http://incubator.quasimondo.com>

// One of my first steps with Processing. I am a fan
// of blurring. Especially as you can use blurred images
// as a base for other effects. So this is something I
// might get back to in later experiments.
//
// What you see is an attempt to implement a Gaussian Blur algorithm
// which is exact but fast. I think that this one should be
// relatively fast because it uses a special trick by first
// making a horizontal blur on the original image and afterwards
// making a vertical blur on the pre-processed image-> This
// is a mathematical correct thing to do and reduces the
// calculation a lot.
//
// In order to avoid the overhead of function calls I unrolled
// the whole convolution routine in one method. This may not
// look nice, but brings a huge performance boost.
//
//
// v1.1: I replaced some multiplications by additions
//       and added aome minor pre-caclulations.
//       Also add correct rounding for float->int conversion
//
// v1.2: I completely got rid of all floating point calculations
//       and speeded up the whole process by using a
//       precalculated multiplication table. Unfortunately
//       a precalculated division table was becoming too
//       huge. But maybe there is some way to even speed
//       up the divisions.
//
// v1.3: Fixed a bug that caused blurs that start at y>0
//	 to go wrong. Thanks to Jeroen Schellekens for 
//       finding it!
typedef struct {
	unsigned char *pix;
	int x;
	int y;
	int w;
	int y2;
	int H;
	int wm;
	int wh;
	int *r;
	int *g;
	int *b;
	int *dv;
	int radius;
	int *vminx;
	int *vminy;
} StackBlurRenderingParams;

#include <pthread.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
void *HStackRenderingThread(void *arg) {
	StackBlurRenderingParams *rp=(StackBlurRenderingParams*)arg;
	int rinsum,ginsum,binsum,routsum,goutsum,boutsum,rsum,gsum,bsum,x,y,i,yi,yw,rbs,p,sp;
	int div=rp->radius+rp->radius+1;
	int *stackr=malloc(div*sizeof(int));
	int *stackg=malloc(div*sizeof(int));
	int *stackb=malloc(div*sizeof(int));
	yw=yi=rp->y*rp->w;
	int r1=rp->radius+1;
	for (y=rp->y;y<rp->y2;y++){
		rinsum=ginsum=binsum=routsum=goutsum=boutsum=rsum=gsum=bsum=0;
		for(i=-rp->radius;i<=rp->radius;i++){
			p=(yi+MIN(rp->wm,MAX(i,0)))*4;
			sp=i+rp->radius;
			stackr[sp]=rp->pix[p];
			stackg[sp]=rp->pix[p+1];
			stackb[sp]=rp->pix[p+2];
			rbs=r1-abs(i);
			rsum+=stackr[sp]*rbs;
			gsum+=stackg[sp]*rbs;
			bsum+=stackb[sp]*rbs;
			if (i>0){
				rinsum+=stackr[sp];
				ginsum+=stackg[sp];
				binsum+=stackb[sp];
			} else {
				routsum+=stackr[sp];
				goutsum+=stackg[sp];
				boutsum+=stackb[sp];
			}
		}
		int stackpointer;
		int stackstart;
		stackpointer=rp->radius;

		for (x=rp->x;x<rp->w;x++){
			rp->r[yi]=rp->dv[rsum];
			rp->g[yi]=rp->dv[gsum];
			rp->b[yi]=rp->dv[bsum];
			
			rsum-=routsum;
			gsum-=goutsum;
			bsum-=boutsum;

			stackstart=stackpointer-rp->radius+div;
			sp=stackstart%div;
			
			routsum-=stackr[sp];
			goutsum-=stackg[sp];
			boutsum-=stackb[sp];
			
			p=(yw+rp->vminx[x])*4;
			stackr[sp]=rp->pix[p];
			stackg[sp]=rp->pix[p+1];
			stackb[sp]=rp->pix[p+2];

			rinsum+=stackr[sp];
			ginsum+=stackg[sp];
			binsum+=stackb[sp];

			rsum+=rinsum;
			gsum+=ginsum;
			bsum+=binsum;
			
			stackpointer=(stackpointer+1)%div;
			sp=stackpointer%div;
			
			routsum+=stackr[sp];
			goutsum+=stackg[sp];
			boutsum+=stackb[sp];
			
			rinsum-=stackr[sp];
			ginsum-=stackg[sp];
			binsum-=stackb[sp];
			
			yi++;
		}
		yw+=rp->w;
	}
	free(stackr);
	free(stackg);
	free(stackb);
	stackr=stackg=stackb=NULL;
    pthread_exit(NULL);
}

void *VStackRenderingThread(void *arg) {
	StackBlurRenderingParams *rp=(StackBlurRenderingParams*)arg;
	int rinsum,ginsum,binsum,routsum,goutsum,boutsum,rsum,gsum,bsum,x,y,i,yi,yp,rbs,p,sp;
	int div=rp->radius+rp->radius+1;
	int divsum=(div+1)>>1;
	divsum*=divsum;
	int *stackr=malloc(div*sizeof(int));
	int *stackg=malloc(div*sizeof(int));
	int *stackb=malloc(div*sizeof(int));
	int r1=rp->radius+1;
	int hm=rp->H-rp->y-1;
	for (x=rp->x;x<rp->w;x++) {
		rinsum=ginsum=binsum=routsum=goutsum=boutsum=rsum=gsum=bsum=0;
		yp=(rp->y-rp->radius)*rp->w;
		for(i=-rp->radius;i<=rp->radius;i++) {
			yi=MAX(0,yp)+x;
			sp=i+rp->radius;

			stackr[sp]=rp->r[yi];
			stackg[sp]=rp->g[yi];
			stackb[sp]=rp->b[yi];
			
			rbs=r1-abs(i);
			
			rsum+=rp->r[yi]*rbs;
			gsum+=rp->g[yi]*rbs;
			bsum+=rp->b[yi]*rbs;
			
			if (i>0){
				rinsum+=stackr[sp];
				ginsum+=stackg[sp];
				binsum+=stackb[sp];
			} else {
				routsum+=stackr[sp];
				goutsum+=stackg[sp];
				boutsum+=stackb[sp];
			}
			
			if(i<hm){
				yp+=rp->w;
			}
		}
		yi=rp->y*rp->w+x;
		int stackpointer;
		int stackstart;
		stackpointer=rp->radius;

		for (y=rp->y;y<rp->y2;y++) {
			p=yi*4;
#ifdef DEBUG
// 		fprintf(stdout,"y: %i %i %i 1\n",rp->y, x, y);
#endif
 			rp->pix[p]=(unsigned char)(rp->dv[rsum]);
 			rp->pix[p+1]=(unsigned char)(rp->dv[gsum]);
 			rp->pix[p+2]=(unsigned char)(rp->dv[bsum]);
 			rp->pix[p+3]=0xff;
#ifdef DEBUG
// 		fprintf(stdout,"y: %i 2\n",rp->y);
#endif

			rsum-=routsum;
			gsum-=goutsum;
			bsum-=boutsum;

			stackstart=stackpointer-rp->radius+div;
			sp=stackstart%div;
			
			routsum-=stackr[sp];
			goutsum-=stackg[sp];
			boutsum-=stackb[sp];
			
			p=x+rp->vminy[y];
			stackr[sp]=rp->r[p];
			stackg[sp]=rp->g[p];
			stackb[sp]=rp->b[p];
			
			rinsum+=stackr[sp];
			ginsum+=stackg[sp];
			binsum+=stackb[sp];

			rsum+=rinsum;
			gsum+=ginsum;
			bsum+=binsum;

			stackpointer=(stackpointer+1)%div;
			
			routsum+=stackr[stackpointer];
			goutsum+=stackg[stackpointer];
			boutsum+=stackb[stackpointer];
			
			rinsum-=stackr[stackpointer];
			ginsum-=stackg[stackpointer];
			binsum-=stackb[stackpointer];

			yi+=rp->w;
		}
	}
	free(stackr);
	free(stackg);
	free(stackb);
	stackr=stackg=stackb=NULL;
    pthread_exit(NULL);
}

void stackblur(XImage *image,int x, int y,int w,int h,int radius, unsigned int num_threads) {
	if (radius<1)
		return;
	char *pix=image->data;
	int wh=w*h;
	int *r=malloc(wh*sizeof(int));
	int *g=malloc(wh*sizeof(int));
	int *b=malloc(wh*sizeof(int));
	int i;

	int div=radius+radius+1;
	int divsum=(div+1)>>1;
	divsum*=divsum;
	int *dv=malloc(256*divsum*sizeof(int));
	for (i=0;i<256*divsum;i++) {
		dv[i]=(i/divsum);
	}
	int *vminx=malloc(w*sizeof(int));
	for (i=0;i<w;i++)
		vminx[i]=MIN(i+radius+1,w-1);
	int *vminy=malloc(h*sizeof(int));
	for (i=0;i<h;i++)
		vminy[i]=MIN(i+radius+1,h-1)*w;

	pthread_t *pthh=malloc(num_threads*sizeof(pthread_t));
	StackBlurRenderingParams *rp=malloc(num_threads*sizeof(StackBlurRenderingParams));
	int threadY=y;
	int threadH=(h/num_threads);
 	for (i=0;i<num_threads;i++) {
		rp[i].pix=(unsigned char*)pix;
		rp[i].x=x;
		rp[i].w=w;
		rp[i].y=threadY;
		//Below "if" is to avoid vertical threads running on the same line when h/num_threads is not a round number i.e. 1080 lines / 16 threads = 67.5 lines!
		if (i==num_threads-1)//last turn
			rp[i].y2=y+h;
		else
			rp[i].y2=threadY+threadH;
 		rp[i].H=h;
		rp[i].wm=rp[i].w-1;
		rp[i].wh=wh;
		rp[i].r=r;
		rp[i].g=g;
		rp[i].b=b;
		rp[i].dv=dv;
		rp[i].radius=radius;
		rp[i].vminx=vminx;
		rp[i].vminy=vminy;
#ifdef DEBUG
		fprintf(stdout,"HThread: %i X: %i Y: %i W: %i H: %i x: %i y: %i w: %i h: %i\n",i,x,y,w,h,rp[i].x,rp[i].y,rp[i].w,threadH);
#endif
		pthread_create(&pthh[i],NULL,HStackRenderingThread,(void*)&rp[i]);
		threadY+=threadH;
	}
	for (i=0;i<num_threads;i++)
		pthread_join(pthh[i],NULL);
	pthread_t *pthv=malloc(num_threads*sizeof(pthread_t));
	for (i=0;i<num_threads;i++) {
#ifdef DEBUG
 		fprintf(stdout,"VThread: %i X: %i Y: %i W: %i H: %i x: %i y: %i w: %i h: %i\n",i,x,y,w,h,rp[i].x,rp[i].y,rp[i].w,threadH);
#endif
		pthread_create(&pthv[i],NULL,VStackRenderingThread,(void*)&rp[i]);
	}
	for (i=0;i<num_threads;i++)
		pthread_join(pthv[i],NULL);
	free(vminx);
	free(vminy);
	free(rp);
	free(r);
	free(g);
	free(b);
	free(dv);
	free(pthh);
	free(pthv);
	rp=NULL;
	dv=vminx=vminy=r=g=b=NULL;
	pthh=pthv=NULL;
#ifdef DEBUG
 	fprintf(stdout,"Done.\n");
#endif
}
//End of Fast Gaussian Blur v1.3

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
