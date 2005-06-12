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
#include "accel_button.h"
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

static GtkWidget *new_key_ui_dialog;
static GtkWidget *new_key_ui_dialog_close;

/* ------------------------- prototypes */
static void configure_keygrab(struct keygrab_data *data);
static void reconfigure_keygrab(GConfClient *caller, guint id, GConfEntry *entry, gpointer userdata);
static GdkFilterReturn filter_keygrab (GdkXEvent *xevent, GdkEvent *gdk_event_unused, gpointer data);
static void install_keygrab_filters(struct keygrab_data *data);
static gboolean install_keygrab(const char *accelerator, struct keygrab_data *data);
static void uninstall_keygrab(void);
static void already_running(void);
static void launch_get_new_key_ui(struct keygrab_data *data);
static void launch_get_new_key_ui_accept(void);
static void launch_get_new_key_ui_cb(GtkDialog *dialog, int arg, gpointer userdata);

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

        ocha_init("ochad", argc, argv, TRUE/*has gui*/, &config);
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

        configure_keygrab(&keygrab_data);

        gtk_main();

        return 0;
}

/* ------------------------- static functions */

static void configure_keygrab(struct keygrab_data *data)
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
        }

        success=install_keygrab(accelerator ? accelerator:OCHA_GCONF_ACCELERATOR_KEY_DEFAULT,
                                data);
        g_free(accelerator);
        if(!success) {
                launch_get_new_key_ui(data);
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
}
static void reconfigure_keygrab(GConfClient *caller, guint id, GConfEntry *entry, gpointer userdata)
{
        const char *accelerator;
        struct keygrab_data *data = (struct keygrab_data *)userdata;

        uninstall_keygrab();

        accelerator = gconf_value_get_string(gconf_entry_get_value(entry));
        if(!install_keygrab(accelerator, data)) {
                launch_get_new_key_ui(data);
        } else {
                launch_get_new_key_ui_accept();
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
        int xerr;

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

        printf("%s:%d: gdk_error_trap_push\n", /*@nocommit@*/
               __FILE__,
               __LINE__
               );

        gdk_error_trap_push();

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
        gdk_flush();
        xerr=gdk_error_trap_pop();
        printf("%s:%d: gdk_error_trap_pop\n", /*@nocommit@*/
               __FILE__,
               __LINE__
               );

        if(xerr!=0) {
                return FALSE;
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

        accelerator = gconf_client_get_string(ocha_gconf_get_client(),
                                              OCHA_GCONF_ACCELERATOR_KEY,
                                              &err);
        if(accelerator==NULL) {
                accelerator=OCHA_GCONF_ACCELERATOR_KEY_DEFAULT;
                g_error_free(err);
        }

        dialog = gtk_message_dialog_new(NULL,
                                        0/*flags*/,
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
                                    ocha_init_preferences_argc,
                                    ocha_init_preferences_argv);
                break;

        case 2: /* kill ocha */
                ocha_init_kill();
                gtk_main();
                break;
        }

}

/**
 * Open a dialog that will ask the user
 * for a new shortcut and then set it using gconf.
 *
 * gconf notification will ensure that the new setting
 * is used.
 */
static void launch_get_new_key_ui(struct keygrab_data *data)
{
        if(new_key_ui_dialog==NULL) {
                GtkWidget *close;
                GtkWidget *quit;
                GtkWidget *choose;
                GtkWidget *vbox;
                GtkWidget *hbox;
                GtkWidget *label;

                new_key_ui_dialog = gtk_message_dialog_new(NULL,
                                                0/*flags*/,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_NONE,
                                                "The accelerator Ocha was configured to respond to "
                                                "is being used by another program.\n\n"
                                                "Please choose another accelerator for the Ocha search window."
                                                "Alternatively, you can click on 'Quit' to stop the Ocha daemon." );

                quit=gtk_dialog_add_button(GTK_DIALOG(new_key_ui_dialog),
                                           "Quit",
                                           2);
                gtk_button_set_image(GTK_BUTTON(quit),
                                     gtk_image_new_from_stock(GTK_STOCK_QUIT,
                                                              GTK_ICON_SIZE_BUTTON));

                gtk_dialog_set_default_response(GTK_DIALOG(new_key_ui_dialog), 2);
                close=gtk_dialog_add_button(GTK_DIALOG(new_key_ui_dialog),
                                           "Close",
                                           0);
                gtk_button_set_image(GTK_BUTTON(close),
                                     gtk_image_new_from_stock(GTK_STOCK_CLOSE,
                                                              GTK_ICON_SIZE_BUTTON));

                gtk_dialog_set_default_response(GTK_DIALOG(new_key_ui_dialog), 0);
                new_key_ui_dialog_close=close;

                vbox=GTK_DIALOG(new_key_ui_dialog)->vbox;

                hbox=gtk_hbox_new(FALSE/*not homogenous*/, 12/*spacing*/);
                gtk_widget_show(hbox);
                gtk_box_pack_end(GTK_BOX(vbox),
                                 hbox,
                                 TRUE,
                                 TRUE,
                                 12);


                label=gtk_label_new("<b>New accelerator:</b>");
                gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
                gtk_widget_show(label);
                gtk_box_pack_start(GTK_BOX(hbox),
                                   label,
                                   FALSE,
                                   FALSE,
                                   12);

                choose = accel_button_new();
                gtk_box_pack_end(GTK_BOX(hbox),
                                   choose,
                                   TRUE,
                                   TRUE,
                                   12);

                g_signal_connect (new_key_ui_dialog,
                                  "response",
                                  G_CALLBACK (launch_get_new_key_ui_cb),
                                  new_key_ui_dialog/*userdata*/);

        }
        gtk_widget_set_sensitive(new_key_ui_dialog_close, FALSE);
        gtk_window_present(GTK_WINDOW(new_key_ui_dialog));
}

static void launch_get_new_key_ui_cb(GtkDialog *dialog, int arg, gpointer userdata)
{
        gtk_widget_hide(GTK_WIDGET(dialog));
        if(arg==2) {
                gtk_main_quit();
        }
}

static void launch_get_new_key_ui_accept(void)
{
        if(new_key_ui_dialog!=NULL) {
                gtk_widget_set_sensitive(new_key_ui_dialog_close, TRUE);
        }
}
