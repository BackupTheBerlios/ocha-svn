/** \file Make a connection to the catalog and open a catalog window */

#include "ocha_init.h"
#include "preferences_catalog.h"
#include "preferences_general.h"
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
        GtkWidget *rootvbox;
        struct preferences_catalog *prefs_catalog;
        GtkWidget *catalog_widget;
        GtkWidget *notebook;
        GtkWidget *buttonbox;
        GtkWidget *close;
        GtkWidget *general_widget;
        GtkWidget *label;

        window =  gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), "Ocha Preferences");

        rootvbox =  gtk_vbox_new(FALSE, 0);
        gtk_widget_show(rootvbox);
        gtk_container_add(GTK_CONTAINER(window), rootvbox);

        notebook = gtk_notebook_new();
        gtk_widget_show (notebook);
        gtk_box_pack_start (GTK_BOX (rootvbox), notebook, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (notebook), 12);

        buttonbox = gtk_hbutton_box_new ();
        gtk_widget_show (buttonbox);
        gtk_box_pack_start (GTK_BOX (rootvbox), buttonbox, FALSE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (buttonbox), 12);
        gtk_button_box_set_layout (GTK_BUTTON_BOX (buttonbox), GTK_BUTTONBOX_END);

        close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
        gtk_widget_show (close);
        gtk_container_add (GTK_CONTAINER (buttonbox), close);
        GTK_WIDGET_SET_FLAGS (close, GTK_CAN_DEFAULT);

        /* notebook page : general */
        general_widget = preferences_general_widget_new();
        gtk_widget_show(general_widget);
        gtk_container_add(GTK_CONTAINER(notebook), general_widget);

        label = gtk_label_new ("General");
        gtk_widget_show (label);
        gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
                                    gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0),
                                    label);

        /* notebook page : catalog */

        prefs_catalog = preferences_catalog_new(catalog);
        catalog_widget =  preferences_catalog_get_widget(prefs_catalog);
        gtk_widget_show(catalog_widget);
        gtk_container_add(GTK_CONTAINER(notebook), catalog_widget);

        label = gtk_label_new ("Catalog");
        gtk_widget_show (label);
        gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
                                    gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1),
                                    label);


        gtk_window_set_default_size(GTK_WINDOW(window),
                                    250, 300);
        g_signal_connect (G_OBJECT (window), "destroy",
                          G_CALLBACK (destroy_cb),
                          NULL);
        g_signal_connect (G_OBJECT (close), "clicked",
                          G_CALLBACK (destroy_cb),
                          NULL);

        gtk_widget_show(window);
}

