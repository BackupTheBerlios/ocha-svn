
#include "preferences_catalog.h"
#include "indexers.h"
#include <gtk/gtk.h>

/** \file implement the API defined in preference_catalog.h */

typedef enum {
    /** a column of type STRING that contains the label of
     * the indexer or indexer source
     */
    COLUMN_LABEL=0,
    /** name of the indexer (STRING) */
    COLUMN_INDEXER_TYPE,
    /** source ID, if it's a source (INT)*/
    COLUMN_SOURCE_ID,
    /** # of entries in the source (or in the indexer) */
    COLUMN_COUNT,
    NUMBER_OF_COLUMNS
} TreeViewColumns;

static GtkTreeModel *createModel(struct catalog *catalog);
static void add_indexer(GtkTreeStore *model, struct indexer *indexer, struct catalog *catalog);
static void add_source(GtkTreeStore *model, GtkTreeIter *parent, struct indexer *indexer, struct catalog *catalog, int source_id);
static void increase_entry_count(GtkTreeStore *model, GtkTreeIter *iter, unsigned int count);
static GtkWidget *createView();

/* ------------------------- static functions */
static GtkTreeModel *createModel(struct catalog *catalog)
{
    GtkTreeStore *model = gtk_tree_store_new(NUMBER_OF_COLUMNS,
                                             G_TYPE_STRING/*COLUMN_LABEL*/,
                                             G_TYPE_STRING/*COLUMN_INDEXER_TYPE*/,
                                             G_TYPE_INT/*COLUMN_SOURCE_ID*/,
                                             G_TYPE_UINT/*COLUMN_COUNT*/);

    for(struct indexer **indexers = indexers_list();
        *indexers;
        indexers++)
        {
            struct indexer *indexer = *indexers;
            add_indexer(model, indexer, catalog);
        }
    return GTK_TREE_MODEL(model);
}

static void add_indexer(GtkTreeStore *model, struct indexer *indexer, struct catalog *catalog)
{
    GtkTreeIter iter;
    gtk_tree_store_append(model, &iter, NULL/*toplevel*/);
    gtk_tree_store_set(model, &iter,
                       COLUMN_LABEL, indexer->display_name,
                       COLUMN_INDEXER_TYPE, indexer->name,
                       -1);

    int *ids = NULL;
    int ids_len = -1;
    if(catalog_list_sources(catalog, indexer->name, &ids, &ids_len))
        {
            if(ids)
                {
                    for(int i=0; i<ids_len; i++)
                        {
                            add_source(model,
                                       &iter,
                                       indexer,
                                       catalog,
                                       ids[i]);
                        }
                    g_free(ids);
                }
        }
}

static void add_source(GtkTreeStore *model,
                       GtkTreeIter *parent,
                       struct indexer *indexer,
                       struct catalog *catalog,
                       int source_id)
{
    GtkTreeIter iter;
    struct indexer_source *source = indexer->load_source(indexer, catalog, source_id);
    if(source!=NULL)
        {
            gtk_tree_store_append(model, &iter, parent);
            gtk_tree_store_set(model, &iter,
                               COLUMN_LABEL, source->display_name,
                               COLUMN_INDEXER_TYPE, indexer->name,
                               COLUMN_SOURCE_ID, source_id,
                               -1);
            int count=0;
            if(catalog_get_source_content_count(catalog, source_id, &count))
                increase_entry_count(model, &iter, count);
            source->release(source);
        }
}
static void increase_entry_count(GtkTreeStore *model, GtkTreeIter *iter, unsigned int count)
{
    unsigned int oldcount=0;
    gtk_tree_model_get(GTK_TREE_MODEL(model), iter,
                       COLUMN_COUNT, &oldcount,
                       -1);
    count+=oldcount;
    gtk_tree_store_set(model, iter,
                       COLUMN_COUNT, count,
                       -1);

    GtkTreeIter parent;
    if(gtk_tree_model_iter_parent(GTK_TREE_MODEL(model),
                                  &parent,
                                  iter))
        {
            increase_entry_count(model, &parent, count);
        }
}

static GtkWidget *createView(GtkTreeModel *model)
{
    GtkWidget *_view = gtk_tree_view_new_with_model(model);
    GtkTreeView *view = GTK_TREE_VIEW(_view);

    gtk_tree_view_set_headers_visible(view, TRUE);
    gtk_tree_view_insert_column_with_attributes(view,
                                                0,
                                                "Sources",
                                                gtk_cell_renderer_text_new(),
                                                "text", COLUMN_LABEL,
                                                NULL);
    gtk_tree_view_insert_column_with_attributes(view,
                                                1,
                                                "Count",
                                                gtk_cell_renderer_text_new(),
                                                "text", COLUMN_COUNT,
                                                NULL);
    return _view;
}

/* ------------------------- public functions */
GtkWidget *preferences_catalog_new(struct catalog *catalog)
{
    GtkTreeModel* model = createModel(catalog);
    GtkWidget * view = createView(model);
    gtk_widget_show(view);
    g_object_unref(model);
    model=NULL;

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(scroll);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll),
                      view);


    return scroll;
}
