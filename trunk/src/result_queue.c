#include "result_queue.h"
#include <glib.h>
#include <string.h>
/** \file
 * Implementation of the API defined in result_queue.h
 */

struct result_queue
{
   /** The corresponding GSource, so that one can be cast to the other. */
   GSource source;

   /** Asynchronous queue */
   GAsyncQueue* async_queue;

   /** result handler */
   result_queue_handler_f handler;

   /** main context passed to constructor */
   GMainContext *main_context;

   /** user data passed to constructor */
   gpointer userdata;
};
struct result_queue_event
{
   struct result_queue *queue;
   struct queryrunner *caller;
   struct result *result;
   float pertinence;
   char query[];
};

static struct result_queue_event *result_queue_event_new(struct result_queue *queue,
                                                         struct queryrunner *caller,
                                                         const char *query,
                                                         float pertinence,
                                                         struct result *result);
static void result_queue_event_delete(struct result_queue_event *ev);


static gboolean source_callback(gpointer data);
static gboolean source_prepare(GSource *source, gint *timeout);
static gboolean source_check(GSource *source);
static gboolean source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data);
static void source_finalize(GSource *source);

static GSourceFuncs source_functions = {
   source_prepare,
   source_check,
   source_dispatch,
   source_finalize,
};



struct result_queue* result_queue_new(GMainContext* context,
                                      result_queue_handler_f handler,
                                      gpointer userdata)
{
   g_return_val_if_fail(handler!=NULL, NULL);

   struct result_queue *queue = (struct result_queue *)
      g_source_new(&source_functions,
                   sizeof(struct result_queue));
   queue->handler=handler;
   queue->main_context=context;
   queue->userdata=userdata;
   queue->async_queue=g_async_queue_new();
   g_source_set_callback(&queue->source,
                         source_callback,
                         NULL/*data*/,
                         NULL/*notify*/);

   g_source_attach(&queue->source, context);

   return queue;
}

void result_queue_delete(struct result_queue* queue)
{
   g_return_if_fail(queue);
   GSource* source=&queue->source;
   g_source_destroy(source);
   g_source_unref(source);
}

void result_queue_add(struct result_queue *queue,
                      struct queryrunner *caller,
                      const char *query,
                      float pertinence,
                      struct result *result)
{
   struct result_queue_event *ev = result_queue_event_new(queue, caller, query, pertinence, result);
   g_async_queue_push(queue->async_queue, ev);
   g_main_context_wakeup(queue->main_context);
}

/* ------------------------- result queue events */

static struct result_queue_event *result_queue_event_new(struct result_queue *queue,
                                                         struct queryrunner *caller,
                                                         const char *query,
                                                         float pertinence,
                                                         struct result *result)
{
   g_return_val_if_fail(query!=NULL, NULL);
   g_return_val_if_fail(result!=NULL, NULL);

   struct result_queue_event *retval =
      (struct result_queue_event *)
      g_malloc(sizeof(struct result_queue_event)+strlen(query));
   retval->queue=queue;
   retval->caller=caller;
   retval->result=result;
   retval->pertinence=pertinence;
   strcpy(retval->query, query);
   return retval;
}

static void result_queue_event_delete(struct result_queue_event *ev)
{
   g_return_if_fail(ev!=NULL);
   g_free(ev);
}

/* ------------------------- source functions */

/**
 * Run on the main loop's thread, with the
 * event created in result_queue_add
 * @param data the event
 * @return TRUE
 */
static gboolean source_callback(gpointer data)
{
   g_return_val_if_fail(data!=NULL, TRUE);
   struct result_queue_event *ev = (struct result_queue_event*)data;
   ev->queue->handler(ev->caller,
                      ev->query,
                      ev->pertinence,
                      ev->result,
                      ev->queue->userdata);
   result_queue_event_delete(ev);
}

static gboolean source_prepare(GSource *source, gint *timeout)
{
   struct result_queue *queue = (struct result_queue *)source;
    *timeout=-1;
    return g_async_queue_length(queue->async_queue)>0;
}

static gboolean source_check(GSource *source)
{
}

static gboolean source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
   struct result_queue *queue = (struct result_queue *)source;
   gpointer data=g_async_queue_pop(queue->async_queue);
   gboolean result=callback(data);
   g_free(data);
   return result;

}

static void source_finalize(GSource *source)
{
   struct result_queue *queue = (struct result_queue *)source;
   g_async_queue_lock(queue->async_queue);
   g_async_queue_unref_and_unlock(queue->async_queue);
}
