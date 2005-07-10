#include "indexer_views.h"
#include "indexer_view.h"
#include "catalog.h"
#include <gtk/gtk.h>

/** \file Implement the API defined in indexer_views.h
 *
 */

struct indexer_views
{
        struct catalog *catalog;
        GtkWidget *window;
        GtkWidget *view_widget;
        struct indexer_view *view;
        int source_id;
        guint display_name_notify;
};

/* ------------------------- prototypes: static functions */
static void close(struct indexer_views  *views);
static void close_cb(GtkButton *button, gpointer userdata);
static void destroy_cb(GtkWidget *widget, gpointer userdata);
static void update_display_name(struct indexer_views *views, struct indexer_source *source);
static void display_name_change_cb(struct indexer_source *source, gpointer userdata);
/* ------------------------- definitions */

/* ------------------------- public functions */
struct indexer_views *indexer_views_new(struct catalog *catalog)
{
        struct indexer_views *views;

        views = g_new(struct indexer_views, 1);
        memset(views, 0, sizeof(struct indexer_views));
        views->catalog=catalog;
        return views;
}

void indexer_views_free(struct indexer_views *views)
{
        g_return_if_fail(views!=NULL);

        if(views->window!=NULL) {
                gtk_widget_destroy(GTK_WIDGET(views->window));
        }
        if(views->view!=NULL) {
                indexer_view_destroy(views->view);
                views->view=NULL;
                views->view_widget=NULL;
        }

        g_free(views);
}

void indexer_views_refresh_source(struct indexer_views *views, struct indexer  *indexer, struct indexer_source  *source)
{
        g_return_if_fail(views);
        g_return_if_fail(source);

        if(views->view!=NULL && source->id==views->source_id) {
                indexer_view_refresh(views->view);
        }
}

void indexer_views_delete_source(struct indexer_views *views, struct indexer  *indexer, struct indexer_source  *source)
{
        g_return_if_fail(views);
        g_return_if_fail(source);

        if(views->view!=NULL && source->id==views->source_id) {
                close(views);
        }
}

void indexer_views_open(struct indexer_views  *views, struct indexer  *indexer, struct indexer_source  *source)
{
        g_return_if_fail(views);
        g_return_if_fail(indexer);
        g_return_if_fail(source);

        if(views->view==NULL) {
                GtkWidget *window;
                GtkWidget *vbox;
                GtkWidget *close;
                GtkWidget *buttons;
                struct indexer_view *view;

                window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
                view=indexer_view_new(views->catalog);

                views->view=view;
                views->window=window;
                views->view_widget=indexer_view_widget(view);

                vbox=gtk_vbox_new(FALSE/*not homogenous*/, 12/*spacing*/);
                gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
                gtk_widget_show(vbox);

                gtk_container_add(GTK_CONTAINER(window),
                                  vbox);

                gtk_box_pack_start(GTK_BOX(vbox),
                                   views->view_widget,
                                   TRUE/*expand*/,
                                   TRUE/*fill*/,
                                   0/*padding*/);

                close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
                gtk_widget_show (close);

                buttons = gtk_hbutton_box_new();
                gtk_widget_show(buttons);
                gtk_button_box_set_layout (GTK_BUTTON_BOX (buttons),
                                           GTK_BUTTONBOX_END);
                gtk_container_add(GTK_CONTAINER(buttons),
                                  close);
                gtk_box_pack_end(GTK_BOX(vbox),
                                 buttons,
                                 FALSE/*not expand*/,
                                 TRUE/*fill*/,
                                 0/*padding*/);




                g_signal_connect(window,
                                 "destroy",
                                 G_CALLBACK(destroy_cb),
                                 views);
                g_signal_connect(close,
                                 "clicked",
                                 G_CALLBACK(close_cb),
                                 views);
                views->source_id=-1;
        }
        if(views->source_id!=source->id) {
                if(views->source_id!=-1) {
                        indexer_source_remove_notification(source,
                                                           views->display_name_notify);
                        views->source_id=-1;
                }

                views->source_id=source->id;
                indexer_view_attach_source(views->view, indexer, source);
                views->display_name_notify = indexer_source_notify_display_name_change(source,
                                                                          views->catalog,
                                                                          display_name_change_cb,
                                                                          views);
                update_display_name(views, source);
        }
        gtk_window_present(GTK_WINDOW(views->window));
}


/* ------------------------- static functions */

static void close(struct indexer_views  *views)
{
        gtk_widget_hide(views->window);
        gtk_widget_destroy(views->window);
}

static void close_cb(GtkButton *button, gpointer userdata)
{
        struct indexer_views *views;
        g_return_if_fail(userdata);
        views=(struct indexer_views *)userdata;


        if(views->window!=NULL) {
                close(views);
        }
}

static void destroy_cb(GtkWidget *widget, gpointer userdata)
{
        struct indexer_views *views;

        g_return_if_fail(userdata);

        views=(struct indexer_views *)userdata;
        indexer_view_destroy(views->view);
        views->view=NULL;
        views->window=NULL;
        views->view_widget=NULL;
        views->source_id=-1;
}

static void update_display_name(struct indexer_views *views, struct indexer_source *source)
{
        g_return_if_fail(views);
        g_return_if_fail(source);

        if(views->window!=NULL) {
                gtk_window_set_title(GTK_WINDOW(views->window),
                                     source->display_name);
        }
}

static void display_name_change_cb(struct indexer_source *source, gpointer userdata)
{
        struct indexer_views *views;
        g_return_if_fail(source);
        g_return_if_fail(userdata);

        views = (struct indexer_views *)userdata;

        update_display_name(views, source);
}
