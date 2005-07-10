
#include "preferences_catalog.h"
#include "indexers.h"
#include "indexer_view.h"
#include "parse_uri_list_next.h"
#include "ocha_gconf.h"
#include <gtk/gtk.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs.h>

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
        /** TRUE if this is a system source */
        COLUMN_IS_SYSTEM,
        /** TRUE if the source is enabled, FALSE otherwise */
        COLUMN_ENABLED,

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
        GtkWidget *delete_button;
        GtkWidget *refresh_button;
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
static GtkListStore *create_model(struct catalog *catalog);
static void add_indexer(GtkListStore *model, struct indexer *indexer, struct catalog *catalog);
static gboolean find_iter_for_source(GtkListStore *model, int goal_id, GtkTreeIter *iter_out);
static void display_name_changed(struct indexer_source *source, gpointer userdata);
static void add_source(GtkListStore *model, struct indexer *indexer, struct catalog *catalog, int source_id, GtkTreeIter *iter_out);
static void update_entry_count(GtkListStore *model, GtkTreeIter *iter, unsigned int source_id, struct catalog *catalog);
static GtkTreeView *create_view(GtkTreeModel *model, struct preferences_catalog *prefs);
static void reindex(struct preferences_catalog *prefs, GtkTreeIter *current);
static void reload_cb(GtkButton *button, gpointer userdata);
static void update_properties(struct preferences_catalog  *prefs);
static void row_selection_changed_cb(GtkTreeSelection *selection, gpointer userdata);
static void select_row(struct preferences_catalog  *prefs, GtkTreeIter *source_iter);
static void register_new_source(struct preferences_catalog  *prefs, struct indexer  *indexer, struct indexer_source  *source);
static void new_source_button_or_item_cb(GtkWidget *button_or_item, gpointer userdata);
static void new_source_popup_cb(GtkButton *button, gpointer userdata);
static void delete_cb(GtkButton *button, gpointer userdata);
static GtkWidget *init_widget(struct preferences_catalog *prefs);
static gint list_sort_cb(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata);
static void source_enabled_toggle_cb(GtkCellRendererToggle *cell, gchar *path_string, gpointer userdata);
static void install_drop(struct preferences_catalog *prefs);
static void drop_cb(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer userdata);
static gboolean add_directory_uri(struct preferences_catalog *prefs, const gchar *uri);
static void display_dnd_errors(struct preferences_catalog *prefs, GSList *errors);

/* ------------------------- data */
GtkTargetEntry drop_targets[] = {
        { "text/uri-list", 0 , 0 },
        { "text/plain", 0 , 0 } /* for Firefox */
};
#define drop_targets_len (sizeof(drop_targets)/sizeof(GtkTargetEntry))

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

        prefs->model = create_model(catalog);



        prefs->view = create_view(GTK_TREE_MODEL(prefs->model), prefs);
        prefs->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(prefs->view));
        prefs->properties=indexer_view_new(catalog);
        prefs->widget = init_widget(prefs);

        g_signal_connect(prefs->selection,
                         "changed",
                         G_CALLBACK(row_selection_changed_cb),
                         prefs);

        install_drop(prefs);

        return prefs;
}

GtkWidget *preferences_catalog_get_widget(struct preferences_catalog *prefs)
{
        return prefs->widget;
}

