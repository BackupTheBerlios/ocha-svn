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
        /** true if it's been executed */
        gboolean executed;
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

/* ------------------------- prototypes */
static void row_inserted_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer userdata);
static void row_deleted_cb(GtkTreeModel *model, GtkTreePath *path, gpointer userdata);
static void select_first_row_if_no_selection(void);
static struct resultholder *resultholder_new(const char *query, float pertinence, struct result *result);
static void resultholder_refresh(const char *query, struct resultholder *self, float pertinence);
static void resultholder_delete(struct resultholder *self);
static void append_markup_escaped(GString *gstr, const char *str);
static void append_query_pango_highlight(GString *gstr, const char *query, const char *str, const char *on, const char *off);
static const char *create_highlighted_label_markup(const char *query, struct result  *result);
static void cell_name_data_func(GtkTreeViewColumn* col, GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter, gpointer userdata);

/* ------------------------- public functions */
void resultlist_init()
{
        GtkTreeViewColumn* col;
        GtkCellRenderer* cell_renderer_name;

        view = gtk_tree_view_new();
        gtk_widget_show (view);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

        model=gtk_list_store_new(1/*n_columns*/, G_TYPE_POINTER);

        col =  gtk_tree_view_column_new();
        cell_renderer_name =  gtk_cell_renderer_text_new();
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

        hash=g_hash_table_new_full(g_str_hash,
                                   g_str_equal,
                                   g_free/*key_destroy_func*/,
                                   (GDestroyNotify)resultholder_delete/*value_destroy_func*/);

        g_signal_connect(model,
                         "row-inserted",
                         G_CALLBACK(row_inserted_cb),
                         NULL/*data*/);
        g_signal_connect(model,
                         "row-deleted",
                         G_CALLBACK(row_deleted_cb),
                         NULL/*data*/);
}

void resultlist_clear()
{
        GtkTreeIter iter;
        if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model),
                                         &iter)) {
                gboolean go_on;
                do {
                        struct resultholder *holder = NULL;
                        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &holder, -1);
                        if(holder && holder->executed) {
                                /* I refuse to remove a result that's been executed, at
                                 * least until a query is run that it does not match.
                                 */
                                go_on=gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);
                        } else {
                                if(holder) {
                                        g_hash_table_remove(hash, holder->result->path);
                                }
                                go_on=gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
                        }
                } while(go_on);
        }
}

void resultlist_executed(struct result *result)
{
        struct resultholder *holder;

        g_return_if_fail(result!=NULL);

        holder = (struct resultholder *)g_hash_table_lookup(hash, result->path);
        if(holder) {
                holder->executed=TRUE;
        }
}
void resultlist_set_current_query(const char *query)
{
        GtkTreeIter iter;

        if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model),
                                         &iter)) {
                gboolean go_on;
                do {
                        struct resultholder *holder;
                        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &holder, -1);
                        if(query_result_ismatch(query, holder->result)) {
                                /* update the label */resultholder_refresh(query, holder, holder->pertinence);
                                /* make sure viewers are told about this change */
                                gtk_list_store_set(model, &iter,
                                                   0,
                                                   holder,
                                                   -1);
                                go_on=gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);
                        } else {
                                g_hash_table_remove(hash, holder->result->path);
                                go_on=gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
                        }
                } while(go_on);
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
                                           &iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &retval, -1);
        }
        if(retval) {
                return retval->result;
        }
        return NULL;
}

void resultlist_add_result(const char *query, float pertinence, struct result *result)
{
        GtkTreeIter iter;
        const char *path = result->path;
        if(g_hash_table_lookup(hash, path) || !result->validate(result)) {
                result->release(result);
        } else {
                struct resultholder *holder = resultholder_new(query, pertinence, result);
                gtk_list_store_append(model, &iter);
                gtk_list_store_set(model, &iter,
                                   0,
                                   holder,
                                   -1);
                g_hash_table_insert(hash,
                                    g_strdup(result->path),
                                    holder);
        }
}

void resultlist_verify(void)
{
        GtkTreeIter iter;
        if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model),
                                         &iter)) {
                gboolean go_on;
                do {
                        struct resultholder *holder;
                        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &holder, -1);
                        if(!holder->result->validate(holder->result)) {
                                g_hash_table_remove(hash, holder->result->path);
                                go_on=gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
                        } else {
                                go_on=gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);
                        }
                } while(go_on);
        }
}

/* ------------------------- static functions */

/**
 * If it's the 1st row that's added, select it
 */
static void row_inserted_cb(GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer userdata)
{
        select_first_row_if_no_selection();
}
/**
 * If the row that was selected is deleted, select the 1st row instead
 */
static void row_deleted_cb(GtkTreeModel *model,
                           GtkTreePath *path,
                           gpointer userdata)
{
        select_first_row_if_no_selection();
}

static void select_first_row_if_no_selection()
{
        GtkTreeIter iter;
        if(!gtk_tree_selection_get_selected(selection,
                                            NULL/* *model */,
                                            &iter)) {
                if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model),
                                                 &iter)) {
                        gtk_tree_selection_select_iter(selection, &iter);
                }
        }
}

/**
 * Create a new result holder.
 *
 * @param pertinence
 * @param result
 * @return a resultholder linked to the result
 */
static struct resultholder *resultholder_new(const char *query,
                                             float pertinence,
                                             struct result *result)
{
        struct resultholder *retval;

        g_return_val_if_fail(result, NULL);

        retval =  g_new(struct resultholder, 1);
        retval->result=result;
        retval->pertinence=pertinence;
        retval->label_markup=create_highlighted_label_markup(query, result);
        retval->executed=FALSE;
        return retval;
}

/**
 * Refresh a result holder, after a query has
 * changed, for example.
 * @param self old result holder
 * @param pertinence new pertinence value
 * @see #query_str
 */
static void resultholder_refresh(const char *query,
                                 struct resultholder *self,
                                 float pertinence)
{
        g_return_if_fail(self);
        g_free((void *)self->label_markup);
        self->pertinence=pertinence;
        self->label_markup=create_highlighted_label_markup(query, self->result);
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
}

static void append_markup_escaped(GString *gstr, const char *str)
{
        char *escaped = g_markup_escape_text(str, -1);
        g_string_append(gstr, escaped);
        g_free(escaped);
}
static void append_query_pango_highlight(GString *gstr,
                                                  const char *query,
                                                  const char *str,
                                                  const char *on,
                                                  const char *off)
{
        const char *markup = query_pango_highlight(query, str, on, off);
        g_string_append(gstr, markup);
        g_free((void *)markup);
}

static const char *create_highlighted_label_markup(const char *query,
                                                   struct result  *result)
{
        const char *markup;
        const char *name;
        GString *full;

        name =  result->name;
        full =  g_string_new("");
        g_string_append(full, "<big><b>");
        append_query_pango_highlight(full, query, name, "<u>", "</u>");
        g_string_append(full, "</b></big>\n<small>");
        append_markup_escaped(full, result->long_name);
        g_string_append(full, "</small>");

        markup =  full->str;
        g_string_free(full, FALSE/* do not free content*/);
        return markup;
}

static void cell_name_data_func(GtkTreeViewColumn* col,
                                GtkCellRenderer* renderer,
                                GtkTreeModel* model,
                                GtkTreeIter* iter,
                                gpointer userdata)
{
        struct resultholder *holder = NULL;
        gtk_tree_model_get(model, iter, 0, &holder, -1);
        if(holder) {
                g_object_set(renderer,
                             "markup",
                             holder->label_markup,
                             NULL);
        }
}

