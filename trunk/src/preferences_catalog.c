
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
struct prefs_and_indexer
{
        struct preferences_catalog *prefs;
        struct indexer *indexer;
};
struct preferences_catalog
{
        GtkListStore *model;
        GtkTreeView *view;
        GtkTreeSelection *selection;
        /** root widget, retured by get_widget */
        GtkWidget *widget;
        GtkLabel *properties_title;

        struct catalog *catalog;
        struct indexer_view *properties;

        /**
         * A prefs_and_indexer structure
         * for each existing indexer. null-terminated
         */
        struct prefs_and_indexer *pai;
};

/* ------------------------- prototypes */
static GtkListStore *createModel(struct catalog *catalog);
static void add_indexer(GtkListStore *model, struct indexer *indexer, struct catalog *catalog);
static gboolean find_iter_for_source(GtkListStore *model, int goal_id, GtkTreeIter *iter_out);
static void display_name_changed(struct indexer_source *source, gpointer userdata);
static void add_source(GtkListStore *model, struct indexer *indexer, struct catalog *catalog, int source_id, GtkTreeIter *iter_out);
static void update_entry_count(GtkListStore *model, GtkTreeIter *iter, unsigned int source_id, struct catalog *catalog);
static GtkTreeView *createView(GtkTreeModel *model);
static void reindex(struct preferences_catalog *prefs, GtkTreeIter *current);
static void reload_cb(GtkButton *button, gpointer userdata);
static void update_properties(struct preferences_catalog  *prefs);
static void row_selection_changed_cb(GtkTreeSelection *selection, gpointer userdata);
static void select_row(struct preferences_catalog  *prefs, GtkTreeIter *source_iter);
static void new_source_button_or_item_cb(GtkWidget *button_or_item, gpointer userdata);
static void new_source_popup_cb(GtkButton *button, gpointer userdata);
static void delete_cb(GtkButton *button, gpointer userdata);
static GtkWidget *init_widget(struct preferences_catalog *prefs);

/* ------------------------- public functions */
static int count_indexers(struct indexer **indexers)
{
        int retval=0;

        for(;*indexers; indexers++, retval++)
                ;

        return retval;
}
struct preferences_catalog *preferences_catalog_new(struct catalog *catalog)
{
        struct preferences_catalog *prefs;
        struct indexer **indexers = indexers_list();
        int indexer_count=count_indexers(indexers);
        int i;
        g_return_val_if_fail(catalog!=NULL, NULL);

        prefs = g_new(struct preferences_catalog, 1);
        prefs->catalog=catalog;
        prefs->pai = g_new(struct prefs_and_indexer, indexer_count+1);
        for(i=0; i<indexer_count; i++) {
                prefs->pai[i].indexer=indexers[i];
                prefs->pai[i].prefs=prefs;
        }
        prefs->pai[indexer_count].indexer=NULL;
        prefs->pai[indexer_count].prefs=NULL;

        prefs->model = createModel(catalog);
        prefs->view = createView(GTK_TREE_MODEL(prefs->model));
        prefs->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(prefs->view));
        prefs->properties=indexer_view_new(catalog);
        prefs->widget = init_widget(prefs);

        g_signal_connect(prefs->selection,
                         "changed",
                         G_CALLBACK(row_selection_changed_cb),
                         prefs);

        return prefs;
}

GtkWidget *preferences_catalog_get_widget(struct preferences_catalog *prefs)
{
        return prefs->widget;
}

/* ------------------------- static functions */
static GtkListStore *createModel(struct catalog *catalog)
{
        GtkListStore *model;
        struct indexer **indexers;

        model = gtk_list_store_new(NUMBER_OF_COLUMNS,
                                   G_TYPE_STRING/*COLUMN_LABEL*/,
                                   G_TYPE_STRING/*COLUMN_INDEXER_TYPE*/,
                                   G_TYPE_INT/*COLUMN_SOURCE_ID*/,
                                   G_TYPE_UINT/*COLUMN_COUNT*/);

        for(indexers = indexers_list();
            *indexers;
            indexers++) {
                struct indexer *indexer = *indexers;
                add_indexer(model, indexer, catalog);
        }
        return model;
}

