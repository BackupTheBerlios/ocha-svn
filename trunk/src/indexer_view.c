#include "indexer_view.h"
#include <gtk/gtk.h>
#include <string.h>
#include "content_view.h"

/** \file implementation of the API defined in indexer_view.h */

/**
 * Definition of the abstract indexer view structure.
 * Everything except the catalog can be null, as
 * indexer_view_new allocates only the structure.
 */
struct indexer_view
{
        struct catalog *catalog;
        /** Widget returned to the caller */
        GtkWidget *root_widget;
        /** gtk_container to add/remove properties_widget from */
        GtkWidget *properties_widget_container;
        /** widget that represents the view of the current source or indexer */
        GtkWidget *properties_widget;

        struct indexer *current_indexer;
        /** id of current source or -1 (no source) */
        int current_source_id;
        struct indexer_source_view *current_view;

        struct content_view *content;
};

/* ------------------------- prototypes */
static void init_widgets(struct indexer_view *view);
static GtkWidget *properties_widget_from_source(struct indexer_view *view, struct indexer *indexer, struct indexer_source *source);
static GtkWidget *properties_widget_from_indexer(struct indexer *indexer);

/* ------------------------- public functions */
struct indexer_view *indexer_view_new(struct catalog *catalog)
{
        struct indexer_view *view;

        g_return_val_if_fail(catalog!=NULL, NULL);

        view =  g_new(struct indexer_view, 1);
        memset(view, 0, sizeof(struct indexer_view));
        view->catalog=catalog;
        view->current_source_id=-1;
        view->current_view=NULL;
        view->content=content_view_new(catalog);
        init_widgets(view);
        return view;
}

GtkWidget *indexer_view_widget(struct indexer_view *view)
{
        g_return_val_if_fail(view, NULL);
        return view->root_widget;
}

void indexer_view_destroy(struct indexer_view *view)
{
        g_return_if_fail(view);

        if(view->current_view) {
                indexer_source_view_release(view->current_view);
        }

        indexer_view_detach(view);

        if(view->root_widget) {
                gtk_widget_destroy(view->properties_widget_container);
        }
        content_view_destroy(view->content);
        g_free(view);
}
void indexer_view_attach_indexer(struct indexer_view *view,
                                 struct indexer *indexer)
{
        g_return_if_fail(view);
        g_return_if_fail(indexer);
        indexer_view_attach_source(view, indexer, NULL);
}

void indexer_view_attach_source(struct indexer_view *view,
                                struct indexer *indexer,
                                struct indexer_source *source)
{
        gboolean issame;

        g_return_if_fail(view);
        g_return_if_fail(indexer);

        issame = view->current_indexer==indexer
                && ( ( source==NULL && view->current_source_id==-1)
                     || ( source!=NULL && source->id==view->current_source_id));
        if(!issame)
        {
                GtkWidget *widget;

                if(view->current_indexer!=NULL) {
                        indexer_view_detach(view);
                }

                g_return_if_fail(view->properties_widget_container!=NULL);
                g_return_if_fail(view->current_indexer==NULL);
                g_return_if_fail(view->properties_widget==NULL);

                view->current_indexer=indexer;
                if(source) {
                        view->current_source_id=source->id;
                        widget=properties_widget_from_source(view, indexer, source);
                } else {
                        view->current_source_id=-1;
                        widget=properties_widget_from_indexer(indexer);
                }
                view->properties_widget=widget;

                gtk_container_add(GTK_CONTAINER(view->properties_widget_container),
                                  widget);

                if(source) {
                        content_view_attach(view->content, source->id);
                } else {
                        content_view_detach(view->content);
                }
        }
}
void indexer_view_detach(struct indexer_view *view)
{
        g_return_if_fail(view);

        if(view->properties_widget) {
                gtk_container_remove(GTK_CONTAINER(view->properties_widget_container),
                                     view->properties_widget);
        }
        view->properties_widget=NULL;
        view->current_indexer=NULL;
        view->current_source_id=-1;

        content_view_detach(view->content);
}

void indexer_view_refresh(struct indexer_view *view)
{
        g_return_if_fail(view);
        content_view_refresh(view->content);
}
/* ------------------------- static functions */
static void init_widgets(struct indexer_view *view)
{
        GtkWidget *notebook;
        GtkWidget *location;
        GtkWidget *contentlist;
        int i;
        static const char *notebook_labels[] = {"Location", "Contents", NULL };

        g_return_if_fail(view);
        g_return_if_fail(view->properties_widget_container==NULL);

        notebook =  gtk_notebook_new ();
        view->root_widget = notebook;
        gtk_widget_show(notebook);

        /* location is a one-row vbox that's only there
         * so that I don't have to worry about packing order
         * when manipulation properties_widget and
         * properties_widget_container; container_add/remove
         * are enough
         */
        location =  gtk_vbox_new(FALSE/*not homogenous*/,
                                 0/*padding*/);
        gtk_widget_show (location);
        gtk_container_add(GTK_CONTAINER(notebook), location);

        view->properties_widget_container=location;

        contentlist =  content_view_get_widget(view->content);
        gtk_widget_show(contentlist);
        gtk_container_add(GTK_CONTAINER(notebook), contentlist);

        for(i=0; notebook_labels[i]; i++) {
                GtkWidget *label = gtk_label_new(notebook_labels[i]);
                gtk_widget_show(label);
                gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook),
                                           gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook),
                                                                     i),
                                           label);
        }
}

static GtkWidget *properties_widget_from_source(struct indexer_view *view,
                                                struct indexer *indexer,
                                                struct indexer_source *source)
{
        GtkWidget *widget;

        g_return_val_if_fail(indexer!=NULL, NULL);
        g_return_val_if_fail(source!=NULL, NULL);

        if(view->current_view) {
                if(view->current_view->indexer!=indexer) {
                        indexer_source_view_release(view->current_view);
                        view->current_view=NULL;
                }
        }
        if(!view->current_view) {
                view->current_view=indexer_new_view(indexer);
        }

        indexer_source_view_attach(view->current_view, source);
        widget = view->current_view->widget;
        gtk_widget_show(widget);
        return widget;
}

static GtkWidget *properties_widget_from_indexer(struct indexer *indexer)
{
        char *text;
        GtkWidget *label;
        GtkWidget *align;

        g_return_val_if_fail(indexer!=NULL, NULL);


        text = g_strdup_printf("<span size='x-large'><b>%s</b></span>\n\n"
                               "<span size='large'>%s</span>",
                               indexer->display_name,
                               indexer->description);

        label =  gtk_label_new(text);
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_FILL);
        gtk_label_set_selectable(GTK_LABEL(label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(label),
                               0.0,
                               0.0);
        g_free(text);
        gtk_widget_show(label);

        align =  gtk_alignment_new(0.0/*xalign*/,
                                   0.5/*yalign*/,
                                   0.6/*xscale*/,
                                   1.0/*yscale*/);
        gtk_alignment_set_padding(GTK_ALIGNMENT(align),
                                  12/*top*/,
                                  0/*bottom*/,
                                  12/*right*/,
                                  6/*left*/);
        gtk_widget_show(align);
        gtk_container_add(GTK_CONTAINER(align),
                          label);
        return align;
}
