
/** \file
 * Implementation of the API defined in catalog_queryrunner.h
 */

#include "catalog_queryrunner.h"
#include "catalog.h"
#include "result_queue.h"
#include <stdio.h>
#include <string.h>

/**
 * Extension of the structure queryrunner for this implementation
 */
struct catalog_queryrunner
{
   struct queryrunner base;
   struct result_queue *queue;
   const char *path;

   /** catalog, protected by the mutex */
   struct catalog *catalog;

   /** query to be run, protected by the mutex */
   GString *query;
   /** query being run, used only on the thread */
   GString *running_query;
   /** number of results found so far, used only on the thread */
   int count;

   GThread *thread;
   GMutex *mutex;

   /** tell the thread that query or stopping has changed */
   GCond *cond;

   /** set to true to tell the thread to end, protected by the mutex */
   bool stopping;
   /** true between start() and stop() */
   bool started;
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
static gpointer runquery_thread(gpointer);
static bool result_callback(struct catalog *catalog, float pertinence, struct result *result, void *userdata);

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
   queryrunner->query=g_string_new("");
   queryrunner->running_query=g_string_new("");
   queryrunner->path=g_strdup(path);
   queryrunner->catalog=NULL;
   queryrunner->mutex=g_mutex_new();
   queryrunner->cond=g_cond_new();
   queryrunner->stopping=false;
   queryrunner->thread=g_thread_create(runquery_thread,
                                       queryrunner/*userdata*/,
                                       true/*joinable*/,
                                       NULL/*error*/);


   return QUERYRUNNER(queryrunner);
}

static void release(struct queryrunner *_self)
{
   g_return_if_fail(_self!=NULL);
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);

   stop(QUERYRUNNER(self));

   g_mutex_lock(self->mutex);
   self->stopping=true;
   g_cond_broadcast(self->cond);
   g_mutex_unlock(self->mutex);

   g_thread_join(self->thread);
   g_string_free(self->query, true/*free content*/);
   g_string_free(self->running_query, true/*free content*/);
   g_free((gpointer)self->path);
   g_free(self);
}


static gpointer runquery_thread(gpointer userdata)
{
   struct catalog_queryrunner *queryrunner
      = (struct catalog_queryrunner *)userdata;

   printf("thread\n");
   g_mutex_lock(queryrunner->mutex);
   while(!queryrunner->stopping)
      {
         if(queryrunner->started)
            {
               if(!queryrunner->catalog)
                  {
                     g_mutex_unlock(queryrunner->mutex);
                     struct catalog *catalog=catalog_connect(queryrunner->path, NULL/*errmsg*/);
                     if(!catalog)
                        {
                           printf("connection to catalog %s failed\n",
                                  queryrunner->path);
                           continue;
                        }
                     g_mutex_lock(queryrunner->mutex);
                     queryrunner->catalog=catalog;
                  }

               if(queryrunner->query->len>0)
                  {
                     while(strcmp(queryrunner->query->str, queryrunner->running_query->str)!=0)
                        {
                           g_string_assign(queryrunner->running_query,
                                           queryrunner->query->str);

                           catalog_restart(queryrunner->catalog);
                           printf("execute query: %s\n",
                                  queryrunner->running_query->str);
                           queryrunner->count=0;
                           g_mutex_unlock(queryrunner->mutex);
                           if(!catalog_executequery(queryrunner->catalog,
                                                    queryrunner->running_query->str,
                                                    result_callback,
                                                    queryrunner/*userdata*/))
                              {
                                 printf("run_query(%s,%s) failed: %s\n",
                                        queryrunner->path,
                                        queryrunner->running_query->str,
                                        catalog_error(queryrunner->catalog));
                              }
                           g_mutex_lock(queryrunner->mutex);
                        }
                  }
            }
         else if(queryrunner->catalog)
            {
               struct catalog *catalog = queryrunner->catalog;
               queryrunner->catalog=NULL;
               g_mutex_unlock(queryrunner->mutex);
               catalog_disconnect(catalog);
               g_mutex_lock(queryrunner->mutex);
            }

         printf("thread: waiting on cond\n");
         g_cond_wait(queryrunner->cond,
                     queryrunner->mutex);
      }
   g_mutex_unlock(queryrunner->mutex);
   return NULL;
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
   g_mutex_lock(self->mutex);
   self->started=true;
   g_cond_broadcast(self->cond);
   g_mutex_unlock(self->mutex);
}

static bool result_callback(struct catalog *catalog, float pertinence, struct result *result, void *userdata)
{
   g_return_val_if_fail(userdata!=NULL, false);
   g_return_val_if_fail(result!=NULL, false);

   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(userdata);

   result_queue_add(self->queue,
                    QUERYRUNNER(self),
                    self->running_query->str,
                    pertinence,
                    result);
   self->count++;
   return self->count<MAX_RESULT_COUNT;
}

static void run_query(struct queryrunner *_self, const char *query)
{
   g_return_if_fail(_self!=NULL);
   g_return_if_fail(query!=NULL);

   printf("run_query(%s):enter\n", query);
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);
   g_mutex_lock(self->mutex);
   printf("run_query(%s):locked\n", query);
   if(self->catalog)
      {
         printf("interrupt...\n");
         catalog_interrupt(self->catalog);
      }
   g_string_assign(self->query, query);
   g_cond_broadcast(self->cond);
   printf("run_query(%s):unlock\n", query);
   g_mutex_unlock(self->mutex);
   printf("run_query(%s):done\n", query);
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

   g_mutex_lock(self->mutex);
   self->started=false;
   g_string_truncate(self->query, 0);
   if(self->catalog)
      catalog_interrupt(self->catalog);
   g_cond_broadcast(self->cond);
   g_mutex_unlock(self->mutex);
}

