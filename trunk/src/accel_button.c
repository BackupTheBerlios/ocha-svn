#include "accel_button.h"
#include "ocha_gconf.h"
#include "ocha_init.h"
#include <gdk/gdkkeysyms.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <string.h>

/** \file a button used to display and modify ocha's accelerator
 *
 */

/* ------------------------- prototypes: member functions () */

/* ------------------------- prototypes: static functions */
static void accel_button_init(GtkWidget *button);
static void accel_button_set_label(GtkButton *button);
static void accel_button_cb(GtkButton *button, gpointer userdata);
static gboolean accel_button_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer userdata);
static void accel_button_update_cb(GConfClient *client, guint id, GConfEntry *entry, gpointer userdata);
static void remove_notify(GtkWidget *widget, gpointer userdata);
static gboolean accel_button_is_recording(GtkButton *button);

/* ------------------------- definitions */

/* ------------------------- public functions */
GtkWidget *accel_button_new(void)
{
        GtkWidget *accel_button;

        accel_button = gtk_button_new_with_label("accel");
        gtk_widget_show(accel_button);
        accel_button_init(accel_button);

        return accel_button;
}

/* ------------------------- static functions */

static void accel_button_init(GtkWidget *button)
{
        guint notify_id;

        accel_button_set_label(GTK_BUTTON(button));

        g_signal_connect(button,
                         "clicked",
                         G_CALLBACK(accel_button_cb),
                         NULL/*userdata*/);
        g_signal_connect(button,
                         "key-release-event",
                         G_CALLBACK(accel_button_key_cb),
                         NULL/*userdata*/);
        g_signal_connect(button,
                         "key-press-event",
                         G_CALLBACK(accel_button_key_cb),
                         NULL/*userdata*/);

        notify_id=gconf_client_notify_add(ocha_gconf_get_client(),
                                          OCHA_GCONF_ACCELERATOR_KEY,
                                          accel_button_update_cb,
                                          button,
                                          NULL/*free_func*/,
                                          NULL/*err*/);
        g_signal_connect(button,
                         "destroy",
                         G_CALLBACK(remove_notify),
                         GUINT_TO_POINTER(notify_id));
}
static void accel_button_set_label(GtkButton *button)
{
        char *accelerator;

        accelerator = gconf_client_get_string(ocha_gconf_get_client(),
                                              OCHA_GCONF_ACCELERATOR_KEY,
                                              NULL/*err*/);
        gtk_button_set_label(button,
                             accelerator ? accelerator:OCHA_GCONF_ACCELERATOR_KEY_DEFAULT);
}


static void accel_button_cb(GtkButton *button, gpointer userdata)
{
        if(accel_button_is_recording(button)) {
                accel_button_set_label(button);
        } else {
                gtk_button_set_label(button,
                                     "Type new accelerator...");
                gtk_widget_grab_focus(GTK_WIDGET(button));
        }
}

static gboolean accel_button_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer userdata)
{
        char *accelerator;

        if(!accel_button_is_recording(GTK_BUTTON(widget))) {
                return FALSE;
        }
        if(event->type==GDK_KEY_RELEASE) {
                if(event->keyval==GDK_BackSpace || event->keyval==GDK_Escape || event->keyval==GDK_Delete) {
                        accel_button_set_label(GTK_BUTTON(widget));
                } else {
                        if(!IsModifierKey(event->keyval)) {
                                accelerator= gtk_accelerator_name(event->keyval, event->state);

                                gconf_client_set_string(ocha_gconf_get_client(),
                                                        OCHA_GCONF_ACCELERATOR_KEY,
                                                        accelerator,
                                                        NULL/*err*/);
                                g_free(accelerator);
                        }
                }
        }
        return TRUE;
 }

/**
 * Called by gconf when the accelerator key has changed.
 * @param client
 * @param id
 * @param entry
 * @param userdata the GtkButton
 */
static void accel_button_update_cb(GConfClient *client, guint id, GConfEntry *entry, gpointer userdata)
{
        GtkButton *button;
        const char *accelerator;

        g_return_if_fail(GTK_IS_BUTTON(userdata));
        button = GTK_BUTTON(userdata);

        accelerator = gconf_value_get_string(gconf_entry_get_value(entry));
        gtk_button_set_label(GTK_BUTTON(button), accelerator);
}


/**
 * Remove the notification for the accelerator key when
 * the button is destroyed.
 * @param widget
 * @param userdata ID returned by gconf_client_notify_add()
 */
static void remove_notify(GtkWidget *widget, gpointer userdata)
{
        guint id = GPOINTER_TO_UINT(userdata);

        gconf_client_notify_remove(ocha_gconf_get_client(),
                                   id);
}

static gboolean accel_button_is_recording(GtkButton *button)
{
        const char *label;

        label = gtk_button_get_label(button);
        return label && g_str_has_suffix(label, "...");
}

