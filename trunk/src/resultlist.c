#include "resultlist.h"
#include "query.h"
#include <string.h>
#include <ctype.h>

/**
 * Container for a result and some associated
 * data.
 */
struct resultholder
{
   /** the result */
   struct result *result;
   /** pertinence of the result, passed to resultlist_add() */
   float pertinence;
   /** pango markup for the label */
   const char *label_markup;
};

/**
 * GtkListView widget.
 * This list contains one column, which is displayed
 * using cell_name_data_func()
 */
static GtkWidget *view;

/**
 * GtkListStore, the model of view.
 * A list of struct resultholder *
 */
static GtkListStore *model;

/**
 * char * x struct resultholder *, result path -> holder.
 * This hash map is contains exactly the same
 * value as the model. It MUST BE UPDATED whenever
 * the model is updated. result holders are
 * freed automatically whenever they're removed
 * from the hash map.
 */
static GHashTable *hash;

/** List selection */
static GtkTreeSelection *selection;

/** Current query */
static GString *query_str;

static void str_lower(char *dest, const char *from);
static void g_string_append_markup_escaped(GString *gstr, const char *str);
static void g_string_append_query_pango_highlight(GString *gstr, const char *query, const char *str, const char *on, const char *off);
static const char *create_highlighted_label_markup(struct result *result);
static void cell_name_data_func(GtkTreeViewColumn* col, GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter, gpointer userdata);
static void result_free_cb(struct result *result, gpointer userdata);

static struct resultholder *resultholder_new(float pertinence, struct result *result);
static void resultholder_refresh(struct resultholder *, float pertinence);
static void resultholder_delete(struct resultholder *);

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
                                           NULL/*destroy data*/);
   gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

   gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(model));

   selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

   query_str=g_string_new("");

   hash=g_hash_table_new_full(g_str_hash,
                              g_str_equal,
                              g_free/*key_destroy_func*/,
                              (GDestroyNotify)resultholder_delete/*value_destroy_func*/);
}

void resultlist_set_current_query(const char *query)
{
   g_string_assign(query_str, query);

   GtkTreeIter iter;

   if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model),
                                    &iter))
      {
         bool go_on;
         do
            {
               struct resultholder *holder;
               gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &holder, -1);
               if(query_result_ismatch(query_str->str, holder->result))
                  {
                     /* update the label */
                     resultholder_refresh(holder, holder->pertinence);
                     /* make sure viewers are told about this change */
                     gtk_list_store_set(model, &iter,
                                        0,
                                        holder,
                                        -1);
                     go_on=gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);
                  }
               else
                  {
                     g_hash_table_remove(hash, holder->result->path);
                     go_on=gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
                  }
            }
         while(go_on);
      }
}

GtkWidget *resultlist_get_widget()
{
   return view;
}

struct result *resultlist_get_selected()
{
   struct resultholder *retval = NULL;
   GtkTreeIter iter;
   if(gtk_tree_selection_get_selected(selection,
                                      NULL/* *model */,
                                      &iter))
      {
         gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &retval, -1);
      }
   if(retval)
      return retval->result;
   return NULL;
}

void resultlist_add_result(float pertinence, struct result *result)
{
   GtkTreeIter iter;
   bool was_empty = !gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model),
                                                   &iter);

   const char *path = result->path;
   if(g_hash_table_lookup(hash, path))
      {
         result->release(result);
      }
   else
      {
         struct resultholder *holder = resultholder_new(pertinence, result);
         gtk_list_store_append(model, &iter);
         gtk_list_store_set(model, &iter,
                            0,
                            holder,
                            -1);
         g_hash_table_insert(hash,
                             g_strdup(result->path),
                             holder);

         if(was_empty)
            gtk_tree_selection_select_iter(selection, &iter);

      }
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

static const char *create_highlighted_label_markup(struct result  *result)
{
   const char *query = query_str->str;
   const char *name = result->name;

   GString *full = g_string_new("");
   g_string_append(full, "<big><b>");
   g_string_append_query_pango_highlight(full, query, name, "<u>", "</u>");
   g_string_append(full, "</b></big>\n<small>");
   g_string_append_markup_escaped(full, result->path);
   g_string_append(full, "</small>");

   const char *markup = full->str;
   g_string_free(full, false/* do not free content*/);
   return markup;
}

static void cell_name_data_func(GtkTreeViewColumn* col, GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter, gpointer userdata)
{
   struct resultholder *holder = NULL;
   gtk_tree_model_get(model, iter, 0, &holder, -1);
   g_return_if_fail(holder);

   g_object_set(renderer,
                "markup",
                holder->label_markup,
                NULL);
}

/* ------------------------- resultholder functions */
/**
 * Create a new result holder.
 *
 * @param pertinence
 * @param result
 * @return a resultholder linked to the result
 */
static struct resultholder *resultholder_new(float pertinence, struct result *result)
{
   g_return_val_if_fail(result, NULL);

   struct resultholder *retval = g_new(struct resultholder, 1);
   retval->result=result;
   retval->pertinence=pertinence;
   retval->label_markup=create_highlighted_label_markup(result);
   printf("0x%lx:new\n", retval);
   return retval;
}

/**
 * Refresh a result holder, after a query has
 * changed, for example.
 * @param self old result holder
 * @param pertinence new pertinence value
 * @see #query_str
 */
static void resultholder_refresh(struct resultholder *self, float pertinence)
{
   g_return_if_fail(self);
   g_free((void *)self->label_markup);
   self->pertinence=pertinence;
   self->label_markup=create_highlighted_label_markup(self->result);
}

/**
 * Free a result holder and the result in it.
 * @param self result holder to free
 */
static void resultholder_delete(struct resultholder *self)
{
   g_return_if_fail(self);
   self->result->release(self->result);
   g_free((void *)self->label_markup);
   g_free(self);
   printf("0x%lx:delete\n", self);
}
