
/** \file
 * Implementation of the API defined in catalog_queryrunner.h
 */

#include "catalog_queryrunner.h"
#include "catalog.h"
#include "result_queue.h"
#include <stdio.h>

/**
 * Extension of the structure queryrunner for this implementation
 */
struct catalog_queryrunner
{
   struct queryrunner base;
   struct result_queue *queue;
   struct catalog *catalog;
   const char *path;
   /** # of results received so far */
   int result_count;

   /** query being run */
   const char *query;
};

/** Maximum # of results to take into account */
#define MAX_RESULT_COUNT 10

/** catalog_queryrunner to queryrunner */
#define QUERYRUNNER(catalog_qr) (&(catalog_qr)->base)
/** queryrunner to catalog_queryrunner */
#define CATALOG_QUERYRUNNER(catalog) ((struct catalog_queryrunner *)(catalog))

static void start(struct queryrunner *self);
static void run_query(struct queryrunner *self, const char *query);
static void consolidate(struct queryrunner *self);
static void stop(struct queryrunner *self);
static void release(struct queryrunner *self);

static bool try_connect(const char *path)
{
   struct catalog *catalog=catalog_connect(path, NULL/*errormsg*/);
   if(catalog==NULL)
      return false;
   catalog_disconnect(catalog);
   return true;
}

struct queryrunner *catalog_queryrunner_new(const char *path, struct result_queue *queue)
{
   if(!try_connect(path))
      {
         printf("connection to catalog in %s failed\n", path);
         return NULL;
      }
   struct catalog_queryrunner *queryrunner = g_new(struct catalog_queryrunner, 1);

   queryrunner->base.start=start;
   queryrunner->base.run_query=run_query;
   queryrunner->base.consolidate=consolidate;
   queryrunner->base.stop=stop;
   queryrunner->base.release=release;
   queryrunner->queue=queue;
   queryrunner->path=g_strdup(path);
   queryrunner->catalog=NULL;

   return QUERYRUNNER(queryrunner);
}

/**
 * Open a connection to the catalog.
 *
 * @param self
 */
static void start(struct queryrunner *_self)
{
   g_return_if_fail(_self!=NULL);
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);
   if(!self->catalog)
      {
         self->catalog=catalog_connect(self->path, NULL/*errmsg*/);
         if(!self->catalog)
            printf("connection to catalog %s failed\n", self->path);
      }
}

static bool result_callback(struct catalog *catalog, float pertinence, struct result *result, void *userdata)
{
   g_return_val_if_fail(userdata!=NULL, false);
   g_return_val_if_fail(result!=NULL, false);

   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(userdata);

   result_queue_add(self->queue,
                    QUERYRUNNER(self),
                    self->query,
                    pertinence,
                    result);
   self->result_count++;

   return self->result_count<MAX_RESULT_COUNT;
}

static void run_query(struct queryrunner *_self, const char *query)
{
   g_return_if_fail(_self!=NULL);
   g_return_if_fail(query!=NULL);
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);
   if(!self->catalog)
      {
         printf("run_query(%s,%s) failed: queryrunner not started\n", self->path, query);
         return;
      }

   self->result_count=0;
   self->query=query;
   if(!catalog_executequery(self->catalog,
                            query,
                            result_callback,
                            self/*userdata*/))
      {
         printf("run_query(%s,%s) failed: %s\n",
                self->path,
                query,
                catalog_error(self->catalog));
      }
   self->query=NULL;
}

static void consolidate(struct queryrunner *_self)
{
   g_return_if_fail(_self!=NULL);
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);

}

static void stop(struct queryrunner *_self)
{
   g_return_if_fail(_self!=NULL);
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);

   if(self->catalog)
      {
         catalog_disconnect(self->catalog);
         self->catalog=NULL;
      }
}

static void release(struct queryrunner *_self)
{
   g_return_if_fail(_self!=NULL);
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);

   stop(QUERYRUNNER(self));
   g_free((gpointer)self->path);
   g_free(self);
}