static void add_indexer(GtkListStore *model,
                        struct indexer *indexer,
                        struct catalog *catalog)
{
        int *ids = NULL;
        int ids_len = -1;

        ocha_gconf_get_sources(indexer->name, &ids, &ids_len);
        if(ids) {
                int i;
                for(i=0; i<ids_len; i++) {
                        add_source(model,
                                   indexer,
                                   catalog,
                                   ids[i], NULL/*iter_out*/);
                }
                g_free(ids);
        }
}
static gboolean find_iter_for_source(GtkListStore *model,
                                     int goal_id,
                                     GtkTreeIter *iter_out)
{
        int source_id;
        g_return_val_if_fail(model, FALSE);
        g_return_val_if_fail(iter_out, FALSE);

        if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), iter_out)) {
                gtk_tree_model_get(GTK_TREE_MODEL(model), iter_out,
                                   COLUMN_SOURCE_ID, &source_id,
                                   -1);
                if(source_id==goal_id) {
                        return TRUE;
                }
        } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), iter_out));
        /* iter_out has been set to be invalid */
        return FALSE;

}

static void display_name_changed(struct indexer_source *source, gpointer userdata)
{
        GtkListStore *model = GTK_LIST_STORE(userdata);
        GtkTreeIter iter;

        g_return_if_fail(source);
        g_return_if_fail(model);

        if(find_iter_for_source(model, source->id, &iter))
        {
                gtk_list_store_set(model, &iter,
                                   COLUMN_LABEL, source->display_name,
                                   -1);
        }
}
static void add_source(GtkListStore *model,
                       struct indexer *indexer,
                       struct catalog *catalog,
                       int source_id,
                       GtkTreeIter *iter_out)
{
        GtkTreeIter iter;
        struct indexer_source *source = indexer_load_source(indexer, catalog, source_id);
        if(source!=NULL)
        {
                gtk_list_store_append(model, &iter);
                if(iter_out)
                        memcpy(iter_out, &iter, sizeof(GtkTreeIter));
                gtk_list_store_set(model, &iter,
                                   COLUMN_LABEL, source->display_name,
                                   COLUMN_INDEXER_TYPE, indexer->name,
                                   COLUMN_SOURCE_ID, source_id,
                                   -1);

                indexer_source_notify_display_name_change(source,
                                catalog,
                                display_name_changed,
                                model);
                update_entry_count(model, &iter, source_id, catalog);
                indexer_source_release(source);
        }
}
static void update_entry_count(GtkListStore *model,
                               GtkTreeIter *iter,
                               unsigned int source_id,
                               struct catalog *catalog)
{
        unsigned int count=0;

        g_return_if_fail(model!=NULL);
        g_return_if_fail(iter!=NULL);
        g_return_if_fail(source_id!=0);


        if(!catalog_get_source_content_count(catalog, source_id, &count)) {
                return;
        }

        gtk_list_store_set(model, iter,
                           COLUMN_COUNT, count,
                           -1);
}

static GtkTreeView *createView(GtkTreeModel *model)
{
        GtkWidget *_view = gtk_tree_view_new_with_model(model);
        GtkTreeView *view = GTK_TREE_VIEW(_view);
        GtkTreeViewColumn *labelCol;
        GtkTreeViewColumn *countCol;
        gtk_widget_show(_view);

        gtk_tree_view_set_headers_visible(view, TRUE);
        gtk_tree_view_insert_column_with_attributes(view,
                        0,
                        "Sources",
                        gtk_cell_renderer_text_new(),
                        "text", COLUMN_LABEL,
                        NULL);
        labelCol = gtk_tree_view_get_column(view, 0);
        gtk_tree_view_column_set_expand(labelCol, TRUE);

        gtk_tree_view_insert_column_with_attributes(view,
                        1,
                        "Count",
                        gtk_cell_renderer_text_new(),
                        "text", COLUMN_COUNT,
                        NULL);
        countCol = gtk_tree_view_get_column(view, 1);
        gtk_tree_view_column_set_expand(countCol, FALSE);
        return view;
}

