#include "mock_queryrunner.h"
#include "result_queue.h"
#include <glib.h>
#include <string.h>
#include <stdio.h>

/** \file
 * Implement the API defined in mock_queryrunner.h
 */


/** Number of result to add for one query */
#define COUNT 5

static void start(struct queryrunner *self);
static void run_query(struct queryrunner *self, const char *query);
static void consolidate(struct queryrunner *self);
static void stop(struct queryrunner *self);
static void release(struct queryrunner *self);

static struct result *result_new(const char *query, int index);
static gboolean result_execute(struct result *, GError **);
static void result_release(struct result *);

/** keep up to 256 characters of a query */
#define MAX_QUERY_LEN 256

/**
 * Keep whatever the mock queryrunner needs to remember.
 *
 * Can be cast to a struct queryrunner and back using
 * the defines MOCK_QUERYRUNNER and QUERYRUNNER.
 */
struct mock_queryrunner
{
   /** base queryrunner, must be the 1st element */
   struct queryrunner base;
   /** the queue to add results to */
   struct result_queue *queue;
   /** TRUE when the query is started */
   gboolean started;
   /** keep a copy of the query, for consolidate - up to a point. */
   char query[MAX_QUERY_LEN];
   /** make sure query always ends with a zero, which strncpy doesn't. */
   char zero;
};
/** Cast a struct queryrunner * to a struct mock_queryrunner * */
#define MOCK_QUERYRUNNER(runner) ((struct mock_queryrunner *)(runner))
/** Cast a struct mock_queryrunner * to a struct queryrunner * */
#define QUERYRUNNER(runner) (&(runner)->base)

struct queryrunner *mock_queryrunner_new(struct result_queue *queue)
{
   struct mock_queryrunner *runner = g_new(struct mock_queryrunner, 1);
   runner->base.start=start;
   runner->base.run_query=run_query;
   runner->base.consolidate=consolidate;
   runner->base.stop=stop;
   runner->base.release=release;
   runner->queue=queue;
   runner->started=FALSE;
   runner->query[0]='\0';
   runner->zero='\0';

   return QUERYRUNNER(runner);
}

/* ------------------------- static functions: result */

/**
 * Create a new result object with the given index.
 *
 * @param query query that's being run
 * @param index index of the result
 * @return a result
 */
static struct result *result_new(const char *query, int index)
{
   struct result *retval = g_new(struct result, 1);
   GString* name = g_string_new("");
   g_string_printf(name, "%s%d", query, index);
   retval->name=name->str;
   retval->long_name=name->str;
   retval->path=name->str;
   g_string_free(name, FALSE/*not free_segment*/);
   retval->execute=result_execute;
   retval->release=result_release;
   return retval;
}

/**
 * Execute a result.
 *
 * This function prints a message to stdout.
 * @praam result
 * @return TRUE, always
 */
static gboolean result_execute(struct result *result, GError **errors)
{
   g_return_val_if_fail(result!=NULL, FALSE);
   printf("execute result: %s\n", result->name);
   return TRUE;
}

/**
 * Release a result structure created by result_new().
 * @param result data to release
 */
static void result_release(struct result *result)
{
   g_return_if_fail(result!=NULL);
   printf("result_release(%s)\n", result->name);
   g_free((gpointer)result->name);
   g_free(result);
}

/* ------------------------- static functions: queryrunner */

/** Start the query */
static void start(struct queryrunner *_self)
{
   printf("start(0x%lx)\n", (unsigned long)_self);
   struct mock_queryrunner *self = MOCK_QUERYRUNNER(_self);
   g_return_if_fail(!self->started);
   self->started=TRUE;
}


/**
 * Run the mock query.
 *
 * This function immediately adds 5 results to the queue.
 * The results have the name of the current query + an
 * integer between 0 and 4.
 */
static void run_query(struct queryrunner *_self, const char *query)
{
   struct mock_queryrunner *self = MOCK_QUERYRUNNER(_self);
   g_return_if_fail(self->started);

   printf("run_query(%s)\n", query);

   strncpy(self->query, query, MAX_QUERY_LEN);
   /* self->zero=='\0' guarantees it'll be null-terminated */

   for(int i=0; i<COUNT; i++)
      {
         result_queue_add(self->queue,
                          QUERYRUNNER(self),
                          query,
                          0.8,
                          result_new(query, i));
      }
}

/**
 * Run the mock query.
 *
 * This function immediately adds 5 results to the queue.
 * The results have the name of the current query + an
 * integer between 10 and 14.
 */
static void consolidate(struct queryrunner *_self)
{
   struct mock_queryrunner *self = MOCK_QUERYRUNNER(_self);
   g_return_if_fail(self->started);
   printf("consolidate(%s)\n", self->query);

   for(int i=0; i<COUNT; i++)
      {
         result_queue_add(self->queue,
                          QUERYRUNNER(self),
                          self->query,
                          0.5,
                          result_new(self->query, 10+i));
      }
}

/**
 * Stop the query.
 */
static void stop(struct queryrunner *_self)
{
   struct mock_queryrunner *self = MOCK_QUERYRUNNER(_self);
   if(!self->started)
      return;
   printf("stop(0x%lx)\n", (unsigned long)_self);
   self->started=FALSE;
}

/**
 * Release the query runner.
 */
static void release(struct queryrunner *self)
{
   printf("release(0x%lx)\n", (unsigned long)self);
   g_free(self);
}
