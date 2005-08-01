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
        QueryId query_id;
};

/** Number of result queues on the system.
 * This is not part of the public API and is meant
 * to be used only by test cases.
 */
int result_queue_counter;


/* ------------------------- prototypes: result_queue_source */
static gboolean result_queue_source_prepare(GSource *source, gint *timeout);
static gboolean result_queue_source_check(GSource *source);
static gboolean result_queue_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data);
static void result_queue_source_finalize(GSource *source);

/* ------------------------- prototypes: other */
static struct event *event_new(struct result_queue *queue, struct queryrunner *caller, QueryId query_id, struct result *result);
static void event_free(struct event *queue);
static gboolean source_callback(gpointer data);

/* ------------------------- definitions */

/** Definition of a source */
static GSourceFuncs source_functions = {
        result_queue_source_prepare,
        result_queue_source_check,
        result_queue_source_dispatch,
        result_queue_source_finalize,
};

/* ------------------------- public functions */

struct result_queue* result_queue_new(GMainContext* context,
                                      result_queue_handler_f handler,
                                      gpointer userdata)
{
        struct result_queue *queue;

        g_return_val_if_fail(handler!=NULL, NULL);

        queue = (struct result_queue *)
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
        GSource *source;
        g_return_if_fail(queue);

        source=SOURCE(queue);
        g_source_destroy(source);
        g_source_unref(source);
        /* resources are only released by source_finalize(),
         * caled by g_source_unref() whenever it thinks
         * there are no more references to the source.
         */
}

void result_queue_add(struct result_queue *queue,
                      struct queryrunner *caller,
                      QueryId query_id,
                      struct result *result)
{
        struct event *ev;

        /* WARNING: can be used by more than one thread at a time */
        g_return_if_fail(queue);
        g_return_if_fail(result);

        ev = event_new(queue, caller, query_id, result);
        g_async_queue_push(queue->async_queue, ev);
        g_main_context_wakeup(queue->main_context);
}

/* ------------------------- member functions: result_queue_source */

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
static gboolean result_queue_source_prepare(GSource *source, gint *timeout)
{
        struct result_queue *queue;

        g_return_val_if_fail(source, FALSE);
        queue = (struct result_queue *)source;
        *timeout=-1;
        return g_async_queue_length(queue->async_queue)>0;
}

/**
 * Check the source, after polling.
 * @return TRUE if there's something on the queue
 */
static gboolean result_queue_source_check(GSource *source)
{
        struct result_queue *queue;

        g_return_val_if_fail(source, FALSE);
        queue = (struct result_queue *)source;
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
static gboolean result_queue_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
        struct result_queue *queue;
        struct event *ev;

        g_return_val_if_fail(source, FALSE);
        g_return_val_if_fail(callback==source_callback, FALSE);

        queue = RESULT_QUEUE(source);
        while( (ev=(struct event *)g_async_queue_try_pop(queue->async_queue)) != NULL) {
                callback(ev);
                event_free(ev);
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
static void result_queue_source_finalize(GSource *source)
{
        struct result_queue *queue;
        GAsyncQueue* async_queue;
        struct event *ev;

        g_return_if_fail(source);
        queue = RESULT_QUEUE(source);
        async_queue = queue->async_queue;
        g_async_queue_lock(async_queue);

        /* Just in case: empty the queue, release the results and unreference */
        while((ev=(struct event *)g_async_queue_try_pop_unlocked(async_queue))!=NULL) {
                struct result *result = ev->result;
                if(result && result->release) {
                        result->release(result);
                }
                event_free(ev);
        }
        g_async_queue_unref_and_unlock(async_queue);
        result_queue_counter--;
}

/* ------------------------- static functions */

static struct event *event_new(struct result_queue *queue,
                               struct queryrunner *caller,
                               QueryId query_id,
                               struct result *result)
{
        struct event *retval;

        g_return_val_if_fail(result!=NULL, NULL);


        retval = g_new(struct event, 1);
        retval->queue=queue;
        retval->caller=caller;
        retval->result=result;
        retval->query_id=query_id;
        return retval;
}
static void event_free(struct event *event)
{
        g_return_if_fail(event);
        g_free(event);
}

/**
 * Run on the main loop's thread, with the
 * event created in result_queue_add
 * @param data the event
 * @return TRUE
 */
static gboolean source_callback(gpointer data)
{
        struct event *ev;
        struct result_queue_element element;

        g_return_val_if_fail(data!=NULL, TRUE);

        ev = (struct event*)data;
        element.caller=ev->caller;
        element.query_id=ev->query_id;
        element.result=ev->result;
        ev->queue->handler(&element, ev->queue->userdata);

        return TRUE;
}

