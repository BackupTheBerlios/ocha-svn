#include "resultlist.h"
#include "query.h"
#include <string.h>
#include <ctype.h>

static GtkWidget *view;
static GtkListStore *model;
static GList* results;
static GtkTreeSelection *selection;
static GString *query_str;

static void str_lower(char *dest, const char *from);
static void g_string_append_markup_escaped(GString *gstr, const char *str);
static void g_string_append_query_pango_highlight(GString *gstr, const char *query, const char *str, const char *on, const char *off);
static void cell_name_data_func(GtkTreeViewColumn* col, GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter, gpointer userdata);
static void result_free_cb(struct result *result, gpointer userdata);

/* ------------------------- public functions */
void resultlist_init()
{
   view = gtk_tree_view_new();
   gtk_widget_show (view);
   gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

   model=gtk_list_store_new(1/*n_columns*/, G_TYPE_POINTER);

   GtkTreeViewColumn* col = gtk_tree_view_column_new();
   GtkCellRenderer* cell_renderer_name = gtk_cell_renderer_text_new();
   gtk_tree_view_column_set_title(col, "Result");
   gtk_tree_view_column_pack_start(col, cell_renderer_name, TRUE/*expand*/);
   gtk_tree_view_column_set_cell_data_func(col,
                                           cell_renderer_name,
                                           cell_name_data_func,
                                           NULL/*data*/,
                                           NULL/*destroy*/);
   gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

   gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(model));

   selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

   query_str=g_string_new("");
}

void resultlist_set_current_query(const char *query)
{
   g_string_assign(query_str, query);
}

GtkWidget *resultlist_get_widget()
{
   return view;
}

struct result *resultlist_get_selected()
{
   struct result *retval = NULL;
   GtkTreeIter iter;
   if(gtk_tree_selection_get_selected(selection,
                                      NULL/* *model */,
                                      &iter))
      {
         gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &retval, -1);
      }
   return retval;
}

static void result_free_cb(struct result *result, gpointer userdata)
{
   result->release(result);
}


void resultlist_clear()
{
   gtk_list_store_clear(model);

   g_list_foreach(results, (GFunc)result_free_cb, NULL/*user_data*/);
   g_list_free(results);
   results=NULL;
}

void resultlist_add_result(float pertinence, struct result *result)
{
   bool was_empty = g_list_length(results)==0;
   results=g_list_append(results, result);

   GtkTreeIter iter;
   gtk_list_store_append(model, &iter);
   gtk_list_store_set(model, &iter,
                      0, result,
                      -1);

   if(was_empty)
      gtk_tree_selection_select_iter(selection, &iter);
}

/* ------------------------- static functions */

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
