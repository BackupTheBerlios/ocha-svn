#include "config.h"

#include <signal.h>
#include "querywin.h"
#include "result_queue.h"
#include "queryrunner.h"
#include "catalog_queryrunner.h"
#include "query.h"
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

#define QUERY_TIMEOUT 3000

static struct querywin win_data;
static struct result_queue *result_queue;
static struct queryrunner *queryrunner;
static GtkListStore *list_model;
static GString* query_str;
static GString* running_query;
static GList* results;
static struct result* selected;
static guint32 last_keypress;
#define query_label_text_len 256
static char query_label_text[256];
bool shown;
static void querywin_start()
{
   if(shown)
      return;
   printf("show\n");
   last_keypress=0;
   queryrunner->start(queryrunner);
   gtk_window_reshow_with_initial_size(GTK_WINDOW(win_data.querywin));
   shown=true;
}
static void querywin_stop()
{
   printf("hide\n");
   queryrunner->stop(queryrunner);
   gtk_widget_hide(win_data.querywin);
   shown=false;
}

static gboolean focus_out_cb(GtkWidget* widget, GdkEventFocus* ev, gpointer userdata)
{
   querywin_stop();
   return FALSE; /*propagate*/
}

static gboolean map_event_cb(GtkWidget *widget, GdkEvent *ev, gpointer userdata)
{
   gtk_window_present(GTK_WINDOW(win_data.querywin));
}

void result_handler_cb(struct queryrunner *caller,
                       const char *query,
                       float pertinence,
                       struct result *result,
                       gpointer userdata)
{
   g_return_if_fail(result);

   bool was_empty = g_list_length(results)==0;
   results=g_list_append(results, result);



   GtkTreeIter iter;
   gtk_list_store_append(list_model, &iter);
   gtk_list_store_set(list_model, &iter,
                      0, result,
                      -1);

   if(was_empty)
      {
         GtkTreeSelection* selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(win_data.treeview));
         gtk_tree_selection_select_iter(selection, &iter);
      }
}

static void result_free_cb(struct result *result, gpointer userdata)
{
   if(selected==result)
      selected=NULL;
   result->release(result);
}
static void clear_list_model()
{
   gtk_list_store_clear(list_model);

   g_list_foreach(results, (GFunc)result_free_cb, NULL/*user_data*/);
   g_list_free(results);
   results=NULL;
}

static gboolean run_query(gpointer userdata)
{
   if(strcmp(running_query->str, query_str->str)!=0)
      {
         g_string_assign(running_query, query_str->str);
         clear_list_model();
         queryrunner->run_query(queryrunner, running_query->str);
      }
   return FALSE;
}

static void set_query_string()
{
   strncpy(query_label_text, query_str->str, query_label_text_len-1);
   gtk_label_set_text(GTK_LABEL(win_data.query_label), query_label_text);
   g_timeout_add(300, run_query, NULL/*userdata*/);
}
static gboolean key_release_event_cb(GtkWidget* widget, GdkEventKey *ev, gpointer userdata)
{
   switch(ev->keyval)
      {
      case GDK_Escape:
         querywin_stop();
         return TRUE; /*handled*/

      case GDK_Delete:
      case GDK_BackSpace:
         if(query_str && query_str->len>0)
            {
               g_string_truncate(query_str, query_str->len-1);
               set_query_string();
            }
         last_keypress=ev->time;
         return TRUE; /*handled*/

      case GDK_Return:
         if(selected!=NULL)
            {
               querywin_stop();
               selected->execute(selected);
            }
         return TRUE; /*handled*/
      default:
         if(ev->string && ev->string[0]!='\0')
            {
               if((ev->time-last_keypress)>QUERY_TIMEOUT)
                  g_string_assign(query_str, ev->string);
               else
                  g_string_append(query_str, ev->string);
               set_query_string();
               last_keypress=ev->time;
               return TRUE;/*handled*/
            }
         break;
      }
   return FALSE; /*propagate*/
}
static void str_lower(char *dest, const char *from)
{
   int len = strlen(from);
   for(int i=0; i<len; i++)
      dest[i]=tolower(from[i]);
   dest[len]='\0';
}

