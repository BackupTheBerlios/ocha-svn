#include "config.h"

#include <signal.h>
#include "querywin.h"
#include "result_queue.h"
#include "queryrunner.h"
#include "catalog_queryrunner.h"
#include "netwm_queryrunner.h"
#include "compound_queryrunner.h"
#include "query.h"
#include "resultlist.h"
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

static struct result_queue *result_queue;

#define PROGRAM_NAME "ocha"
#define PROGRAM_VERSION "0.1"

static GdkFilterReturn filter_keygrab (GdkXEvent *xevent,
                                       GdkEvent *gdk_event_unused,
                                       gpointer data)
{
   int keycode = GPOINTER_TO_INT(data);
   XEvent *xev = (XEvent *) xevent;
   if (xev->type != KeyPress)
      return GDK_FILTER_CONTINUE;

   XKeyEvent *key = (XKeyEvent *) xevent;
   if (keycode == key->keycode)
      querywin_start();
   return GDK_FILTER_CONTINUE;
}

static void install_keygrab()
{
   GdkDisplay *display = gdk_display_get_default();
   int keycode = XKeysymToKeycode(GDK_DISPLAY(), XK_space);
   for (int i = 0; i < gdk_display_get_n_screens (display); i++)
      {
         GdkScreen *screen = gdk_display_get_screen (display, i);
         if (!screen)
            continue;
         GdkWindow *root = gdk_screen_get_root_window (screen);
         gdk_error_trap_push ();

         XGrabKey(GDK_DISPLAY(), keycode,
                  Mod1Mask,
                  GDK_WINDOW_XID(root),
                  True,
                  GrabModeAsync,
                  GrabModeAsync);
         gdk_flush ();
         if (gdk_error_trap_pop ())
            {
               fprintf (stderr, "Error grabbing key %d, %p\n", keycode, root);
               exit(88);
            }
         gdk_window_add_filter(root, filter_keygrab, GINT_TO_POINTER(keycode)/*userdata*/);
      }
}

int main(int argc, char *argv[])
{
   g_thread_init(NULL/*vtable*/);
   gnome_program_init (PROGRAM_NAME,
                       PROGRAM_VERSION,
                       LIBGNOME_MODULE,
                       argc,
                       argv,
                       NULL);
   gtk_init(&argc, &argv);

   for(int i=1; i<argc; i++)
      {
         const char *arg = argv[i];
         if(strncmp("--pid=", arg, strlen("--pid="))==0)
            {
               const char *file=&arg[strlen("--pid=")];
               pid_t pid = getpid();
               FILE* pidh = fopen(file, "w");
               if(pidh==NULL)
                  {
                     fprintf(stderr, "error: cannot open pid file %s\n", file);
                     exit(14);
                  }
               fprintf(pidh, "%d\n", pid);
               fclose(pidh);
            }
         else
            {
               fprintf(stderr, "error: unknown argument %s\n", arg);
               exit(15);
            }
      }


   querywin_init();

   GString *catalog_path = g_string_new(getenv("HOME"));
   g_string_append(catalog_path, "/.ocha");
   mkdir(catalog_path->str, 0600);
   g_string_append(catalog_path, "/catalog");

   struct result_queue *queue = querywin_get_result_queue();
   struct queryrunner *runners[] =
      {
         netwm_queryrunner_create(GDK_DISPLAY(),
                                  queue),
         catalog_queryrunner_new(catalog_path->str,
                                 queue),
      };
   struct queryrunner *compound = compound_queryrunner_new(runners, sizeof(runners)/sizeof(struct queryrunner *));
   querywin_set_queryrunner(compound);
   querywin_start();

   install_keygrab();

   gtk_main();

   return 0;
}

