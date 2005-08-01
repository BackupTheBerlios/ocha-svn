#ifndef RESULT_QUEUE_H
#define RESULT_QUEUE_H

#include <glib.h>
#include "result.h"
#include "queryrunner.h"

/** \file
 * A result queue is an ansynchronous queue
 * that is used to send results to the
 * glib/gdk main loop.
 *
 * A result queue is mainly a GSource based on
 * a GAsyncQueue.
 *
 * A result queue is meant to be used by several
 * threads at the same time, with one thread
 * running the main loop on which the handler
 * will be executed and several other threads
 * adding results into the queue.
 */

/**
 * Element of a result queue that's passed to the
 * result queue handler.
 *
 * This structure and all its element are only valid
 * for the duration of the call to the result queue
 * handler, except for the result itself (struct result *)
 * which needs to be freed explicitely using result_release()
 * before it is discarded.
 */
struct result_queue_element
{
        /**
         * Origin of this element
         */
        struct queryrunner *caller;
        /**
         * Query that's being run
         */
        char *query;

        /**
         * ID of the query, as returned by queryrunner_execute()
         */
        QueryId query_id;

        /**
         * The result itself, to be freed with
         * result->release()
         */
        struct result *result;
};

/**
 * A new result was added into the query queue.
 *
 * There's always only one handler running at a time
 * on the main loop's thread. The thread may be different
 * from the queryrunner's thread so care must be taken
 * when making calls to queryrunner.
 *
 * The content of the structure result_queue_element
 * is only valid until this function return, except for
 * the result itself with needs to be released by
 * the handler.
 * @param element
 * @param userdata passed to result_queue_new
 */
typedef void (*result_queue_handler_f)(struct result_queue_element *element, gpointer userdata);

/**
 * Create a new result queue.
 *
 * @param context context to attach this queue to, or NULL for the default context
 * @param handler the handler function
 * @param userdata user data to pass to the handler when it's called
 * @return a new result queue, to be freed using result_queue_delete()
 */
struct result_queue* result_queue_new(GMainContext* context, result_queue_handler_f handler, gpointer userdata);

/**
 * Release a queue and all the resources it used.
 *
 * Make sure the queue is empty before calling this function
 * otherwise you'll lose the data that's currently in the
 * queue.
 *
 * @param queue the queue to free
 */
void result_queue_delete(struct result_queue* queue);

/**
 * Put a result into the queue.
 *
 * This call is thread-safe.
 *
 * @param queue
 * @param caller query runner that added this result. the caller must exist at least
 * as long as all the results it added into the queue have been released.
 * @param query the query that's currently run by the query runner; this pointer will only be
 * available until the function returns
 * @param query_id ID of the query that's being run, as returned from queryrunner's execute_query
 * @param pertinence pertinence of this result, according to the query runner, between 0 and 1,
 * 1 meaning an exact match.
 * @param result the result itself. once this call is made, the caller should not
 * use the result in any way, unless the result implementation is thread-safe. The query
 * queue is responsible for freeing the result.
 */
void result_queue_add(struct result_queue *queue, struct queryrunner *caller, QueryId query_id, struct result *result);

#endif /*RESULT_QUEUE_H*/
