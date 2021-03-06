#include "preferences_general.h"
#include "ocha_gconf.h"
#include "ocha_init.h"
#include "accel_button.h"
#include <gdk/gdkkeysyms.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <string.h>
#include <libgnome/libgnome.h>
/** \file Implementation of the API defined in preferences_general.h
 *
 */

/**
 * Label for possible values for update_catalog, as defined in ocha_gconf.h
 *
 * @see OchaGConfUpdateCatalog
 */
static const char *update_catalog_labels[OCHA_GCONF_UPDATE_CATALOG_COUNT] = {
        "Manually",
        "Every 10 minutes",
        "Every half hour",
        "Every hour",
        "Every day"
};

/* ------------------------- prototypes: static functions */
static void update_radio_button_init(GtkWidget **buttons);
static void update_radio_button_set_state(GtkWidget **buttons);
static void update_radio_button_cb(GtkToggleButton *buttons, gpointer userdata);
static void update_radio_button_notify_cb(GConfClient *client, guint id, GConfEntry *entry, gpointer userdata);
static void start_ocha_cb(GtkButton *button, gpointer userdata);
static void remove_notify(GtkWidget *widget, gpointer userdata);


/* ------------------------- definitions */

/* ------------------------- public functions */
GtkWidget *preferences_general_widget_new()
{
        GtkWidget *retval;
        GtkWidget *accel_frame;
        GtkWidget *sched_frame;
        GtkWidget *label;
        GtkWidget *accel_button;
        GtkWidget *accel_box;
        GtkWidget *align;
        GtkWidget *schedule_vbox;
        GtkWidget *schedule_items[OCHA_GCONF_UPDATE_CATALOG_COUNT];
        GSList *schedule_group;
        int i;

        retval = gtk_vbox_new(FALSE, 0);
        gtk_widget_show(retval);

        /* Top frame: accelerator */

        accel_frame = gtk_frame_new(NULL);
        gtk_widget_show(accel_frame);
        gtk_frame_set_shadow_type(GTK_FRAME(accel_frame), GTK_SHADOW_NONE);
        gtk_container_set_border_width(GTK_CONTAINER(accel_frame), 12);
        label = gtk_label_new("<b>Accelerator</b>");
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_widget_show(label);
        gtk_frame_set_label_widget(GTK_FRAME(accel_frame), label);

        gtk_widget_show(accel_frame);
        gtk_box_pack_start(GTK_BOX(retval),
                           accel_frame,
                           FALSE,
                           TRUE,
                           0);

        align =  gtk_alignment_new(0.5, 0.5, 1, 1);
        gtk_widget_show(align);
        gtk_container_add(GTK_CONTAINER(accel_frame), align);
        gtk_alignment_set_padding(GTK_ALIGNMENT(align),
                                  0, 0, 12, 0);


        accel_box = gtk_hbox_new(FALSE, 12);
        gtk_widget_show(accel_box);
        gtk_container_add(GTK_CONTAINER(align),
                          accel_box);

        label = gtk_label_new("Search Key: ");
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(accel_box),
                           label,
                           FALSE,
                           FALSE,
                           0);

        accel_button = accel_button_new();
        gtk_box_pack_start(GTK_BOX(accel_box),
                         accel_button,
                         FALSE,
                         FALSE,
                         0);

        label = gtk_label_new("");
        gtk_widget_show(label);
        gtk_box_pack_end(GTK_BOX(accel_box),
                         label,
                         TRUE,
                         TRUE,
                         0);


        /* Bottom frame: schedule */
        sched_frame = gtk_frame_new(NULL);
        gtk_widget_show(sched_frame);
        gtk_frame_set_shadow_type(GTK_FRAME(sched_frame), GTK_SHADOW_NONE);
        label = gtk_label_new("<b>Catalog Updates</b>");
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_widget_show(label);
        gtk_frame_set_label_widget(GTK_FRAME(sched_frame), label);


        schedule_vbox = gtk_vbox_new(TRUE/*homogeneous*/, 0/*spacing*/);
        gtk_container_set_border_width(GTK_CONTAINER(schedule_vbox), 12);
        gtk_widget_show(schedule_vbox);
        gtk_container_add(GTK_CONTAINER(sched_frame), schedule_vbox);


        align = gtk_alignment_new(0.0/*xalign*/, 0.0/*yalign*/, 0/*xscale*/, 0/*yscale*/);
        gtk_widget_show(align);
        gtk_box_pack_start(GTK_BOX(schedule_vbox),
                           align,
                           TRUE,
                           TRUE,
                           0);

        label = gtk_label_new("Update Catalog");
        gtk_widget_show(label);
        gtk_container_add(GTK_CONTAINER(align), label);


        schedule_group=NULL;
        for(i=0; i<OCHA_GCONF_UPDATE_CATALOG_COUNT; i++) {
                if(update_catalog_labels[i]==NULL) {
                        g_warning("catalog update type %d has no label "
                                  "(incomplete variable update_catalog_labels in prefences_general.c)",
                                  i);
                        continue;
                }
                schedule_items[i] = gtk_radio_button_new_with_label(schedule_group, update_catalog_labels[i]);
                gtk_widget_show(schedule_items[i]);
                gtk_container_add(GTK_CONTAINER(schedule_vbox), schedule_items[i]);
                schedule_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(schedule_items[i]));

        }
        update_radio_button_init(schedule_items);

        align =  gtk_alignment_new(0.5, 0.5, 1, 1);
        gtk_widget_show(align);
        gtk_alignment_set_padding(GTK_ALIGNMENT(align),
                                  0/*top*/,
                                  0/*bottom*/,
                                  12/*left*/,
                                  12/*rigth*/);
        gtk_container_add(GTK_CONTAINER(align), sched_frame);

        gtk_box_pack_start(GTK_BOX(retval),
                           align,
                           FALSE,
                           TRUE,
                           0);

        if(!ocha_init_ocha_is_running()) {
                GtkWidget *line;
                GtkWidget *label;
                GtkWidget *image;
                GtkWidget *href;

                line = gtk_hbox_new(FALSE/*not homogenous*/,
                                    0/*spacing*/);
                gtk_widget_show(line);

                image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING,
                                                 GTK_ICON_SIZE_MENU);
                gtk_widget_show(image);
                gtk_box_pack_start(GTK_BOX(line),
                                   image,
                                   FALSE/*expand*/,
                                   FALSE/*fill*/,
                                   6/*padding*/);
                label = gtk_label_new("<span foreground='red'>Ocha is currently disabled.</span>");
                gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
                gtk_widget_show(label);
                gtk_box_pack_start(GTK_BOX(line),
                                   label,
                                   FALSE/*expand*/,
                                   FALSE/*fill*/,
                                   0/*padding*/);

                label = gtk_label_new("<span foreground='red'><span underline='single'>Start it</span> ?</span>");
                gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
                gtk_widget_show(label);

                href = gtk_button_new();
                gtk_button_set_relief(GTK_BUTTON(href),
                                      GTK_RELIEF_NONE);
                gtk_container_add(GTK_CONTAINER(href),
                                  label);
                gtk_widget_show(href);
                gtk_box_pack_start(GTK_BOX(line),
                                   href,
                                   FALSE/*expand*/,
                                   FALSE/*fill*/,
                                   0/*padding*/);


                gtk_widget_show(href);


                align =  gtk_alignment_new(0.5, 0.5, 1, 1);
                gtk_widget_show(align);
                gtk_alignment_set_padding(GTK_ALIGNMENT(align),
                                          0/*top*/,
                                          12/*bottom*/,
                                          12/*left*/,
                                          12/*rigth*/);
                gtk_container_add(GTK_CONTAINER(align), line);

                gtk_box_pack_end(GTK_BOX(retval),
                                 align,
                                 TRUE/*expand*/,
                                 FALSE/*fill*/,
                                 0/*padding*/);

                g_signal_connect(href,
                                 "clicked",
                                 G_CALLBACK(start_ocha_cb),
                                 align/*userdata*/);
        }

        return retval;
}

