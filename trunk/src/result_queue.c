#include "result_queue.h"
#include <glib.h>
#include <string.h>
/** \file
 * Implementation of the API defined in result_queue.h
 */

/**
 * The public structure, struct result_queue, which
 * can also be seen as a source.
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

#define RESULT_QUEUE(source) ((struct result_queue *)(source))
#define SOURCE(result_queue) (&(result_queue)->source)

/** private event */
struct event
{
   struct result_queue *queue;
   struct queryrunner *caller;
   struct result *result;
   float pertinence;
   char query[];
};

static struct event *event_new(struct result_queue *queue,
                               struct queryrunner *caller,
                               const char *query,
                               float pertinence,
                               struct result *result);


static gboolean source_callback(gpointer data);
static gboolean source_prepare(GSource *source, gint *timeout);
static gboolean source_check(GSource *source);
static gboolean source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data);
static void source_finalize(GSource *source);
static void result_handler(struct queryrunner *, const char *, float, struct result *, gpointer);

/** Definition of a source */
static GSourceFuncs source_functions = {
   source_prepare,
   source_check,
   source_dispatch,
   source_finalize,
};

/** Number of result queues on the system.
 * This is not part of the public API and is meant
 * to be used only by test cases.
 */
int result_queue_counter;

/* ------------------------- public functions */

struct result_queue* result_queue_new(GMainContext* context,
                                      result_queue_handler_f handler,
                                      gpointer userdata)
{
   g_return_val_if_fail(handler!=NULL, NULL);

   struct result_queue *queue = (struct result_queue *)
      g_source_new(&source_functions,
                   sizeof(struct result_queue));
   queue->async_queue=g_async_queue_new();
   queue->handler=handler;
   queue->main_context=context;
   queue->userdata=userdata;

   g_source_set_callback(SOURCE(queue),
                         source_callback,
                         NULL/*data*/,
                         NULL/*notify*/);

   g_source_attach(SOURCE(queue), context);

   result_queue_counter++;
   return queue;
}

void result_queue_delete(struct result_queue* queue)
{
   g_return_if_fail(queue);

   GSource* source=SOURCE(queue);
   g_source_destroy(source);
   g_source_unref(source);
   /* resources are only released by source_finalize(),
    * caled by g_source_unref() whenever it thinks
    * there are no more references to the source.
    */
}

void result_queue_add(struct result_queue *queue,
                      struct queryrunner *caller,
                      const char *query,
                      float pertinence,
                      struct result *result)
{
   /* WARNING: can be used by more than one thread at a time */
   g_return_if_fail(queue);
   g_return_if_fail(query);
   g_return_if_fail(result);

   struct event *ev = event_new(queue, caller, query, pertinence, result);
   g_async_queue_push(queue->async_queue, ev);
   g_main_context_wakeup(queue->main_context);
}

/* ------------------------- result queue events */

static struct event *event_new(struct result_queue *queue,
                               struct queryrunner *caller,
                               const char *query,
                               float pertinence,
                               struct result *result)
{
   g_return_val_if_fail(query!=NULL, NULL);
   g_return_val_if_fail(result!=NULL, NULL);

   struct event *retval =
      (struct event *)
      g_malloc(sizeof(struct event)+strlen(query)+1);
   retval->queue=queue;
   retval->caller=caller;
   retval->result=result;
   retval->pertinence=pertinence;
   strcpy(retval->query, query);
   return retval;
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
   struct event *ev = (struct event*)data;
   ev->queue->handler(ev->caller,
                      ev->query,
                      ev->pertinence,
                      ev->result,
                      ev->queue->userdata);
}

/**
 * Before source content are polled.
 *
 * *timeout is always set to -1, since we rely on
 * g_main_context_wakeup to wake up the loop when
 * there's a new result
 *
 * @param source
 * @param timeout maximum timeout to pass to the poll() call (out)
 * @return TRUE if there's something on the queue
 */
static gboolean source_prepare(GSource *source, gint *timeout)
{
   g_return_val_if_fail(source, FALSE);
   struct result_queue *queue = (struct result_queue *)source;
   *timeout=-1;
   return g_async_queue_length(queue->async_queue)>0;
}

/**
 * Check the source, after polling.
 * @return TRUE if there's something on the queue
 */
static gboolean source_check(GSource *source)
{
   g_return_val_if_fail(source, FALSE);
   struct result_queue *queue = (struct result_queue *)source;
   return g_async_queue_length(queue->async_queue)>0;
}

/**
 * Call the GSource callback.
 *
 * @param source
 * @param callback callback function, which may be NULL
 * @param user_data ignored
 * @return result TRUE or FALSE
 */
static gboolean source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
   g_return_val_if_fail(source, FALSE);
   g_return_val_if_fail(callback==source_callback, FALSE);

   struct result_queue *queue = RESULT_QUEUE(source);

   gpointer data;
   while( (data=g_async_queue_try_pop(queue->async_queue)) != NULL)
      {
         callback(data);
         g_free(data);
      }
   return TRUE;

}

/**
 * Free the members of the struct result_queue.
 * The memory usedy by result_queue will be freed
 * automatically, since it's a GSource
 * This function is called when there are no
 * more reference to the queue, before
 * the source is freed.
 * @param source the source to free
 */
static void source_finalize(GSource *source)
{
   g_return_if_fail(source);
   struct result_queue *queue = RESULT_QUEUE(source);
   GAsyncQueue* async_queue = queue->async_queue;
   g_async_queue_lock(async_queue);

   /* Just in case: empty the queue, release the results and unreference */
   struct event *ev;
   while((ev=(struct event *)g_async_queue_try_pop_unlocked(async_queue))!=NULL)
      {
         struct result *result = ev->result;
         if(result && result->release)
            result->release(result);
         g_free(ev);
      }
   g_async_queue_unref_and_unlock(async_queue);
   result_queue_counter--;
}
