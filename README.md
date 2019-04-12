slock-blur - simple screen locker
============================
Super fast and highly stable blur version of the simple screen locker 
utility for X. It takes a screenshot of your desktop and blurs it and
remain there until you enter your password.

![Screenshot](screenshot.png?raw=true "Fully CPU Based No OpenGL Static Blur Effect")

Debian Patches
--------------
Applied Debian Sid patches to support PAM instead of using Shadow.
More information at:
  https://packages.debian.org/source/sid/suckless-tools
All patches applied:
- slock-Fix-resize-with-multiple-monitors-and-portrait-mode.patch
- Use-PAM-for-authentication.patch
- slock-there-can-only-be-one-window-in-the-event.patch
- Don-t-exit-if-failed-to-adjust-OOM-score.patch
- Remove-custom-library-search-paths-from-Makefiles.patch
- slock-Properly-clear-the-last-entered-character.patch
- slock-Do-not-drop-privileges.patch
Also added man file from debian package.

Requirements
------------
In order to build slock you need the Xlib header files.

DO NOT PANIC!
------------
- This blur effect does not crash
- This blur effect does not need OpenGL
- This blur effect only relies on Xlib and your CPU for rendering
- This blur effect is multi-threaded (It takes less that 30ms for bluring 
whole desktop of a 10inch Intel Atom Laptop)

Installation
------------
Edit config.mk to match your local setup (slock is installed into
the /usr/local namespace by default).

Afterwards enter the following command to build and install slock
(if necessary as root):

    make clean install


Running slock
-------------
Simply invoke the 'slock' command. To get out of it, enter your password.
.

## THE STACK-BLUR PART OF THE CODE IS BASED ON GREATE WORK BY:
Mario Klingemann <mario@quasimondo.com>

Link to the original work:
- http://incubator.quasimondo.com/processing/fast_blur_deluxe.php

The original source code of stack-blur:
- http://incubator.quasimondo.com/processing/stackblur.pde