/* ------------------------- static functions */

static void update_radio_button_init(GtkWidget **buttons)
{
        GtkWidget **buttons_copy;
        guint notify_id;
        int i;

        g_return_if_fail(buttons!=NULL);

        update_radio_button_set_state(buttons);
        for(i=0; i<OCHA_GCONF_UPDATE_CATALOG_COUNT; i++) {
                g_signal_connect(buttons[i],
                                 "toggled",
                                 G_CALLBACK(update_radio_button_cb),
                                 GINT_TO_POINTER(i));
        }

        buttons_copy = g_new(GtkWidget *, OCHA_GCONF_UPDATE_CATALOG_COUNT);
        memcpy(buttons_copy, buttons, sizeof(GtkWidget *)*OCHA_GCONF_UPDATE_CATALOG_COUNT);

        notify_id=gconf_client_notify_add(ocha_gconf_get_client(),
                                          OCHA_GCONF_UPDATE_CATALOG_KEY,
                                          update_radio_button_notify_cb,
                                          buttons_copy,
                                          g_free/*free_func for buttons_copy*/,
                                          NULL/*err*/);
        g_signal_connect(buttons[0],
                         "destroy",
                         G_CALLBACK(remove_notify),
                         GUINT_TO_POINTER(notify_id));

}
static void update_radio_button_cb(GtkToggleButton *button, gpointer userdata)
{
        gboolean active = gtk_toggle_button_get_active(button);
        int value = GPOINTER_TO_INT(userdata);
        if(active) {
                gconf_client_set_int(ocha_gconf_get_client(),
                                     OCHA_GCONF_UPDATE_CATALOG_KEY,
                                     value,
                                     NULL/*ignore errors*/);
        }
}
static void update_radio_button_set_state(GtkWidget **buttons)
{
        int current;
        int i;

        g_return_if_fail(buttons!=NULL);

        current = gconf_client_get_int(ocha_gconf_get_client(),
                                       OCHA_GCONF_UPDATE_CATALOG_KEY,
                                       NULL/*ignore errors*/);
        if(current<0 || current>=OCHA_GCONF_UPDATE_CATALOG_COUNT) {
                g_warning("invalid value for key "
                          OCHA_GCONF_UPDATE_CATALOG_KEY
                          ": %d. valid range: 0<=value<%d. Value ignored, using default (%s)",
                          current,
                          OCHA_GCONF_UPDATE_CATALOG_COUNT,
                          update_catalog_labels[OCHA_GCONF_UPDATE_CATALOG_DEFAULT]);
                current=OCHA_GCONF_UPDATE_CATALOG_DEFAULT;
        }

        for(i=0; i<OCHA_GCONF_UPDATE_CATALOG_COUNT; i++) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(buttons[i]),
                                             current==i);
        }

}

