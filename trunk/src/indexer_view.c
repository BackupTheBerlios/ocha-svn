#include "indexer_view.h"
#include <gtk/gtk.h>
#include <string.h>

/** \file implementation of the API defined in indexer_view.h */

/**
 * Definition of the abstract indexer view structure.
 * Everything except the catalog can be null, as
 * indexer_view_new allocates only the structure.
 */
struct indexer_view
{
    struct catalog *catalog;
    GtkWidget *window;
    /** gtk_container to add/remove properties_widget from */
    GtkWidget *properties_widget_container;
    /** widget that represents the view of the current source or indexer */
    GtkWidget *properties_widget;
    struct indexer_source *current_source;
    struct indexer *current_indexer;
};

static void properties_window_destroy_cb(GtkWidget *widget, gpointer userdata);
static void create_window(struct indexer_view *view);
static GtkWidget *properties_widget_from_source(struct indexer *indexer, struct indexer_source *source, struct catalog *catalog);
static GtkWidget *properties_widget_from_indexer(struct indexer *indexer);


/* ------------------------- public functions */
struct indexer_view *indexer_view_new(struct catalog *catalog)
{
    g_return_val_if_fail(catalog!=NULL, NULL);

    struct indexer_view *view = (struct indexer_view *)
        g_new(struct indexer_view, 1);
    memset(view, 0, sizeof(struct indexer_view));
    view->catalog=catalog;
    return view;
}
void indexer_view_destroy(struct indexer_view *view)
{
    g_return_if_fail(view);

    if(view->window)
        gtk_widget_destroy(view->window);
    g_assert(view->window==NULL);
    g_assert(view->properties_widget==NULL);
    g_assert(view->current_source==NULL);
    g_assert(view->current_indexer==NULL);
    g_free(view);
}
void indexer_view_attach_indexer(struct indexer_view *view,
                                 struct indexer *indexer)
{
    g_return_if_fail(view);
    g_return_if_fail(indexer);
    indexer_view_attach_source(view, indexer, -1);
}
void indexer_view_attach_source(struct indexer_view *view,
                                struct indexer *indexer,
                                int source_id)
{
    g_return_if_fail(view);
    g_return_if_fail(indexer);

    gboolean issame = view->current_indexer==indexer
        && ( (source_id==-1 && view->current_source==NULL)
             || (view->current_source!=NULL && view->current_source->id==source_id));

    if(!issame)
    {
        if(view->current_indexer!=NULL)
            indexer_view_detach(view);
        if(!view->window)
            create_window(view);

        struct indexer_source *source=NULL;
        if(source_id!=-1)
        {
            source=indexer->load_source(indexer, view->catalog, source_id);
            if(!source)
                return;
        }

        GtkWidget *widget;
        const char *title;

        g_return_if_fail(view->current_indexer==NULL);
        g_return_if_fail(view->current_source==NULL);
        g_return_if_fail(view->properties_widget==NULL);
        g_return_if_fail(view->window!=NULL);

        view->current_indexer=indexer;
        if(source)
        {
            title=source->display_name;
            widget=properties_widget_from_source(indexer, source, view->catalog);
        }
        else
        {
            widget=properties_widget_from_indexer(indexer);
            title=indexer->display_name;
        }
        view->properties_widget=widget;

        GtkWidget *window = view->window;
        gtk_container_add(GTK_CONTAINER(view->properties_widget_container),
                          widget);
        gtk_window_set_title(GTK_WINDOW(window),
                             title);
        if(source)
            source->release(source);
    }

    gtk_window_present(GTK_WINDOW(view->window));
}
void indexer_view_detach(struct indexer_view *view)
{
    g_return_if_fail(view);

    if(view->window)
        {
            if(view->properties_widget)
                gtk_container_remove(GTK_CONTAINER(view->properties_widget_container),
                                     view->properties_widget);
            gtk_window_set_title(GTK_WINDOW(view->window),
                                 "");
        }
    view->properties_widget=NULL;
    if(view->current_source)
        {
            view->current_source->release(view->current_source);
            view->current_source=NULL;
        }
    view->current_indexer=NULL;
}

/* ------------------------- private functions */
static void create_window(struct indexer_view *view)
{
    g_return_if_fail(view);
    g_return_if_fail(view->window==NULL);

    GtkWidget  *window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    view->window=window;
    g_signal_connect(window,
                     "destroy",
                     G_CALLBACK(properties_window_destroy_cb),
                     view);

    GtkWidget *notebook = gtk_notebook_new ();
    gtk_widget_show(notebook);
    gtk_container_add(GTK_CONTAINER(window), notebook);

    /* location is a one-row vbox that's only there
     * so that I don't have to worry about packing order
     * when manipulation properties_widget and
     * properties_widget_container; container_add/remove
     * are enough
     */
    GtkWidget *location = gtk_vbox_new(FALSE/*not homogenous*/,
                                       0/*padding*/);
    gtk_widget_show (location);
    gtk_container_add(GTK_CONTAINER(notebook), location);

    view->properties_widget_container=location;

    GtkWidget *contentlist = gtk_label_new("Content\nList");
    gtk_widget_show(contentlist);
    gtk_container_add(GTK_CONTAINER(notebook), contentlist);

    const char *notebook_labels[] = {"Location", "Contents", NULL};
    for(int i=0; notebook_labels[i]; i++)
    {
        GtkWidget *label = gtk_label_new(notebook_labels[i]);
        gtk_widget_show(label);
        gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook),
                                   gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i),
                                   label);
    }
}

static void properties_window_destroy_cb(GtkWidget *widget, gpointer userdata)
{
    struct indexer_view *view = (struct indexer_view *)userdata;
    g_return_if_fail(view);

    indexer_view_detach(view);
    view->window=NULL;
    g_assert(!view->properties_widget);
    g_assert(!view->current_indexer);
    g_assert(!view->current_source);
}

static GtkWidget *properties_widget_from_source(struct indexer *indexer, struct indexer_source *source, struct catalog *catalog)
{
    g_return_val_if_fail(indexer!=NULL, NULL);
    g_return_val_if_fail(source!=NULL, NULL);

    GtkWidget *widget = source->editor_widget(source, catalog);
    gtk_widget_show(widget);
    return widget;
}

static GtkWidget *properties_widget_from_indexer(struct indexer *indexer)
{
    g_return_val_if_fail(indexer!=NULL, NULL);

    char *text = g_strdup_printf("<b>%s</b>\n\n%s",
                                 indexer->display_name,
                                 indexer->description);

    GtkWidget *retval = gtk_label_new(text);
    gtk_label_set_use_markup(GTK_LABEL(retval), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(retval), TRUE);
    gtk_label_set_justify(GTK_LABEL(retval), GTK_JUSTIFY_FILL);
    gtk_label_set_selectable(GTK_LABEL(retval), TRUE);

    g_free(text);
    gtk_widget_show(retval);
    return retval;
}
