
/** \file Ripped of from wmctrl and later heavily modified.
 
Original copyright notice:
 
wmctrl
A command line tool to interact with an EWMH/NetWM compatible X Window Manager.
 
Author, current maintainer: Tomas Styblo <tripie@cpan.org>
 
Copyright (C) 2003
 
This program is free software which I release under the GNU General Public
License. You may redistribute and/or modify this program under the terms
of that license as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
 
To get a copy of the GNU General Puplic License,  write to the
Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
*/

#include "netwm.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>
#include <glib.h>


#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */


#define MAX_PROPERTY_VALUE_LEN 4096
#define SELECT_WINDOW_MAGIC ":SELECT:"

#define p_verbose(...) ((void *)0)

/* ------------------------- declarations of static functions */
static gboolean wm_supports (Display *disp, const gchar *prop);
static Window *get_client_list (Display *disp, unsigned long *size);
static int client_msg(Display *disp, Window win, char *msg,
                      unsigned long data0, unsigned long data1,
                      unsigned long data2, unsigned long data3,
                      unsigned long data4);
static int list_windows (Display *disp);
static int list_desktops (Display *disp);
static int showing_desktop (Display *disp);
static int change_viewport (Display *disp);
static int change_geometry (Display *disp);
static int change_number_of_desktops (Display *disp);
static int switch_desktop (Display *disp);
static int wm_info (Display *disp);
static gchar *get_output_str (gchar *str, gboolean is_utf8);
static int action_window (Display *disp, Window win, char mode);
static int action_window_pid (Display *disp, char mode);
static int action_window_str (Display *disp, char mode);
static int activate_window (Display *disp, Window win,
                            gboolean switch_desktop);
static int close_window (Display *disp, Window win);
static int longest_str (gchar **strv);
static int window_to_desktop (Display *disp, Window win, int desktop);
static void window_set_title (Display *disp, Window win, char *str, char mode);
static gchar *get_window_class (Display *disp, Window win);
static gchar *get_window_title (Display *disp, Window win);
static gchar *get_property (Display *disp, Window win,
                            Atom xa_prop_type, gchar *prop_name, unsigned long *size);
static void init_charset(void);
static int window_move_resize (Display *disp, Window win, char *arg);
static int window_state (Display *disp, Window win, char *arg);
static Window Select_Window(Display *dpy);

/* ------------------------- public functions */

Window *netwm_get_client_list (Display *disp, unsigned long *size)
{
        return get_client_list(disp, size);
}

gboolean netwm_activate_window (Display *disp, Window win, gboolean switch_desktop)
{
        return activate_window(disp, win, switch_desktop)==EXIT_SUCCESS;
}

char *netwm_get_window_title(Display *disp, Window win)
{
        return get_window_title(disp, win);
}

char *netwm_get_window_class(Display *disp, Window win)
{
        return get_window_class(disp, win);
}

char *netwm_get_window_host(Display *disp, Window win)
{
        char *host = get_property(disp, win,
                                  XA_STRING,
                                  "WM_CLIENT_MACHINE",
                                  NULL);
        if(host==NULL)
                return NULL;
        char *host_utf8 = g_locale_to_utf8(host, -1, NULL, NULL, NULL);
        g_free(host);
        return host_utf8;
}

/* ------------------------- static functions */

static int client_msg(Display *disp, Window win, char *msg, /* {{{ */
                      unsigned long data0, unsigned long data1,
                      unsigned long data2, unsigned long data3,
                      unsigned long data4)
{
        XEvent event;
        long mask = SubstructureRedirectMask | SubstructureNotifyMask;

        event.xclient.type = ClientMessage;
        event.xclient.serial = 0;
        event.xclient.send_event = True;
        event.xclient.message_type = XInternAtom(disp, msg, False);
        event.xclient.window = win;
        event.xclient.format = 32;
        event.xclient.data.l[0] = data0;
        event.xclient.data.l[1] = data1;
        event.xclient.data.l[2] = data2;
        event.xclient.data.l[3] = data3;
        event.xclient.data.l[4] = data4;

        if (XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
                return EXIT_SUCCESS;
        } else {
                fprintf(stderr, "Cannot send %s event.\n", msg);
                return EXIT_FAILURE;
        }
}/*}}}*/