static void reindex(struct preferences_catalog *prefs, GtkTreeIter *current)
{
        GtkListStore *model;
        int source_id;

        g_return_if_fail(prefs!=NULL);
        g_return_if_fail(current!=NULL);

        model = prefs->model;
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
                if(type!=NULL) {
                        struct indexer *indexer = indexers_get(type);
                        if(indexer) {
                                struct indexer_source *source;
                                source = indexer_load_source(indexer,
                                                             prefs->catalog,
                                                             source_id);
                                if(source) {
                                        indexer_source_index(source,
                                                             prefs->catalog,
                                                             NULL/*err*/);
                                        indexer_source_release(source);
                                        update_entry_count(prefs->model,
                                                           current,
                                                           source_id,
                                                           prefs->catalog);
                                }
                        }
                        g_free(type);
                }
        } else {
                GtkTreeIter child;
                if(gtk_tree_model_iter_children(GTK_TREE_MODEL(model),
                                                &child,
                                                current)) {
                        do {
                                reindex(prefs, &child);
                        } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model),
                                                         &child));
                }
        }
}
static void reload_cb(GtkButton *button, gpointer userdata)
{
        struct preferences_catalog *prefs;
        GtkTreeIter iter;

        g_return_if_fail(userdata);
        prefs =  (struct preferences_catalog *)userdata;

        if(gtk_tree_selection_get_selected(prefs->selection,
                                           NULL/*ptr to model*/,
                                           &iter)) {
                reindex(prefs, &iter);
        }
}

static void update_properties(struct preferences_catalog  *prefs)
{
        GtkTreeIter iter;
        if(gtk_tree_selection_get_selected(prefs->selection, NULL, &iter))
        {
                char *type=NULL;
                int source_id=0;
                gtk_tree_model_get(GTK_TREE_MODEL(prefs->model),
                                   &iter,
                                   COLUMN_INDEXER_TYPE, &type,
                                   COLUMN_SOURCE_ID, &source_id,
                                   -1);
                if(type) {
                        struct indexer *indexer=indexers_get(type);
                        if(indexer) {
                                if(source_id>0) {
                                        struct indexer_source *source;
                                        source=indexer_load_source(indexer,
                                                                   prefs->catalog,
                                                                   source_id);
                                        if(source) {
                                                indexer_view_attach_source(prefs->properties,
                                                                           indexer,
                                                                           source);
                                                indexer_source_release(source);
                                        }

                                } else {
                                        indexer_view_attach_indexer(prefs->properties,
                                                                    indexer);
                                }
                                gtk_label_set_text(prefs->properties_title,
                                                   indexer->display_name);
                        }
                        g_free(type);
                }
        } else {
                indexer_view_detach(prefs->properties);
                gtk_label_set_text(prefs->properties_title, "");
        }
}

/** Selection has been modified */
static void row_selection_changed_cb(GtkTreeSelection *selection, gpointer userdata)
{
        struct preferences_catalog *prefs = (struct preferences_catalog *)userdata;
        g_return_if_fail(prefs);

        update_properties(prefs);
}


/**
 * Make sure the row is visible and then select it
 */
static void select_row(struct preferences_catalog  *prefs, GtkTreeIter *source_iter)
{
        GtkTreePath *path;
        path = gtk_tree_model_get_path(GTK_TREE_MODEL(prefs->model), source_iter);
        if(path) {
                gtk_tree_view_expand_to_path(prefs->view,
                                             path);
                gtk_tree_view_scroll_to_cell(prefs->view,
                                             path,
                                             NULL/*column*/,
                                             FALSE/*do not use_align*/,
                                             0,
                                             0);
                gtk_tree_path_free(path);
        }
        gtk_tree_selection_select_iter(prefs->selection, source_iter);
}


