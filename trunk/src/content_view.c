#include "content_view.h"

/** \file Implementation of the API defined in content_view.h
 *
 */
#include "contentlist.h"
#include <string.h>

/**
 * The content view structure.
 */
struct content_view
{
        /** a connection to the catalog */
        struct catalog *catalog;

        /** This view's main widget */
        GtkWidget *widget;

        /** Whether this view is attached to anything */
        gboolean attached;

        /** The source this view is attached to if attached is true */
        int source_id;

        /** The tree view (may be null, wether or not attached is true) */
        GtkTreeView *treeview;

        /** The tree model, only if attached (but not always) */
        ContentList *treemodel;

};

struct fill_contentlist_userdata
{
        ContentList *contentlist;
        int index;
        int size;
};
/* ------------------------- prototypes: static functions */
static void init_widget(struct content_view *view);
static ContentList *run_query(struct content_view *view);
static gboolean fill_contentlist_cb(struct catalog *catalog,
                                    const struct catalog_query_result *result,
                                    void *userdata);
static void cell_data_func(GtkTreeViewColumn* col,
                           GtkCellRenderer* renderer,
                           GtkTreeModel* model,
                           GtkTreeIter* iter,
                           gpointer userdata);
static void attach_unconditionally(struct content_view *view, int source_id);
static void entry_enabled_toggle_cb(GtkCellRendererToggle *cell, gchar *path_string, gpointer userdata);

/* ------------------------- definitions */

/* ------------------------- public functions */
struct content_view *content_view_new(struct catalog *catalog)
{
        struct content_view *retval;

        g_return_val_if_fail(catalog, NULL);

        retval = g_new(struct content_view, 1);
        memset(retval, sizeof(struct content_view), 0);

        retval->catalog=catalog;
        init_widget(retval);
        return retval;
}

void content_view_destroy(struct content_view *view)
{
        g_return_if_fail(view);
        g_free(view);
}

void content_view_attach(struct content_view *view, int source_id)
{
        g_return_if_fail(view);
        g_return_if_fail(source_id>0);

        if(view->attached && view->source_id==source_id) {
                return;
        }
        attach_unconditionally(view, source_id);
}
void content_view_refresh(struct content_view *view)
{
        g_return_if_fail(view);
        if(!view->attached) {
                return;
        }
        attach_unconditionally(view, view->source_id);
}

void content_view_detach(struct content_view *view)
{
        g_return_if_fail(view);
        if(!view->attached) {
                return;
        }
        if(view->treemodel) {
                gtk_tree_view_set_model(view->treeview, NULL);
                view->treemodel=NULL;
        }
        gtk_widget_hide(view->widget);
        view->attached=FALSE;
}

GtkWidget *content_view_get_widget(struct content_view *view)
{
        g_return_val_if_fail(view, NULL);

        return view->widget;
}

/* ------------------------- static functions */

static void init_widget(struct content_view *view)
{
        GtkWidget *scroll;
        GtkWidget *treeview;
        GtkTreeViewColumn *col;
        GtkCellRenderer *renderer;
        GtkCellRenderer *enabledRenderer;

        scroll = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);

        treeview = gtk_tree_view_new();
        gtk_widget_show (treeview);
        gtk_container_add (GTK_CONTAINER (scroll),
                           treeview);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);

        /* disable/enable */
        enabledRenderer = gtk_cell_renderer_toggle_new();
        g_object_set(enabledRenderer, "activatable", TRUE, NULL);
        g_signal_connect(enabledRenderer,
                         "toggled",
                         G_CALLBACK(entry_enabled_toggle_cb),
                         view);

        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
                                                    0,
                                                    "Enabled",
                                                    enabledRenderer,
                                                    "active", CONTENTLIST_COL_ENABLED,
                                                    NULL);

        /* name/long_name */
        col = gtk_tree_view_column_new();
        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(col,
                                        renderer,
                                        TRUE/*expand*/);

        gtk_tree_view_column_set_cell_data_func(col,
                                                renderer,
                                                cell_data_func,
                                                view/*userdata*/,
                                                NULL/*destroy data*/);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);


        view->treeview=GTK_TREE_VIEW(treeview);
        view->widget=scroll;
}

static ContentList *run_query(struct content_view *view)
{
        ContentList *retval;
        struct catalog *catalog = view->catalog;
        int source_id = view->source_id;
        guint count = 0;
        struct fill_contentlist_userdata userdata;

        catalog_get_source_content_count(catalog, source_id, &count);
        if(count==0) {
                return NULL;
        }

        retval = contentlist_new(count);
        userdata.contentlist = retval;
        userdata.index=0;
        userdata.size=count;
        catalog_get_source_content(catalog,
                                   source_id,
                                   fill_contentlist_cb,
                                   &userdata);
        return retval;
}

static gboolean fill_contentlist_cb(struct catalog *catalog,
                                    const struct catalog_query_result *result,
                                    void *_userdata)
{
        const struct catalog_entry *entry = &result->entry;
        struct fill_contentlist_userdata *userdata;

        g_return_val_if_fail(_userdata, FALSE);

        userdata = (struct fill_contentlist_userdata *)_userdata;
        contentlist_set(userdata->contentlist,
                        userdata->index,
                        result->id,
                        entry->name,
                        entry->long_name,
                        result->enabled);
        userdata->index++;
        return userdata->index<userdata->size;
}


static void cell_data_func(GtkTreeViewColumn* col,
                           GtkCellRenderer* renderer,
                           GtkTreeModel* model,
                           GtkTreeIter* iter,
                           gpointer userdata)
{
        char *name;
        char *long_name;
        if(contentlist_get_at_iter(CONTENTLIST(model),
                                   iter,
                                   NULL/*id_out*/,
                                   &name/*name_out*/,
                                   &long_name/*long_name_out*/, NULL/*enabled_out*/))
        {
                char *markup;
                markup = g_markup_printf_escaped("<big><b>%s</b></big>\n"
                                                 "<small>%s</small>",
                                                 name,
                                                 long_name);
                g_object_set(renderer,
                             "markup",
                             markup,
                             NULL);
                g_free(markup);
        }
        else
        {
                g_object_set(renderer,
                             "markup",
                             "...",
                             NULL);
        }
}

static void attach_unconditionally(struct content_view *view, int source_id)
{
        if(view->attached) {
                content_view_detach(view);
        }
        g_assert(!view->attached);

        view->source_id=source_id;
        view->attached=TRUE;
        view->treemodel=run_query(view);
        if(view->treemodel) {
                gtk_tree_view_set_model(view->treeview,
                                        GTK_TREE_MODEL(view->treemodel));
        }
        gtk_widget_show(view->widget);
}

static void entry_enabled_toggle_cb(GtkCellRendererToggle *cell, gchar *path_string, gpointer userdata)
{
        struct content_view *view;
        gboolean enabled = TRUE;
        int entry_id;
        GtkTreeIter iter;

        g_return_if_fail(path_string);
        g_return_if_fail(userdata);

        view = (struct content_view *)userdata;

        if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(view->treemodel),
                                               &iter,
                                               path_string)) {
                if(contentlist_get_at_iter(CONTENTLIST(view->treemodel),
                                           &iter,
                                           &entry_id,
                                           NULL/*name*/,
                                           NULL/*long_name*/,
                                           &enabled)) {
                        enabled=!enabled;
                        catalog_entry_set_enabled(view->catalog,
                                                  entry_id,
                                                  enabled);
                        contentlist_set_enabled_at_iter(CONTENTLIST(view->treemodel),
                                                        &iter,
                                                        enabled);
                }
        }
}