/* ------------------------- static functions */
static GtkListStore *create_model(struct catalog *catalog)
{
        GtkListStore *model;
        struct indexer **indexers;

        model = gtk_list_store_new(NUMBER_OF_COLUMNS,
                                   G_TYPE_STRING/*COLUMN_LABEL*/,
                                   G_TYPE_STRING/*COLUMN_INDEXER_TYPE*/,
                                   G_TYPE_INT/*COLUMN_SOURCE_ID*/,
                                   G_TYPE_UINT/*COLUMN_COUNT*/,
                                   G_TYPE_BOOLEAN/*COLUMN_SYSTEM*/,
                                   G_TYPE_BOOLEAN/*COLUMN_ENABLED*/);
        g_assert(NUMBER_OF_COLUMNS==6);

        for(indexers = indexers_list();
            *indexers;
            indexers++) {
                struct indexer *indexer = *indexers;
                add_indexer(model, indexer, catalog);
        }

        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                        0/*sort_column_id*/,
                                        list_sort_cb,
                                        NULL/*userdata*/,
                                        NULL/*destroy_notify*/);
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
                                             0,
                                             GTK_SORT_ASCENDING);


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
                do {
                        gtk_tree_model_get(GTK_TREE_MODEL(model), iter_out,
                                           COLUMN_SOURCE_ID, &source_id,
                                           -1);
                        if(source_id==goal_id) {
                                return TRUE;
                        }
                } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), iter_out));
        }
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
                gboolean source_enabled=TRUE;
                gtk_list_store_append(model, &iter);
                if(iter_out)
                        memcpy(iter_out, &iter, sizeof(GtkTreeIter));

                catalog_source_get_enabled(catalog, source_id, &source_enabled);
                gtk_list_store_set(model, &iter,
                                   COLUMN_LABEL, source->display_name,
                                   COLUMN_INDEXER_TYPE, indexer->name,
                                   COLUMN_SOURCE_ID, source_id,
                                   COLUMN_IS_SYSTEM, source->system,
                                   COLUMN_ENABLED, source_enabled,
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

static GtkTreeView *create_view(GtkTreeModel *model, struct preferences_catalog *prefs)
{
        GtkWidget *_view = gtk_tree_view_new_with_model(model);
        GtkTreeView *view = GTK_TREE_VIEW(_view);
        GtkTreeViewColumn *labelCol;
        GtkTreeViewColumn *countCol;
        GtkTreeViewColumn *enabledCol;
        GtkCellRenderer *enabledRenderer;
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

        enabledRenderer = gtk_cell_renderer_toggle_new();
        g_object_set(enabledRenderer, "activatable", TRUE, NULL);
        g_signal_connect(enabledRenderer,
                         "toggled",
                         G_CALLBACK(source_enabled_toggle_cb),
                         prefs);

        gtk_tree_view_insert_column_with_attributes(view,
                                                    2,
                                                    "",
                                                    enabledRenderer,
                                                    "active", COLUMN_ENABLED,
                                                    NULL);
        enabledCol = gtk_tree_view_get_column(view, 2);
        gtk_tree_view_column_set_expand(enabledCol, FALSE);

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
                                        indexer_view_refresh(prefs->properties);
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
        gboolean system;
        GtkTreeIter iter;
        struct preferences_catalog *prefs = (struct preferences_catalog *)userdata;
        g_return_if_fail(prefs);

        update_properties(prefs);

        if(gtk_tree_selection_get_selected(selection, NULL/*model_ptr*/, &iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(prefs->model), &iter,
                                   COLUMN_IS_SYSTEM, &system,
                                   -1);
                gtk_widget_set_sensitive(prefs->delete_button, !system);
                gtk_widget_set_sensitive(prefs->refresh_button, TRUE);
        } else {
                gtk_widget_set_sensitive(prefs->delete_button, FALSE);
                gtk_widget_set_sensitive(prefs->refresh_button, FALSE);
        }
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

static void register_new_source(struct preferences_catalog  *prefs, struct indexer  *indexer, struct indexer_source  *source)
{
        GtkTreeIter source_iter;
        add_source(GTK_LIST_STORE(prefs->model),
                   indexer,
                   prefs->catalog,
                   source->id,
                   &source_iter);

        select_row(prefs, &source_iter);

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
                register_new_source(prefs, indexer, source);
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
        prefs->delete_button = del;
        gtk_widget_set_sensitive(del, FALSE);

        reload =  gtk_button_new_from_stock(GTK_STOCK_REFRESH);
        gtk_widget_show(reload);
        gtk_container_add(GTK_CONTAINER(buttons), reload);
        g_signal_connect(reload,
                         "clicked",
                         G_CALLBACK(reload_cb),
                         prefs/*userdata*/);
        prefs->refresh_button = reload;
        gtk_widget_set_sensitive(reload, FALSE);


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
        gtk_container_set_border_width(GTK_CONTAINER(twopanes), 12);

        return twopanes;
}

static gint list_sort_cb(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata)
{
        gboolean a_system = FALSE, b_system = FALSE;

        gtk_tree_model_get(model, a,
                           COLUMN_IS_SYSTEM, &a_system,
                           -1);
        gtk_tree_model_get(model, b,
                           COLUMN_IS_SYSTEM, &b_system,
                           -1);

        if(a_system && !b_system) {
                return -1;
        } else if(!a_system && b_system) {
                return 1;
        }

        g_assert(a_system==b_system);
        if(a_system) {
                int a_id, b_id;

                /* sort system sources by source_id (from the oldest to the newest) */

                gtk_tree_model_get(model, a,
                                   COLUMN_SOURCE_ID, &a_id,
                                   -1);
                gtk_tree_model_get(model, b,
                                   COLUMN_SOURCE_ID, &b_id,
                                   -1);
                return a_id-b_id;
        } else {
                int retval;
                char *a_label, *b_label;

                /* sort user sources by label */
                gtk_tree_model_get(model, a,
                                   COLUMN_LABEL, &a_label,
                                   -1);
                gtk_tree_model_get(model, b,
                                   COLUMN_LABEL, &b_label,
                                   -1);


                retval = g_utf8_collate(a_label, b_label);

                g_free(a_label);
                g_free(b_label);
                return retval;
        }
}

static void source_enabled_toggle_cb(GtkCellRendererToggle *cell, gchar *path_string, gpointer userdata)
{
        struct preferences_catalog *prefs;
        gboolean enabled = TRUE;
        int source_id;
        GtkTreeIter iter;

        g_return_if_fail(path_string);
        g_return_if_fail(userdata);

        prefs = (struct preferences_catalog *)userdata;

        if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(prefs->model),
                                               &iter,
                                               path_string)) {
                gtk_tree_model_get(GTK_TREE_MODEL(prefs->model), &iter,
                                   COLUMN_ENABLED, &enabled,
                                   COLUMN_SOURCE_ID, &source_id,
                                   -1);
                enabled=!enabled;
                if(source_id>0
                   && catalog_source_set_enabled(prefs->catalog,
                                                 source_id,
                                                 enabled)) {
                        gtk_list_store_set(prefs->model, &iter,
                                           COLUMN_ENABLED, enabled,
                                           -1);
                }
        }
}

