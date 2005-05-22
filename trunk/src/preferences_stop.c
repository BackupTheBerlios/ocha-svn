#include "preferences_stop.h"
#include "ocha_init.h"
#include <gtk/gtk.h>

/** \file Implement the API defined in preferences_stop.h
 *
 */

/* ------------------------- prototypes: static functions */
static void stop_cb(GtkButton *stop, gpointer userdata);

/* ------------------------- definitions */

/* ------------------------- public functions */
GtkWidget *preferences_stop_create(void)
{
        GtkWidget *align;
        GtkWidget *stop;
        GtkWidget *image;

        align = gtk_alignment_new(0.5/*xalign*/,
                                  0.5/*yalign*/,
                                  0/*xscale*/,
                                  0/*yscale*/);
        gtk_widget_show(align);

        stop = gtk_button_new_with_label("Stop Ocha Daemon");
        gtk_widget_show(stop);

        image = gtk_image_new_from_stock(GTK_STOCK_STOP,
                                         GTK_ICON_SIZE_BUTTON);
        gtk_widget_show(image);
        gtk_button_set_image(GTK_BUTTON(stop), image);

        gtk_container_add(GTK_CONTAINER(align), stop);

        g_signal_connect(stop,
                         "clicked",
                         G_CALLBACK(stop_cb),
                         NULL);

        return align;
}

/* ------------------------- static functions */

static void stop_cb(GtkButton *stop, gpointer userdata)
{
        GtkWidget *dialog;
        GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(stop));
        const char *msg;

        if(ocha_init_kill()) {
                msg = "Ocha Daemon stopped.\nThe Ocha service is not available anymore.";
        } else {
                msg = "No Ocha Daemon was found.\nThe Ocha service is not available.";
        }

        dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_INFO,
                                        GTK_BUTTONS_CLOSE,
                                        msg);

        g_signal_connect_swapped (dialog,
                                  "response",
                                  G_CALLBACK (gtk_widget_destroy),
                                  dialog);

        gtk_widget_show(dialog);
}
