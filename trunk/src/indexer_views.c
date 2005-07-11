#include "indexer_views.h"
#include "indexer_view.h"
#include "catalog.h"
#include <gtk/gtk.h>

/** \file Implement the API defined in indexer_views.h
 *
 */

struct indexer_views_item
{
        struct indexer_views *views;
        struct indexer *indexer;
        GtkWidget *window;
        GtkWidget *view_widget;
        struct indexer_view *view;
        int source_id;
        guint display_name_notify;
};

struct indexer_views
{
        struct catalog *catalog;
        struct indexer_views_item *item;
};

/* ------------------------- prototypes: static functions */
static void close_cb(GtkButton *button, gpointer userdata);
static gboolean destroy_event_cb(GtkWidget *widget, GdkEvent *event, gpointer userdata);
static void update_display_name(struct indexer_views_item *item, struct indexer_source *source);
static void display_name_change_cb(struct indexer_source *source, gpointer userdata);
static struct indexer_views_item *indexer_views_item_new(struct indexer_views *views, struct indexer *indexer, struct indexer_source *source);
static void indexer_views_item_init(struct indexer_views *views, struct indexer_views_item *item);
static void indexer_views_item_free(struct indexer_views_item *item);
static void indexer_views_item_refresh(struct indexer_views_item  *item, struct indexer_source  *source);
static void indexer_views_item_set_source(struct indexer_views_item  *item, struct indexer  *indexer, struct indexer_source  *source);

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


        if(views->item) {
                indexer_views_item_free(views->item);
                views->item=NULL;
        }

        g_free(views);
}

void indexer_views_refresh_source(struct indexer_views *views, struct indexer  *indexer, struct indexer_source  *source)
{
        g_return_if_fail(views);
        g_return_if_fail(source);

        if(views->item!=NULL) {
                indexer_views_item_refresh(views->item, source);
        }
}

void indexer_views_delete_source(struct indexer_views *views, struct indexer  *indexer, struct indexer_source  *source)
{
        struct indexer_views_item *item;
        g_return_if_fail(views);
        g_return_if_fail(source);

        item=views->item;

        if(item!=NULL && source->id==item->source_id) {
                indexer_views_item_free(item);
                views->item=NULL;
        }
}


void indexer_views_open(struct indexer_views  *views, struct indexer  *indexer, struct indexer_source  *source)
{
        g_return_if_fail(views);
        g_return_if_fail(indexer);
        g_return_if_fail(source);

        if(views->item) {
                indexer_views_item_free(views->item);
                views->item=NULL;
        }
        views->item=indexer_views_item_new(views, indexer, source);
}


/* ------------------------- static functions */

static void close_cb(GtkButton *button, gpointer userdata)
{
        struct indexer_views_item *item;

        g_return_if_fail(userdata);

        item=(struct indexer_views_item *)userdata;

        if(item->views->item==item) {
                item->views->item=NULL;
        }
        indexer_views_item_free(item);
}

static gboolean destroy_event_cb(GtkWidget *widget, GdkEvent *event, gpointer userdata)
{
        struct indexer_views_item *item;

        g_return_val_if_fail(userdata, TRUE);

        item=(struct indexer_views_item *)userdata;
        if(item->views->item==item) {
                item->views->item=NULL;
        }
        indexer_views_item_free(item);

        return TRUE/*handled, don't propagate*/;
}

static void update_display_name(struct indexer_views_item *item, struct indexer_source *source)
{
        g_return_if_fail(item);
        g_return_if_fail(source);

        if(item->window!=NULL) {
                gtk_window_set_title(GTK_WINDOW(item->window),
                                     source->display_name);
        }
}

static void display_name_change_cb(struct indexer_source *source, gpointer userdata)
{
        struct indexer_views_item *item;
        g_return_if_fail(source);
        g_return_if_fail(userdata);

        item = (struct indexer_views_item *)userdata;

        update_display_name(item, source);
}

static struct indexer_views_item *indexer_views_item_new(struct indexer_views *views, struct indexer *indexer, struct indexer_source *source)
{
        struct indexer_views_item *item;
        g_return_val_if_fail(views, NULL);
        g_return_val_if_fail(source, NULL);

        item = g_new(struct indexer_views_item, 1);
        indexer_views_item_init(views, item);
        indexer_views_item_set_source(item, indexer, source);
        return item;
}

static void indexer_views_item_init(struct indexer_views *views, struct indexer_views_item *item)
{
        g_return_if_fail(views);
        g_return_if_fail(item);

        memset(item, 0, sizeof(struct indexer_views_item));
        item->views = views;
}

static void indexer_views_item_free(struct indexer_views_item *item)
{
        struct indexer_source *source;
        g_return_if_fail(item);

        gtk_widget_hide(GTK_WIDGET(item->window));

        source = indexer_load_source(item->indexer,
                                     item->views->catalog,
                                     item->source_id);
        if(source) {
                indexer_source_remove_notification(source,
                                                   item->display_name_notify);
                indexer_source_release(source);
        }

        if(item->view!=NULL) {
                indexer_view_destroy(item->view);
                item->view=NULL;
                item->view_widget=NULL;
        }
        gtk_widget_destroy(GTK_WIDGET(item->window));
        item->source_id=-1;

        g_free(item);
}

static void indexer_views_item_refresh(struct indexer_views_item  *item, struct indexer_source  *source)
{
        if(item->view!=NULL && source->id==item->source_id) {
                indexer_view_refresh(item->view);
        }
}


static void indexer_views_item_set_source(struct indexer_views_item  *item, struct indexer  *indexer, struct indexer_source  *source)
{
        GtkWidget *window;
        GtkWidget *vbox;
        GtkWidget *close;
        GtkWidget *buttons;
        struct indexer_view *view;

        window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
        view=indexer_view_new(item->views->catalog);

        item->view=view;
        item->window=window;
        item->view_widget=indexer_view_widget(view);

        vbox=gtk_vbox_new(FALSE/*not homogenous*/, 12/*spacing*/);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
        gtk_widget_show(vbox);

        gtk_container_add(GTK_CONTAINER(window),
                          vbox);

        gtk_box_pack_start(GTK_BOX(vbox),
                           item->view_widget,
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
                         "delete-event",
                         G_CALLBACK(destroy_event_cb),
                         item);
        g_signal_connect(close,
                         "clicked",
                         G_CALLBACK(close_cb),
                         item);
        item->source_id=-1;

        item->indexer=indexer;
        item->source_id=source->id;
        indexer_view_attach_source(item->view, indexer, source);
        update_display_name(item, source);

        item->display_name_notify = indexer_source_notify_display_name_change(source,
                                                                              item->views->catalog,
                                                                              display_name_change_cb,
                                                                              item);


        gtk_window_present(GTK_WINDOW(item->window));
}