/**
 * Setup the list as a GTK drop source.
 *
 * @param prefs a preferences_catalog with all its fields initialized
 */
static void install_drop(struct preferences_catalog *prefs)
{
        gtk_drag_dest_set(GTK_WIDGET(prefs->view),
                          GTK_DEST_DEFAULT_ALL,
                          drop_targets,
                          drop_targets_len,
                          GDK_ACTION_COPY
                          | GDK_ACTION_MOVE
                          | GDK_ACTION_LINK
                          | GDK_ACTION_DEFAULT);
        g_signal_connect(prefs->view,
                         "drag_data_received",
                         G_CALLBACK(drop_cb),
                         prefs);
}

static void drop_cb(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer userdata)
{
        struct preferences_catalog *prefs;
        gboolean success = TRUE;
        int added=0;
        GSList *errors = NULL;

        g_return_if_fail(data!=NULL);
        g_return_if_fail(userdata!=NULL);

        prefs = (struct preferences_catalog *)userdata;


        if(data->length>=0 && data->format==8 ) {
                GString *buffer;
                gchar *iter_data;
                gint iter_length;

                buffer = g_string_new("");

                iter_data = (gchar *)data->data;
                iter_length = data->length;
                success=TRUE;
                while(parse_uri_list_next(&iter_data, &iter_length, buffer)) {
                        if(add_directory_uri(prefs, buffer->str)) {
                                added++;
                        } else {
                                errors=g_slist_append(errors,
                                                      g_strdup(buffer->str));
                                success=FALSE;
                        }

                }
                g_string_free(buffer, TRUE/*free content*/);

                if(errors!=NULL) {
                        display_dnd_errors(prefs, errors);
                        g_slist_foreach(errors, (GFunc)g_free, NULL/*userdata*/);
                        g_slist_free(errors);
                }
        }
        gtk_drag_finish (drag_context, success && added>0, FALSE, time);
}
static gboolean add_directory_uri(struct preferences_catalog *prefs, const gchar *uri)
{
        struct indexer **indexers = indexers_list();
        int i;

        for(i=0; indexers[i]!=NULL; i++) {
                GError *err = NULL;
                struct indexer_source *source;
                struct indexer *indexer = indexers[i];
                source = indexer_new_source_for_uri(indexer,
                                                    uri,
                                                    prefs->catalog,
                                                    &err);
                if(source!=NULL) {
                        register_new_source(prefs, indexer, source);
                        indexer_source_release(source);
                        return TRUE;
                }
                if(err!=NULL) {
                        g_error_free(err);
                }
        }
        return FALSE;
}
/**
 * Open a dialog box to report URLs that could not be used to
 * create new catalogs.
 */
static void display_dnd_errors(struct preferences_catalog *prefs, GSList *errors)
{
        GtkWidget *dialog;
        GString *buffer = g_string_new("");
        GSList *item;

        g_string_append(buffer, "No catalogs could be created for the following ");
        if(g_slist_length(errors)>1) {
                g_string_append(buffer, "URI's");
        } else {
                g_string_append(buffer, "URI");
        }
        g_string_append_c(buffer, '\n');

        for(item=errors; item!=NULL; item=g_slist_next(item)) {
                g_string_append_c(buffer, ' ');
                g_string_append(buffer, (char *)item->data);
                g_string_append_c(buffer, '\n');
        }
        g_string_append(buffer, "\nOcha can currently only index local directories, not files or SMB shares.");

        dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(prefs->widget)),
                                        0/*flags*/,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        buffer->str);
        g_signal_connect_swapped (dialog,
                                  "response",
                                  G_CALLBACK (gtk_widget_destroy),
                                  dialog);
        gtk_widget_show(dialog);

        g_string_free(buffer, TRUE/*with content*/);
}
