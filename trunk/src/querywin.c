#include "querywin.h"
#include "resultlist.h"
#include "string_utils.h"
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>

/** \file
 * Create a query window and initialize the querwin structure.
 */

static GtkWidget *querywin;
static GtkWidget *query_label;
static GtkWidget *treeview;
static GString* query_str;
static GString* running_query;
static guint32 last_keypress;
#define query_label_text_len 256
static char query_label_text[256];
gboolean shown;
static struct result_queue *result_queue;
static struct queryrunner *queryrunner;

#define QUERY_TIMEOUT 3000
#define assert_initialized() g_return_if_fail(result_queue)
#define assert_queryrunner_set() g_return_if_fail(queryrunner);

/* ------------------------- prototypes */
static gboolean focus_out_cb(GtkWidget* widget, GdkEventFocus* ev, gpointer userdata);
static gboolean map_event_cb(GtkWidget *widget, GdkEvent *ev, gpointer userdata);
static void result_handler_cb(struct queryrunner *caller, const char *query, float pertinence, struct result *result, gpointer userdata);
static gboolean run_query(gpointer userdata);
static void set_query_string(void);
static void reset_query_string(void);
static gboolean key_release_event_cb(GtkWidget* widget, GdkEventKey *ev, gpointer userdata);
static void querywin_create(GtkWidget *list);
static gboolean execute_result(struct result *result);

/* ------------------------- public functions */

void querywin_init()
{
        query_str=g_string_new("");
        running_query=g_string_new("");
        result_queue=result_queue_new(NULL/*default context*/,
                                      result_handler_cb,
                                      NULL/*userdata*/);

        resultlist_init();
        querywin_create(resultlist_get_widget());

        gtk_window_stick(GTK_WINDOW(querywin));
        gtk_window_set_keep_above(GTK_WINDOW(querywin),
                                  TRUE);


        g_signal_connect(querywin,
                         "key_release_event",
                         G_CALLBACK(key_release_event_cb),
                         NULL/*data*/);

        g_signal_connect(querywin,
                         "focus_out_event",
                         G_CALLBACK(focus_out_cb),
                         NULL/*data*/);

        g_signal_connect(querywin,
                         "map_event",
                         G_CALLBACK(map_event_cb),
                         NULL/*data*/);


}

struct result_queue *querywin_get_result_queue()
{
        g_return_val_if_fail(result_queue, NULL);
        return result_queue;
}
void querywin_set_queryrunner(struct queryrunner *runner)
{
        assert_initialized();
        queryrunner=runner;
}
void querywin_start()
{
        assert_initialized();
        assert_queryrunner_set();

        if(shown)
                return;
        last_keypress=0;
        resultlist_verify();
        queryrunner->start(queryrunner);
        gtk_window_reshow_with_initial_size(GTK_WINDOW(querywin));
        shown=TRUE;
}
void querywin_stop()
{
        assert_initialized();
        assert_queryrunner_set();

        queryrunner->stop(queryrunner);
        gtk_widget_hide(querywin);
        shown=FALSE;

        reset_query_string();
}

/* ------------------------- static functions */

/**
 * Gtk callback for "focus-out".
 *
 * This stops the query and hides the window if the user
 * goes to another windew.
 */
static gboolean focus_out_cb(GtkWidget* widget, GdkEventFocus* ev, gpointer userdata)
{
        querywin_stop();
        return FALSE; /*propagate*/
}

/**
 * Gtk callback for "map-event".
 *
 * This makes sure the window is shown on the currentor the
 * desktop, even if it has been mapped previously on another
 * desktop.
 */
static gboolean map_event_cb(GtkWidget *widget, GdkEvent *ev, gpointer userdata)
{
        gtk_window_present(GTK_WINDOW(querywin));
        return FALSE/*go on*/;
}

/**
 * Result handler called by the query runner.
 *
 * This adds a new result into the result list
 */
static void result_handler_cb(struct queryrunner *caller,
                              const char *query,
                              float pertinence,
                              struct result *result,
                              gpointer userdata)
{
        g_return_if_fail(result);

        if(strcmp(running_query->str, query)==0) {
                resultlist_add_result(query, pertinence, result);
        } else {
                result->release(result);
        }
}