static void new_source_button_or_item_cb(GtkWidget *button_or_item, gpointer userdata)
{
        struct prefs_and_indexer *pai;
        struct preferences_catalog *prefs;
        struct indexer *indexer;
        struct indexer_source *source;

        pai =  (struct prefs_and_indexer *)userdata;
        g_return_if_fail(pai);
        prefs =  pai->prefs;
        indexer =  pai->indexer;

        source = indexer_new_source(indexer, prefs->catalog, NULL/*err*/);
        if(source) {
                GtkTreeIter source_iter;
                add_source(GTK_LIST_STORE(prefs->model),
                           indexer,
                           prefs->catalog,
                           source->id,
                           &source_iter);

                select_row(prefs, &source_iter);
                indexer_source_release(source);
        }
}
static void new_source_popup_cb(GtkButton *button, gpointer userdata)
{
        GtkMenu *menu = GTK_MENU(userdata);
        g_return_if_fail(menu);

        gtk_menu_popup(menu,
                       NULL,
                       NULL/*parent_menu_item*/,
                       NULL/*position_func*/,
                       NULL/*userdata*/,
                       0/*button*/,
                       gtk_get_current_event_time());
}

static void delete_cb(GtkButton *button, gpointer userdata)
{
        GtkTreeIter iter;
        struct preferences_catalog *prefs = (struct preferences_catalog *)userdata;
        g_return_if_fail(prefs);

        if(gtk_tree_selection_get_selected(prefs->selection, NULL, &iter)) {
                char *type=NULL;
                int source_id=0;
                gtk_tree_model_get(GTK_TREE_MODEL(prefs->model),
                                   &iter,
                                   COLUMN_INDEXER_TYPE, &type,
                                   COLUMN_SOURCE_ID, &source_id,
                                   -1);
                if(type && source_id>0) {
                        struct indexer *indexer=indexers_get(type);
                        if(indexer) {
                                struct indexer_source *source;
                                source=indexer_load_source(indexer,
                                                           prefs->catalog,
                                                           source_id);
                                if(source) {
                                        if(gtk_list_store_remove(prefs->model, &iter)) {
                                                gtk_tree_selection_select_iter(prefs->selection, &iter);
                                        } else {
                                                int node_count;
                                                node_count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(prefs->model),
                                                                                            NULL/*parent=root*/);
                                                if(node_count>0) {
                                                        if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(prefs->model),
                                                                                         &iter,
                                                                                         NULL/*parent=root*/,
                                                                                         node_count-1)) {
                                                                gtk_tree_selection_select_iter(prefs->selection, &iter);
                                                        }
                                                }
                                        }
                                        indexer_source_destroy(source, prefs->catalog);
                                }
                        }
                }
                if(type)
                        g_free(type);
        }
}

