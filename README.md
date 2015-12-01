slock-blur - simple screen locker
============================
Super fast and highly stable blur version of the simple screen locker 
utility for X. It takes a screenshot of your desktop and blurs it and
remain there until you enter your password.


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
