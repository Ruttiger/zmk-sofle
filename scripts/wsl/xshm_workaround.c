/*
 * xshm_workaround.c — Replace XShmPutImage with XPutImage.
 *
 * WSL/XWayland crashes in XShmPutImage when called from a non-main POSIX
 * thread (which is how Zephyr native_posix_64 runs its kernel threads).
 * XPutImage uses the normal X11 socket path and is safe in this context.
 *
 * Build (done automatically by preview-display-wsl.sh):
 *   gcc -shared -fPIC -O2 -o xshm_workaround.so xshm_workaround.c -lX11
 *
 * Use:
 *   LD_PRELOAD=/path/to/xshm_workaround.so ./zmk.exe
 */
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

Bool XShmPutImage(Display *dpy, Drawable d, GC gc, XImage *image,
                  int src_x, int src_y, int dest_x, int dest_y,
                  unsigned int width, unsigned int height, Bool send_event)
{
    (void)send_event;
    XPutImage(dpy, d, gc, image, src_x, src_y, dest_x, dest_y, width, height);
    return True;
}
