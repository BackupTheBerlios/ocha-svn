#include "querywin.h"
#include <gtk/gtk.h>

int main(int argc, char *argv[])
{
   gtk_init(&argc, &argv);

   struct querywin querywin;
   querywin_create(&querywin);

   gtk_widget_show(querywin.querywin);

   gtk_main();

   return 0;
}
