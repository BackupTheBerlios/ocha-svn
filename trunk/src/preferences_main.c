/** \file Make a connection to the catalog and open a catalog window */

#include "ocha_init.h"
#include "preferences_catalog.h"
#include "catalog.h"
#include "gtk/gtk.h"
#include <stdio.h>
#include <stdlib.h>

/* ------------------------- prototypes */
static void destroy_cb(GtkWidget *widget, gpointer userdata);
static void create_window(struct catalog *catalog);

/* ------------------------- main */
int main(int argc, char *argv[])
{
        GError *err = NULL;
        struct configuration config;
        struct catalog *catalog;

        ocha_init(argc, argv, TRUE/*GUI*/, &config);
        ocha_init_requires_catalog(config.catalog_path);

        catalog =  catalog_connect(config.catalog_path, &err);
        if(!catalog) {
                printf("error: invalid catalog at %s: %s\n",
                       config.catalog_path,
                       err->message);
                exit(12);
        }

        create_window(catalog);

        gtk_main();

        catalog_disconnect(catalog);
        return 0;
}

/* ------------------------- static functions */

static void destroy_cb(GtkWidget *widget, gpointer userdata)
{
        gtk_main_quit();
}

static void create_window(struct catalog *catalog)
{
        GtkWidget *window;
        GtkWidget *vbox1;
        struct preferences_catalog *prefs;
        GtkWidget *catalog_widget;

        window =  gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), "Ocha Preferences");

        vbox1 =  gtk_vbox_new(FALSE, 0);
        gtk_widget_show(vbox1);
        gtk_container_add(GTK_CONTAINER(window), vbox1);

        prefs = preferences_catalog_new(catalog);
        catalog_widget =  preferences_catalog_get_widget(prefs);
        gtk_widget_show(catalog_widget);
        gtk_box_pack_start(GTK_BOX(vbox1), catalog_widget, TRUE, TRUE, 0);

        gtk_window_set_default_size(GTK_WINDOW(window),
                                    250, 300);
        g_signal_connect (G_OBJECT (window), "destroy",
                          G_CALLBACK (destroy_cb),
                          NULL);

        gtk_widget_show(window);
}

