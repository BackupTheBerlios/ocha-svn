#include "result_queue.h"
#include <glib.h>

/** \file
 * Implementation of the API defined in result_queue.h
 */

struct result_queue
{
   result_queue_handler_f handler;
};

struct result_queue* result_queue_new(GMainContext* context,
                                      result_queue_handler_f handler, gpointer userdata)
{
   g_return_val_if_fail(handler!=NULL, NULL);

   struct result_queue *queue = g_new(struct result_queue, 1);
   queue->handler=handler;

   return queue;
}

void result_queue_delete(struct result_queue* queue)
{
   g_return_if_fail(queue);

   g_free(queue);
}

void result_queue_add(struct result_queue *queue,
                      struct queryrunner *caller,
                      const char *query,
                      float pertinence,
                      struct result *result)
{
}