static int activate_window (Display *disp, Window win, /* {{{ */
                            gboolean switch_desktop)
{
        unsigned long *desktop;

        /* desktop ID */
        if ((desktop = (unsigned long *)get_property(disp, win,
                        XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
                if ((desktop = (unsigned long *)get_property(disp, win,
                                XA_CARDINAL, "_WIN_WORKSPACE", NULL)) == NULL) {
                        p_verbose("Cannot find desktop ID of the window.\n");
                }
        }

        if (switch_desktop && desktop) {
                if (client_msg(disp, DefaultRootWindow(disp),
                                "_NET_CURRENT_DESKTOP",
                                *desktop, 0, 0, 0, 0) != EXIT_SUCCESS) {
                        p_verbose("Cannot switch desktop.\n");
                }
                g_free(desktop);
        }

        client_msg(disp, win, "_NET_ACTIVE_WINDOW",
                   0, 0, 0, 0, 0);
        XMapRaised(disp, win);

        return EXIT_SUCCESS;
}/*}}}*/

static Window *get_client_list (Display *disp, unsigned long *size)
{/*{{{*/
        Window *client_list;

        if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp),
                           XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
                if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp),
                                   XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
                        fputs("Cannot get client list properties. \n"
                              "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)"
                              "\n", stderr);
                        return NULL;
                }
        }

        return client_list;
}/*}}}*/


static gchar *get_window_title (Display *disp, Window win)
{/*{{{*/
        gchar *title_utf8;
        gchar *wm_name;
        gchar *net_wm_name;

        wm_name = get_property(disp, win, XA_STRING, "WM_NAME", NULL);
        net_wm_name = get_property(disp, win,
                                   XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL);

        if (net_wm_name) {
                title_utf8 = g_strdup(net_wm_name);
        } else {
                if (wm_name) {
                        title_utf8 = g_locale_to_utf8(wm_name, -1, NULL, NULL, NULL);
                } else {
                        title_utf8 = NULL;
                }
        }

        g_free(wm_name);
        g_free(net_wm_name);

        return title_utf8;
}/*}}}*/

static gchar *get_property (Display *disp, Window win, /*{{{*/
                            Atom xa_prop_type, gchar *prop_name, unsigned long *size)
{
        Atom xa_prop_name;
        Atom xa_ret_type;
        int ret_format;
        unsigned long ret_nitems;
        unsigned long ret_bytes_after;
        unsigned long tmp_size;
        unsigned char *ret_prop;
        gchar *ret;

        xa_prop_name = XInternAtom(disp, prop_name, False);

        if (XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False,
                               xa_prop_type, &xa_ret_type, &ret_format,
                               &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
                p_verbose("Cannot get %s property.\n", prop_name);
                return NULL;
        }

        if (xa_ret_type != xa_prop_type) {
                p_verbose("Invalid type of %s property.\n", prop_name);
                XFree(ret_prop);
                return NULL;
        }

        /* null terminate the result to make string handling easier */
        tmp_size = (ret_format / 8) * ret_nitems;
        ret = g_malloc(tmp_size + 1);
        memcpy(ret, ret_prop, tmp_size);
        ret[tmp_size] = '\0';

        if (size) {
                *size = tmp_size;
        }

        XFree(ret_prop);
        return ret;
}/*}}}*/

static gchar *get_window_class (Display *disp, Window win)
{/*{{{*/
        gchar *class_utf8;
        gchar *wm_class;
        unsigned long size;

        wm_class = get_property(disp, win, XA_STRING, "WM_CLASS", &size);
        if (wm_class) {
                gchar *p_0 = strchr(wm_class, '\0');
                if (wm_class + size - 1 > p_0) {
                        *(p_0) = '.';
                }
                class_utf8 = g_locale_to_utf8(wm_class, -1, NULL, NULL, NULL);
        } else {
                class_utf8 = NULL;
        }

        g_free(wm_class);

        return class_utf8;
}/*}}}*/