static GtkWidget *init_widget(struct preferences_catalog *prefs)
{
        GtkWidget *scroll;
        GtkWidget *buttons;
        int new_source_count;
        int i;
        GtkWidget *del;
        GtkWidget *reload;
        GtkWidget *vbox;
        GtkWidget *properties_label;
        PangoAttrList *attrs;
        GtkWidget *properties_frame;
        GtkWidget *twopanes;


        scroll =  gtk_scrolled_window_new(NULL, NULL);
        gtk_widget_show(scroll);
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll),
                                            GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(scroll),
                          GTK_WIDGET(prefs->view));

        buttons =  gtk_hbutton_box_new();
        gtk_widget_show(buttons);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(buttons),
                                  GTK_BUTTONBOX_END);

        new_source_count =  0;
        for(i=0; prefs->pai[i].indexer; i++)
        {
                struct indexer *indexer = prefs->pai[i].indexer;
                if(indexer->new_source)
                        new_source_count++;
        }
        if(new_source_count>0)
        {
                GtkWidget *add;
                add = gtk_button_new_from_stock(GTK_STOCK_NEW);
                gtk_widget_show(add);
                gtk_container_add(GTK_CONTAINER(buttons), add);
                if(new_source_count==1) {
                        int i;
                        for(i=0; prefs->pai[i].indexer; i++) {
                                struct prefs_and_indexer *pai = &prefs->pai[i];
                                if(pai->indexer->new_source) {
                                        g_signal_connect(add,
                                                         "clicked",
                                                         G_CALLBACK(new_source_button_or_item_cb),
                                                         pai);
                                        break;
                                }
                        }
                } else {
                        GtkWidget *popup = gtk_menu_new();
                        int i;
                        gtk_widget_show(popup);
                        for(i=0; prefs->pai[i].indexer; i++) {
                                struct prefs_and_indexer *pai = &prefs->pai[i];
                                if(pai->indexer->new_source) {
                                        GtkWidget *item=gtk_menu_item_new_with_label(pai->indexer->display_name);
                                        gtk_widget_show(item);
                                        gtk_menu_append(GTK_MENU(popup), item);
                                        g_signal_connect(item,
                                                         "activate",
                                                         G_CALLBACK(new_source_button_or_item_cb),
                                                         pai);
                                }
                        }
                        g_signal_connect(add,
                                         "pressed",
                                         G_CALLBACK(new_source_popup_cb),
                                         popup);
                }
        }

        del =  gtk_button_new_from_stock(GTK_STOCK_DELETE);
        gtk_widget_show(del);
        gtk_container_add(GTK_CONTAINER(buttons), del);
        g_signal_connect(del,
                         "clicked",
                         G_CALLBACK(delete_cb),
                         prefs);

        reload =  gtk_button_new_from_stock(GTK_STOCK_REFRESH);
        gtk_widget_show(reload);
        gtk_container_add(GTK_CONTAINER(buttons), reload);
        g_signal_connect(reload,
                         "clicked",
                         G_CALLBACK(reload_cb),
                         prefs/*userdata*/);


        vbox =  gtk_vbox_new(FALSE/*not homogeneous*/,
                             4/*spacing*/);
        gtk_widget_show(vbox);
        gtk_box_pack_start(GTK_BOX(vbox),
                           scroll,
                           TRUE/*expand*/,
                           TRUE/*fill*/,
                           0/*padding*/);
        gtk_box_pack_end(GTK_BOX(vbox),
                         buttons,
                         FALSE/*expand*/,
                         FALSE/*fill*/,
                         0/*padding*/);

        properties_label =  gtk_label_new("");
        gtk_widget_show(properties_label);
        prefs->properties_title=GTK_LABEL(properties_label);

        attrs =  pango_attr_list_new();
        pango_attr_list_insert(attrs,
                               pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes(GTK_LABEL(properties_label),
                                 attrs);
        pango_attr_list_unref(attrs);

        properties_frame =  gtk_frame_new("");
        gtk_widget_show(properties_frame);
        gtk_frame_set_shadow_type(GTK_FRAME(properties_frame),
                                  GTK_SHADOW_NONE);
        gtk_frame_set_label_widget(GTK_FRAME(properties_frame),
                                   properties_label);
        gtk_container_add(GTK_CONTAINER(properties_frame),
                          indexer_view_widget(prefs->properties));

        twopanes =  gtk_hpaned_new();
        gtk_widget_show(twopanes);
        gtk_paned_pack1(GTK_PANED(twopanes),
                        vbox,
                        FALSE/*resize*/,
                        FALSE/*shrink*/);
        gtk_paned_pack2(GTK_PANED(twopanes),
                        properties_frame,
                        TRUE/*resize*/,
                        TRUE/*shrink*/);

        return twopanes;
}
