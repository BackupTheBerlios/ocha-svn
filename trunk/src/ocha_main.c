#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include "querywin.h"
#include "result_queue.h"
#include "queryrunner.h"
#include "catalog.h"
#include "catalog_queryrunner.h"
#include "query.h"
#include "resultlist.h"
#include "ocha_init.h"
#include "ocha_gconf.h"
#include "schedule.h"
#include "restart.h"
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
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

struct keygrab_data
{
        GdkModifierType modifiers;
        KeyCode keycode;
        guint keyval;
};

/* ------------------------- prototypes */
static gboolean configure_keygrab(struct keygrab_data *data);
static void reconfigure_keygrab(GConfClient *caller, guint id, GConfEntry *entry, gpointer userdata);
static GdkFilterReturn filter_keygrab (GdkXEvent *xevent, GdkEvent *gdk_event_unused, gpointer data);
static void install_keygrab_filters(struct keygrab_data *data);
static gboolean install_keygrab(const char *accelerator, struct keygrab_data *data);
static void uninstall_keygrab(void);
static void already_running(void);

/* ------------------------- main */

int main(int argc, char *argv[])
{
        struct configuration config;
        int i;
        const char *catalog_path;
        struct result_queue *queue;
        struct queryrunner *runner;
        struct keygrab_data keygrab_data;

        for(i=1; i<argc; i++) {
                const char *arg = argv[i];
                if(strcmp("--kill", arg)==0) {
                        return ocha_init_kill() ? 0:99;
                }
        }

        ocha_init(PACKAGE, argc, argv, TRUE/*has gui*/, &config);
        if(ocha_init_ocha_is_running()) {
                already_running();
                return 89;
        }
        if(!ocha_init_create_socket()) {
                return 71;
        }

        restart_register(argv[0]);
        ocha_init_requires_catalog(config.catalog_path);


        chdir(g_get_home_dir());

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

        schedule_init(&config);

        querywin_init();

        catalog_path = config.catalog_path;

        queue = querywin_get_result_queue();
        runner=catalog_queryrunner_new(catalog_path, queue);

        querywin_set_queryrunner(runner);

        if(!configure_keygrab(&keygrab_data)) {
                return 10;
        }


        gtk_main();

        return 0;
}

/* ------------------------- static functions */

static gboolean configure_keygrab(struct keygrab_data *data)
{
        GError *err = NULL;
        gchar *accelerator;
        gboolean success;

        accelerator = gconf_client_get_string(ocha_gconf_get_client(),
                                              OCHA_GCONF_ACCELERATOR_KEY,
                                              &err);
        if(err) {
                fprintf(stderr,
                        "error: error accessing gconf configuration : %s",
                        err->message);
                g_error_free(err);
                return FALSE;
        }

        success=install_keygrab(accelerator ? accelerator:OCHA_GCONF_ACCELERATOR_KEY_DEFAULT,
                                data);
        g_free(accelerator);

        if(!success) {
                return FALSE;
        }
        install_keygrab_filters(data);

        gconf_client_notify_add(ocha_gconf_get_client(),
                                OCHA_GCONF_ACCELERATOR_KEY,
                                reconfigure_keygrab,
                                data,
                                NULL/*free_func*/,
                                &err);
        if(err) {
                fprintf(stderr,
                        "warning: gconf failed: '%s'\n"
                        "         ocha will have to be restarted for configuration changes to be taken into account\n",
                        err->message);
                g_error_free(err);
        }

        return TRUE;
}
static void reconfigure_keygrab(GConfClient *caller, guint id, GConfEntry *entry, gpointer userdata)
{
        const char *accelerator = gconf_value_get_string(gconf_entry_get_value(entry));
        struct keygrab_data *data = (struct keygrab_data *)userdata;
        uninstall_keygrab();
        if(!install_keygrab(accelerator, data)) {
                gtk_main_quit();
        }
}

static GdkFilterReturn filter_keygrab (GdkXEvent *xevent, GdkEvent *gdk_event_unused, gpointer _data)
{
        XEvent *xev;
        XKeyEvent *key;
        struct keygrab_data *data;

        g_return_val_if_fail(_data, GDK_FILTER_CONTINUE);

        data = (struct keygrab_data *)_data;
        xev = (XEvent *) xevent;

        if (xev->type == KeyPress || xev->type == KeyRelease) {
                key = (XKeyEvent *) xevent;
                if (key->keycode == data->keycode
                    && (key->state&data->modifiers)==(data->modifiers) ) {
                        if (xev->type == KeyPress) {
                                querywin_start();
                        }
                        return GDK_FILTER_REMOVE;
                }
        }
        return GDK_FILTER_CONTINUE;
}

