/** \file Make a connection to the catalog and open a catalog window */

#include "ocha_init.h"
#include "preferences_catalog.h"
#include "catalog.h"
#include "gtk/gtk.h"
#include <stdio.h>
#include <stdlib.h>

static void create_window(struct catalog *catalog)
{
   GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(window), "Ocha Preferences");

   GtkWidget *vbox1 = gtk_vbox_new(FALSE, 0);
   gtk_widget_show(vbox1);
   gtk_container_add(GTK_CONTAINER(window), vbox1);

   GtkWidget *catalog_widget = preferences_catalog_new(catalog);
   gtk_widget_show(catalog_widget);
   gtk_box_pack_start(GTK_BOX(vbox1), catalog_widget, TRUE, TRUE, 0);

   gtk_widget_show(window);
}

int main(int argc, char *argv[])
{
   struct configuration config;
   ocha_init(argc, argv, TRUE/*GUI*/, &config);
   ocha_init_requires_catalog(config.catalog_path);

   GError *err = NULL;
   struct catalog *catalog = catalog_connect(config.catalog_path, &err);
   if(!catalog)
   {
       printf("error: invalid catalog at %s: %s\n",
              config.catalog_path,
              err->message);
       exit(12);
   }

   create_window(catalog);

   gtk_main();

   catalog_disconnect(catalog);
}