static void update_radio_button_notify_cb(GConfClient *client, guint id, GConfEntry *entry, gpointer userdata)
{
        GtkWidget **buttons;

        g_return_if_fail(userdata);

        buttons = (GtkWidget **)userdata;
        update_radio_button_init(buttons);
}

static void start_ocha_cb(GtkButton *button, gpointer userdata)
{
        GtkWidget *hideme = GTK_WIDGET(userdata);
        int pid;

        pid=gnome_execute_async(NULL/*current dir*/,
                                ocha_init_daemon_argc,
                                ocha_init_daemon_argv);
        if(pid==-1) {
                GtkWidget *dialog;

                dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(hideme)),
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_CLOSE,
                                                "Failed to launch Ocha\n%s: %s",
                                                ocha_init_daemon_argv[0],
                                                strerror(errno));
                g_signal_connect_swapped (dialog,
                                          "response",
                                          G_CALLBACK (gtk_widget_destroy),
                                          dialog);
                gtk_widget_show(dialog);
        } else {
                gtk_widget_hide(hideme);
        }
}

static void remove_notify(GtkWidget *widget, gpointer userdata)
{
        guint id = GPOINTER_TO_UINT(userdata);

        gconf_client_notify_remove(ocha_gconf_get_client(),
                                   id);
}
