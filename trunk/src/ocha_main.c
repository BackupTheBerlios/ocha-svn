#include "config.h"

#include <signal.h>
#include "querywin.h"
#include "result_queue.h"
#include "queryrunner.h"
#include "catalog.h"
#include "catalog_queryrunner.h"
#include "netwm_queryrunner.h"
#include "compound_queryrunner.h"
#include "query.h"
#include "resultlist.h"
#include "ocha_init.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libgnome/gnome-init.h>
#include <libgnome/gnome-program.h>

/* ------------------------- prototypes */
static GdkFilterReturn filter_keygrab (GdkXEvent *xevent, GdkEvent *gdk_event_unused, gpointer data);
static void install_keygrab(void);

/* ------------------------- main */

int main(int argc, char *argv[])
{
        struct configuration config;
        int i;
        const char *catalog_path;
        struct result_queue *queue;
        struct queryrunner *runners[3];
        struct queryrunner *compound;

        ocha_init(argc, argv, TRUE/*has gui*/, &config);
        ocha_init_requires_catalog(config.catalog_path);

        for(i=1; i<argc; i++) {
                const char *arg = argv[i];
                if(strncmp("--pid=", arg, strlen("--pid="))==0) {
                        const char *file=&arg[strlen("--pid=")];
                        pid_t pid = getpid();
                        FILE* pidh = fopen(file, "w");
                        if(pidh==NULL) {
                                fprintf(stderr, "error: cannot open pid file %s\n", file);
                                exit(14);
                        }
                        fprintf(pidh, "%d\n", pid);
                        fclose(pidh);
                }
        }

        querywin_init();

        catalog_path = config.catalog_path;

        queue = querywin_get_result_queue();
        runners[0]=netwm_queryrunner_create(GDK_DISPLAY(), queue);
        runners[1]=catalog_queryrunner_new(catalog_path, queue);
        runners[2]=NULL;

        compound = compound_queryrunner_new(runners, 2);
        querywin_set_queryrunner(compound);
        querywin_start();

        install_keygrab();

        gtk_main();

        return 0;
}

/* ------------------------- static functions */

static GdkFilterReturn filter_keygrab (GdkXEvent *xevent, GdkEvent *gdk_event_unused, gpointer data)
{
        int keycode;
        XEvent *xev;
        XKeyEvent *key;

        keycode = GPOINTER_TO_INT(data);
        xev = (XEvent *) xevent;
        if (xev->type != KeyPress)
                return GDK_FILTER_CONTINUE;

        key = (XKeyEvent *) xevent;
        if (keycode == key->keycode)
                querywin_start();
        return GDK_FILTER_CONTINUE;
}

static void install_keygrab()
{
        GdkDisplay *display = gdk_display_get_default();
        int keycode = XKeysymToKeycode(GDK_DISPLAY(), XK_space);
        int i;
        for (i = 0; i < gdk_display_get_n_screens (display); i++) {
                GdkScreen *screen;
                GdkWindow *root;

                screen = gdk_display_get_screen (display, i);
                if (!screen) {
                        continue;
                }
                root = gdk_screen_get_root_window (screen);
                XGrabKey(GDK_DISPLAY(), keycode,
                         Mod1Mask,
                         GDK_WINDOW_XID(root),
                         True,
                         GrabModeAsync,
                         GrabModeAsync);
                gdk_window_add_filter(root, filter_keygrab, GINT_TO_POINTER(keycode)/*userdata*/);
        }
}