/**
 * Run the query (gtk timeout callback)
 */
static gboolean run_query(gpointer userdata)
{
        if(strcmp(running_query->str, query_str->str)!=0) {
                g_string_assign(running_query, query_str->str);
                strstrip_on_gstring(running_query);
                queryrunner->run_query(queryrunner, running_query->str);
        }
        return FALSE;
}

/**
 * Update the query string that's displayed on
 * the window, and set the timeout for actually
 * running the query if the user waits long enough
 * for it to be worth it.
 */
static void set_query_string()
{
        strncpy(query_label_text, query_str->str, query_label_text_len-1);
        gtk_label_set_text(GTK_LABEL(query_label), query_label_text);
        resultlist_set_current_query(query_str->str);
        g_timeout_add(300, run_query, NULL/*userdata*/);
}

/**
 * Empty the query string and the result list.
 * It's not quite the same as setting the query
 * to the empty string so use this one whenever you can.
 */
static void reset_query_string()
{
        g_string_assign(query_str, "");
        g_string_assign(running_query, "");
        *query_label_text='\0';
        gtk_label_set_text(GTK_LABEL(query_label), query_label_text);
        resultlist_clear();
}


/** Gtk callback for "release-event" */
static gboolean key_release_event_cb(GtkWidget* widget, GdkEventKey *ev, gpointer userdata)
{
        switch(ev->keyval) {
        case GDK_Escape:
                querywin_stop();
                return TRUE; /*handled*/

        case GDK_Delete:
        case GDK_BackSpace:
                if(query_str && query_str->len>0) {
                        g_string_truncate(query_str, query_str->len-1);
                        set_query_string();
                }
                last_keypress=ev->time;
                return TRUE; /*handled*/

        case GDK_Return:
        {
                struct result *selected = resultlist_get_selected();
                if(selected!=NULL) {
                        resultlist_executed(selected);
                        querywin_stop();
                        execute_result(selected);
                }
                return TRUE; /*handled*/
        }

        default:
                if(ev->string && ev->string[0]!='\0') {
                        if((ev->time-last_keypress)>QUERY_TIMEOUT)
                                g_string_assign(query_str, ev->string);
                        else
                                g_string_append(query_str, ev->string);
                        set_query_string();
                        last_keypress=ev->time;
                        return TRUE;/*handled*/
                }
                break;
        }
        return FALSE; /*propagate*/
}


/**
 * Create the gtk window.
 * @param list the tree view
 */
static void querywin_create(GtkWidget *list)
{
        GtkWidget *vbox1;
        GtkWidget *query;
        GtkWidget *scrolledwindow1;

        querywin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_widget_set_size_request (querywin, 320, 200);
        gtk_window_set_title (GTK_WINDOW (querywin), "Ocha Query");
        gtk_window_set_position (GTK_WINDOW (querywin), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_set_resizable (GTK_WINDOW (querywin), FALSE);
        gtk_window_set_decorated (GTK_WINDOW (querywin), FALSE);
        gtk_window_set_skip_taskbar_hint (GTK_WINDOW (querywin), TRUE);
        gtk_window_set_skip_pager_hint (GTK_WINDOW (querywin), TRUE);

        vbox1 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox1);
        gtk_container_add (GTK_CONTAINER (querywin), vbox1);

        query = gtk_label_new ("");
        gtk_widget_show (query);
        gtk_box_pack_start (GTK_BOX (vbox1), query, FALSE, FALSE, 0);

        scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
        gtk_widget_show (scrolledwindow1);
        gtk_box_pack_start (GTK_BOX (vbox1), scrolledwindow1, TRUE, TRUE, 0);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);

        gtk_container_add (GTK_CONTAINER (scrolledwindow1),
                           list);

        /* fill in structure */
        query_label=query;
        treeview=list;
}

static gboolean execute_result(struct result *result)
{
        GError *errs = NULL;
        if(!result->execute(result, &errs))
        {
                gdk_beep();
                fprintf(stderr,
                        "execution of %s failed: %s %s\n",
                        result->path,
                        errs->message,
                        errs->code==RESULT_ERROR_INVALID_RESULT
                        ? "(result was invalid)"
                        : "");
                g_error_free(errs);
                return FALSE;
        }
        return TRUE;
}