static gboolean install_keygrab(const char *accelerator, struct keygrab_data *data)
{
        GdkDisplay *display = gdk_display_get_default();
        int i;

        g_return_val_if_fail(accelerator, FALSE);
        g_return_val_if_fail(data, FALSE);

        gtk_accelerator_parse(accelerator,
                              &data->keyval,
                              &data->modifiers);
        if(data->keyval==0) {
                fprintf(stderr,
                        "error: invalid accelerator: '%s'\n",
                        accelerator);
                return FALSE;
        }
        data->keycode=XKeysymToKeycode(GDK_DISPLAY(), data->keyval);
        for (i = 0; i < gdk_display_get_n_screens (display); i++) {
                GdkScreen *screen;
                GdkWindow *root;

                screen = gdk_display_get_screen (display, i);
                if (!screen) {
                        continue;
                }
                root = gdk_screen_get_root_window (screen);
                XGrabKey(GDK_DISPLAY(),
                         data->keycode,
                         data->modifiers,
                         GDK_WINDOW_XID(root),
                         True,
                         GrabModeAsync,
                         GrabModeAsync);
        }
        printf("ocha: Type %s to open the seach window.\n",
               accelerator);
        return TRUE;
}

static void install_keygrab_filters(struct keygrab_data *data)
{
        GdkDisplay *display = gdk_display_get_default();
        int i;

        for (i = 0; i < gdk_display_get_n_screens (display); i++) {
                GdkScreen *screen;
                GdkWindow *root;

                screen = gdk_display_get_screen (display, i);
                if (!screen) {
                        continue;
                }
                root = gdk_screen_get_root_window (screen);
                gdk_window_add_filter(root,
                                      filter_keygrab,
                                      data/*userdata*/);
        }
}

static void uninstall_keygrab(void)
{
        GdkDisplay *display = gdk_display_get_default();
        int i;

        for (i = 0; i < gdk_display_get_n_screens (display); i++) {
                GdkScreen *screen;
                GdkWindow *root;

                screen = gdk_display_get_screen (display, i);
                if (!screen) {
                        continue;
                }
                root = gdk_screen_get_root_window (screen);
                XUngrabKey(GDK_DISPLAY(),
                           AnyKey,
                           AnyModifier,
                           GDK_WINDOW_XID(root));
        }
}

static void already_running(void)
{
        GtkWidget *dialog;
        GtkWidget *close;
        GtkWidget *prefs;
        gchar *accelerator;
        GError *err = NULL;
        gint response;
        static char *preferences_cmd = BINDIR "/ocha_preferences";

        accelerator = gconf_client_get_string(ocha_gconf_get_client(),
                                              OCHA_GCONF_ACCELERATOR_KEY,
                                              &err);
        if(accelerator==NULL) {
                accelerator=OCHA_GCONF_ACCELERATOR_KEY_DEFAULT;
                g_error_free(err);
        }

        dialog = gtk_message_dialog_new(NULL,
                                        0,
                                        GTK_MESSAGE_INFO,
                                        GTK_BUTTONS_NONE,
                                        "Ocha is already active on your system. To start "
                                        "a new search, hit %s.\n\n"
                                        "Alternatively, you can open Ocha's preference panel "
                                        "or stop Ocha",
                                        accelerator);
        prefs=gtk_dialog_add_button(GTK_DIALOG(dialog),
                              "Open Preferences",
                              1);
        gtk_button_set_image(GTK_BUTTON(prefs),
                             gtk_image_new_from_stock(GTK_STOCK_PREFERENCES,
                                                      GTK_ICON_SIZE_BUTTON));
        gtk_dialog_add_button(GTK_DIALOG(dialog),
                              "Stop Ocha",
                              2);

        close=gtk_dialog_add_button(GTK_DIALOG(dialog),
                              "Close",
                              3);
        gtk_button_set_image(GTK_BUTTON(close),
                             gtk_image_new_from_stock(GTK_STOCK_CLOSE,
                                                      GTK_ICON_SIZE_BUTTON));

        gtk_dialog_set_default_response(GTK_DIALOG(dialog), 3);

        response=gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        switch(response) {
        case 1: /* preferences */
                gnome_execute_async(NULL/*current dir*/,
                                    1/*argc*/,
                                    &preferences_cmd);
                break;

        case 2: /* kill ocha */
                ocha_init_kill();
                gtk_main();
                break;
        }

}