static void g_string_append_markup_escaped(GString *gstr, const char *str)
{
   char *escaped = g_markup_escape_text(str, -1);
   g_string_append(gstr, escaped);
   g_free(escaped);
}
static void g_string_append_query_pango_highlight(GString *gstr, const char *query, const char *str, const char *on, const char *off)
{
   const char *markup = query_pango_highlight(query, str, on, off);
   g_string_append(gstr, markup);
   g_free((void *)markup);
}
static void cell_name_data_func(GtkTreeViewColumn* col, GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter, gpointer userdata)
{
   struct result *result = NULL;
   gtk_tree_model_get(model, iter, 0, &result, -1);
   g_return_if_fail(result);

   const char *query = query_str->str;
   const char *name = result->name;

   GString *full = g_string_new("");
   g_string_append(full, "<big><b>");
   g_string_append_query_pango_highlight(full, query, name, "<u>", "</u>");
   g_string_append(full, "</b></big>\n<small>");
   g_string_append_markup_escaped(full, result->path);
   g_string_append(full, "</small>");

   g_object_set(renderer,
                "markup",
                full->str,
                NULL);

   g_string_free(full, true/*free content*/);
}


static void selection_changed_cb(GtkTreeSelection* selection, gpointer userdata)
{
   GtkTreeIter iter;
   if(gtk_tree_selection_get_selected(selection,
                                      NULL/* *model */,
                                      &iter))
      {
         gtk_tree_model_get(GTK_TREE_MODEL(list_model), &iter, 0, &selected, -1);
         g_return_if_fail(selected);
      }
   else
      {
         selection=NULL;
      }
}
static void querywin_init_list()
{
   list_model=gtk_list_store_new(1/*n_columns*/, G_TYPE_POINTER);
   GtkTreeView *view = GTK_TREE_VIEW(win_data.treeview);


   GtkTreeViewColumn* col = gtk_tree_view_column_new();
   GtkCellRenderer* cell_renderer_name = gtk_cell_renderer_text_new();
   gtk_tree_view_column_set_title(col, "Result");
   gtk_tree_view_column_pack_start(col, cell_renderer_name, TRUE/*expand*/);
   gtk_tree_view_column_set_cell_data_func(col,
                                           cell_renderer_name,
                                           cell_name_data_func,
                                           NULL/*data*/,
                                           NULL/*destroy*/);
   gtk_tree_view_append_column(view, col);

   gtk_tree_view_set_model(view, GTK_TREE_MODEL(list_model));

   GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
   g_signal_connect(selection,
                    "changed",
                    G_CALLBACK(selection_changed_cb),
                    NULL/*userdata*/);
}

static void init_ui(void)
{

   querywin_create(&win_data);
   GtkWidget* querywin = win_data.querywin;

   gtk_window_stick(GTK_WINDOW(querywin));
   gtk_window_set_keep_above(GTK_WINDOW(querywin),
                 true);
   querywin_init_list();

   g_signal_connect(querywin,
                    "key_release_event",
                    G_CALLBACK(key_release_event_cb),
                    NULL/*data*/);

   g_signal_connect(querywin,
                    "focus_out_event",
                    G_CALLBACK(focus_out_cb),
                    NULL/*data*/);

   g_signal_connect(querywin,
                    "map_event",
                    G_CALLBACK(map_event_cb),
                    NULL/*data*/);


}

void sighandler(int signum)
{
   querywin_start();
}
static void install_sighandler()
{
   struct sigaction act;
   act.sa_handler=sighandler;
   sigemptyset(&act.sa_mask);
   act.sa_flags=0;
   act.sa_restorer=NULL;
   sigaction(SIGUSR1, &act, NULL/*oldact*/);
}


static GdkFilterReturn filter_keygrab (GdkXEvent *xevent,
                                       GdkEvent *event,
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


   query_str=g_string_new("");
   running_query=g_string_new("");
   result_queue=result_queue_new(NULL/*default context*/,
                                 result_handler_cb,
                                 NULL/*userdata*/);


   GString *catalog_path = g_string_new(getenv("HOME"));
   g_string_append(catalog_path, "/.ocha");
   mkdir(catalog_path->str, 0600);
   g_string_append(catalog_path, "/catalog");
   queryrunner = catalog_queryrunner_new(catalog_path->str,
                                         result_queue);
   init_ui();
   querywin_start();

   install_keygrab();

   install_sighandler();
   gtk_main();

   return 0;
}

