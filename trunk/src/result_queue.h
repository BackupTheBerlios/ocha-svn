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
 * A new result was added into the query queue.
 *
 * There's always only one handler running at a time
 * on the main loop's thread. The thread may be different
 * from the queryrunner's thread so care must be taken
 * when making calls to queryrunner.
 *
 * @param caller the queryrunner that added this result. the caller
 * must exist at least as long as the result is not released. once
 * the result has been released, no calls should be made on the queryrunner.
 * @param query the query that caused this result to be added. this pointer
 * will only be valid for the duration of this function call
 * @param pertinence pertinence of this result, according to the query runner, between 0 and 1,
 * 1 meaning an exact match.
 * @param result the result itself. The function is responsible for freeing
 * the result. A correct empty implementation would call result_delete() on all results
 * it gets and then return.
 * @param userdata userdata passed to the constructor of the result queue
 */
typedef void (*result_queue_handler_f)(struct queryrunner *caller, const char *query, float pertinence, struct result *result, gpointer userdata);

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
 * @param pertinence pertinence of this result, according to the query runner, between 0 and 1,
 * 1 meaning an exact match.
 * @param result the result itself. once this call is made, the caller should not
 * use the result in any way, unless the result implementation is thread-safe. The query
 * queue is responsible for freeing the result.
 */
void result_queue_add(struct result_queue *queue, struct queryrunner *caller, const char *query, float pertinence, struct result *result);

#endif /*RESULT_QUEUE_H*/
