
#include "preferences_catalog.h"
#include "indexers.h"
#include "indexer_view.h"
#include "ocha_gconf.h"
#include <gtk/gtk.h>
#include <string.h>

/** \file implement the API defined in preferences_catalog.h */

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

/** Internal data */
struct preferences_catalog
{
    GtkTreeStore *model;
    GtkTreeView *view;
    GtkTreeSelection *selection;

    struct catalog *catalog;
    struct indexer_view *properties;
};

static GtkTreeStore *createModel(struct catalog *catalog);
static void add_indexer(GtkTreeStore *model, struct indexer *indexer, struct catalog *catalog);
static void add_source(GtkTreeStore *model, GtkTreeIter *parent, struct indexer *indexer, struct catalog *catalog, int source_id);
static void update_entry_count(GtkTreeStore *model, GtkTreeIter *iter, unsigned int count, struct catalog *catalog);
static GtkTreeView *createView();
static void reload_cb(GtkButton *reload, gpointer userdata);
static void row_activated_cb(GtkTreeView *view,
                             GtkTreePath *path,
                             GtkTreeViewColumn *column,
                             gpointer user_data);

/* ------------------------- static functions */
static GtkTreeStore *createModel(struct catalog *catalog)
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
    return model;
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
    ocha_gconf_get_sources(indexer->name, &ids, &ids_len);
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


            update_entry_count(model, &iter, source_id, catalog);
            source->release(source);
        }
}
static void update_entry_count(GtkTreeStore *model, GtkTreeIter *iter, unsigned int source_id, struct catalog *catalog)
{
    g_return_if_fail(model!=NULL);
    g_return_if_fail(iter!=NULL);
    g_return_if_fail(source_id!=0);

    unsigned int count=0;
    if(!catalog_get_source_content_count(catalog, source_id, &count))
        return;

    unsigned int oldcount=0;
    gtk_tree_model_get(GTK_TREE_MODEL(model), iter,
                       COLUMN_COUNT, &oldcount,
                       -1);
    gtk_tree_store_set(model, iter,
                       COLUMN_COUNT, count,
                       -1);
    int difference = count-oldcount;

    GtkTreeIter current;
    GtkTreeIter parent;
    memcpy(&current, iter, sizeof(GtkTreeIter));
    while(gtk_tree_model_iter_parent(GTK_TREE_MODEL(model),
                                     &parent,
                                     &current))
    {
        oldcount=0;
        gtk_tree_model_get(GTK_TREE_MODEL(model), &parent,
                           COLUMN_COUNT, &oldcount,
                           -1);
        gtk_tree_store_set(model, &parent,
                           COLUMN_COUNT, oldcount+difference,
                           -1);

        memcpy(&current, &parent, sizeof(GtkTreeIter));
    }
}

static GtkTreeView *createView(GtkTreeModel *model)
{
    GtkWidget *_view = gtk_tree_view_new_with_model(model);
    gtk_widget_show(_view);
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

    return view;
}

static void reindex(struct preferences_catalog *prefs, GtkTreeIter *current)
{
    g_return_if_fail(prefs!=NULL);
    g_return_if_fail(current!=NULL);

    GtkTreeStore *model=prefs->model;
    int source_id;
    gtk_tree_model_get(GTK_TREE_MODEL(model),
                       current,
                       COLUMN_SOURCE_ID,
                       &source_id,
                       -1);

    if(source_id!=0)
        {
            char *type=NULL;
            gtk_tree_model_get(GTK_TREE_MODEL(model),
                               current,
                               COLUMN_INDEXER_TYPE,
                               &type,
                               -1);
            if(type!=NULL)
                {
                    struct indexer *indexer = indexers_get(type);
                    if(indexer)
                        {
                            struct indexer_source *source = indexer->load_source(indexer,
                                                                                 prefs->catalog,
                                                                                 source_id);
                            if(source)
                                {
                                    source->index(source, prefs->catalog, NULL/*err*/);
                                    source->release(source);
                                    update_entry_count(prefs->model,
                                                       current,
                                                       source_id,
                                                       prefs->catalog);
                                }
                        }
                    g_free(type);
                }
        }
    else
        {
            GtkTreeIter child;
            if(gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &child, current))
                {
                    do
                        {
                            reindex(prefs, &child);
                        }
                    while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &child));
                }
        }
}
static void reload_cb(GtkButton *button, gpointer userdata)
{
    g_return_if_fail(userdata);
    struct preferences_catalog *prefs = (struct preferences_catalog *)userdata;
    GtkTreeIter iter;
    GtkTreeIter child;

    if(gtk_tree_selection_get_selected(prefs->selection,
                                       NULL/*ptr to model*/,
                                       &iter))
    {
        reindex(prefs, &iter);
    }
}


/** A row has been double-clicked: open properties */
static void row_activated_cb(GtkTreeView *view,
                             GtkTreePath *path,
                             GtkTreeViewColumn *column,
                             gpointer userdata)
{
    struct preferences_catalog *prefs = (struct preferences_catalog *)userdata;
    GtkTreeIter iter;
    g_return_if_fail(prefs);

    if(gtk_tree_model_get_iter(GTK_TREE_MODEL(prefs->model), &iter, path))
        {
            char *type=NULL;
            int source_id=0;
            gtk_tree_model_get(GTK_TREE_MODEL(prefs->model), &iter,
                               COLUMN_INDEXER_TYPE, &type,
                               COLUMN_SOURCE_ID, &source_id,
                               -1);
            if(type)
                {
                    struct indexer *indexer=indexers_get(type);
                    if(indexer)
                        {
                            if(source_id>0)
                                indexer_view_attach_source(prefs->properties,
                                                           indexer,
                                                           source_id);
                            else
                                indexer_view_attach_indexer(prefs->properties,
                                                            indexer);
                        }
                    g_free(type);
                }
        }
}


/* ------------------------- public functions */
struct preferences_catalog *preferences_catalog_new(struct catalog *catalog)
{
    g_return_val_if_fail(catalog!=NULL, NULL);

    struct preferences_catalog *prefs = g_new(struct preferences_catalog, 1);

    prefs->model = createModel(catalog);
    prefs->view = createView(GTK_TREE_MODEL(prefs->model));
    prefs->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(prefs->view));
    prefs->catalog=catalog;
    prefs->properties=indexer_view_new(catalog);

    g_signal_connect(prefs->view,
                     "row-activated",
                     G_CALLBACK(row_activated_cb),
                     prefs);

    return prefs;
}

GtkWidget *preferences_catalog_get_widget(struct preferences_catalog *prefs)
{
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(scroll);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll),
                      GTK_WIDGET(prefs->view));

    GtkWidget *buttons = gtk_hbutton_box_new();
    gtk_widget_show(buttons);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(buttons),
                              GTK_BUTTONBOX_END);


    GtkWidget *reload = gtk_button_new_with_label("Reload");
    gtk_widget_show(reload);
    gtk_container_add(GTK_CONTAINER(buttons), reload);
    g_signal_connect(reload,
                     "clicked",
                     G_CALLBACK(reload_cb),
                     prefs/*userdata*/);

    GtkWidget *vbox = gtk_vbox_new(FALSE/*not homogeneous*/, 4/*spacing*/);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE/*expand*/, TRUE/*fill*/, 0/*padding*/);
    gtk_box_pack_end(GTK_BOX(vbox), buttons, FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);

    return vbox;
}
