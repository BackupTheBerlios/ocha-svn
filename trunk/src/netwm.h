#ifndef NETWM_H
#define NETWM_H

#include <X11/Xlib.h>
#include <glib.h>

/** \file NetWM utilities.
 *
 * These functions were ripped off from wmctrl 1.05,
 * they've been originally written Tomas Styblo <tripie@cpan.org>
 * and released under the GPL (2+)
 */

/**
 * Get the list of windows in the current window manager.
 *
 * @param display X display
 * @param size size of the array returned by the function (OUT)
 * @return an array whose size is given by *size, to be freed
 * using g_free()
 */
Window *netwm_get_client_list (Display *disp, unsigned long *size);

/**
 * Activate a window, possibly changing the current desktop.
 *
 * @param disp X display
 * @param win the window to activate
 * @param switch_desktop if true, move to the window's desktop first
 * @return true if activating the window worked
 */
gboolean netwm_activate_window (Display *disp, Window win, gboolean switch_desktop);

/**
 * Get the title of a window in UTF-8.
 *
 * @param disp display
 * @param win window
 * @return NULL or an utf-8 string to free using g_free
 */
char *netwm_get_window_title(Display *disp, Window win);

#endif /*NETWM_H*/
