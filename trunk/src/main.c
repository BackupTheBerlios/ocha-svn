#include <signal.h>
#include "querywin.h"
#include "result_queue.h"
#include "queryrunner.h"
#include "mock_queryrunner.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>


static struct querywin win_data;
static struct result_queue *result_queue;
static struct queryrunner *queryrunner;
static GtkListStore *list_model;
static GString* query_str;
static GList* results;

static void querywin_start()
{
   queryrunner->start(queryrunner);
   gtk_widget_show(win_data.querywin);
}
static void querywin_stop()
{
   gtk_widget_hide(win_data.querywin);
   queryrunner->stop(queryrunner);
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

   results=g_list_append(results, result);

   GtkTreeIter iter;
   gtk_list_store_append(list_model, &iter);
   gtk_list_store_set(list_model, &iter,
                      0, result,
                      -1);
}

static void result_free_cb(struct result *result, gpointer userdata)
{
   result->release(result);
}
static void clear_list_model()
{
   gtk_list_store_clear(list_model);

   g_list_foreach(results, (GFunc)result_free_cb, NULL/*user_data*/);
   g_list_free(results);
   results=NULL;
}

static void run_query()
{
   gtk_label_set_text(GTK_LABEL(win_data.query_label), query_str->str);
   clear_list_model();

   queryrunner->run_query(queryrunner, query_str->str);
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
               query_str=g_string_truncate(query_str, query_str->len-1);
               run_query();

            }
         return TRUE; /*handled*/

      default:
         if(ev->string)
            {
               printf("add: %s\n", ev->string);
               g_string_append(query_str, ev->string);
               printf("->%s\n", query_str->str);
               run_query();
               return TRUE;/*handled*/
            }
         break;
      }
   return FALSE; /*propagate*/
}
static void cell_data_func(GtkTreeViewColumn* col, GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter, gpointer userdata)
{
   struct result *result = NULL;
   gtk_tree_model_get(model, iter, 0, &result, -1);
   g_return_if_fail(result);
   g_object_set(renderer, "text", result->name, NULL);
}
static void querywin_init_list()
{
   list_model=gtk_list_store_new(1/*n_columns*/, G_TYPE_STRING);
   GtkTreeView *view = GTK_TREE_VIEW(win_data.treeview);


   GtkTreeViewColumn* col = gtk_tree_view_column_new();
   GtkCellRenderer* cell_renderer = gtk_cell_renderer_text_new();
   gtk_tree_view_column_set_title(col, "Result");
   gtk_tree_view_column_pack_start(col, cell_renderer, TRUE/*expand*/);
   gtk_tree_view_column_set_cell_data_func(col,
                                           cell_renderer,
                                           cell_data_func,
                                           NULL/*data*/,
                                           NULL/*destroy*/);
   gtk_tree_view_append_column(view, col);

   gtk_tree_view_set_model(view, GTK_TREE_MODEL(list_model));
}

static void init_ui(void)
{

   querywin_create(&win_data);
   GtkWidget* querywin = win_data.querywin;

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

int main(int argc, char *argv[])
{
   g_thread_init(NULL/*vtable*/);
   gtk_init(&argc, &argv);

   query_str=g_string_new("");
   result_queue=result_queue_new(NULL/*default context*/,
                                 result_handler_cb,
                                 NULL/*userdata*/);
   queryrunner = mock_queryrunner_new(result_queue);
   init_ui();
   querywin_start();

   gtk_main();

   return 0;
}

