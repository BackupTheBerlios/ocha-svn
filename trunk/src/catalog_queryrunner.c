
/** \file
 * Implementation of the API defined in catalog_queryrunner.h
 */

#include "catalog_queryrunner.h"
#include "catalog.h"
#include "catalog_result.h"
#include "result_queue.h"
#include "indexer.h"
#include "indexers.h"
#include <stdio.h>
#include <string.h>

/**
 * Number of results to send without pausing
 * in the "first bunch".
 * The goal of the first bunch is to provide
 * an estimate and let the user refine it. It
 * must be fast.
 */
#define FIRST_BUNCH_SIZE 4
/**
 * Time to wait after the first bunch has been
 * sent before sending more results (ms).
 * If the user doesn't react after that time,
 * it probably means that more results will
 * help.
 */
#define AFTER_FIRST_BUNCH_TIMEOUT 1300

/**
 * Time to wait between several "later bunches" (ms).
 * Too many results at once would drown the UI.
 * The goal is to present the result as they come
 * slow enough for the user and the UI not to
 * be drowned in useless results
 */
#define LATER_BUNCH_TIMEOUT 400

/**
 * Number of results to put into a "later bunch"
 */
#define LATER_BUNCH_SIZE 8

/**
 * Maximum number of results to send, ever
 */
#define MAXIMUM 200

#define DEBUG 1
#ifdef DEBUG
# define lock(m) printf("%s:%d query_lock\n", __FILE__, __LINE__);g_mutex_lock(m)
# define unlock(m) printf("%s:%d query_unlock\n", __FILE__, __LINE__);g_mutex_unlock(m)
#else
# define lock(m) g_mutex_lock(m)
# define unlock(m) g_mutex_unlock(m)
#endif


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

   /** set to TRUE to tell the thread to end, protected by the mutex */
   gboolean stopping;
   /** TRUE between start() and stop() */
   gboolean started;
};

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
static gboolean result_callback(struct catalog *catalog,
                                float pertinence,
                                int entry_id,
                                const char *name,
                                const char *long_name,
                                const char *path,
                                int source_id,
                                const char *source_type,
                                void *userdata);
static gboolean query_has_changed(struct catalog_queryrunner *self);
static void wait_on_condition(struct catalog_queryrunner *self, int wait_ms);

static struct result *result_create(struct catalog_queryrunner *runner,
                                    struct indexer *indexer,
                                    const char *name,
                                    const char *long_name,
                                    const char *path,
                                    int entry_id);
static gboolean result_execute(struct result *result, GError **err);
static gboolean result_validate(struct result *result);
static void result_free(struct result *result);

static gboolean try_connect(const char *path)
{
   struct catalog *catalog=catalog_connect(path, NULL/*errormsg*/);
   if(catalog==NULL)
      return FALSE;
   catalog_disconnect(catalog);
   return TRUE;
}

struct queryrunner *catalog_queryrunner_new(const char *path, struct result_queue *queue)
{
   if(!try_connect(path))
      {
          fprintf(stderr, "connection to catalog in %s failed\n", path);
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
   queryrunner->stopping=FALSE;
   queryrunner->thread=g_thread_create(runquery_thread,
                                       queryrunner/*userdata*/,
                                       FALSE/*not joinable*/,
                                       NULL/*error*/);


   return QUERYRUNNER(queryrunner);
}

static void release(struct queryrunner *_self)
{
   g_return_if_fail(_self!=NULL);
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);

   stop(QUERYRUNNER(self));
#ifdef DEBUG
   printf("%s:%d stopping", __FILE__, __LINE__);
#endif
   lock(self->mutex);
   self->stopping=TRUE;
   g_cond_broadcast(self->cond);
   unlock(self->mutex);

#ifdef DEBUG
   printf("%s:%s released\n", __FILE__, __LINE__);
#endif
   /* the real release will be done by the
    * thread when it notices self->stopping==TRUE
    */
}
static gboolean query_has_changed(struct catalog_queryrunner *queryrunner)
{
   return strcmp(queryrunner->query->str, queryrunner->running_query->str)!=0;
}

static gpointer runquery_thread(gpointer userdata)
{
   struct catalog_queryrunner *queryrunner
      = (struct catalog_queryrunner *)userdata;

#ifdef DEBUG
   printf("%s:%d thread started\n", __FILE__, __LINE__);
#endif
   lock(queryrunner->mutex);
   while(!queryrunner->stopping)
      {
         if(queryrunner->started)
            {
               if(!queryrunner->catalog)
                  {
                     unlock(queryrunner->mutex);
                     struct catalog *catalog=catalog_connect(queryrunner->path, NULL/*errmsg*/);
                     if(!catalog)
                        {
#ifdef DEBUG
                           printf("%s:%d connection to catalog %s failed\n",
                                  __FILE__,
                                  __LINE__,
                                  queryrunner->path);
#endif
                           lock(queryrunner->mutex);
                           continue;
                        }
                     lock(queryrunner->mutex);
                     queryrunner->catalog=catalog;
                     continue; /*recheck without waiting */
                  }

               if(query_has_changed(queryrunner) && queryrunner->query->len>0)
                  {
                     g_string_assign(queryrunner->running_query,
                                     queryrunner->query->str);

                     catalog_restart(queryrunner->catalog);
                     queryrunner->count=0;
                     unlock(queryrunner->mutex);
#ifdef DEBUG
                     printf("%s:%d execute query: %s\n",
                            __FILE__,
                            __LINE__,
                            queryrunner->running_query->str);
#endif
                     if(!catalog_executequery(queryrunner->catalog,
                                              queryrunner->running_query->str,
                                              result_callback,
                                              queryrunner/*userdata*/))
                        {
                            fprintf(stderr,
                                    "%s:%d run_query(%s,%s) failed: %s\n",
                                    __FILE__,
                                    __LINE__,
                                    queryrunner->path,
                                    queryrunner->running_query->str,
                                    catalog_error(queryrunner->catalog));
                        }
#ifdef DEBUG
                     printf("%s:%d finished executing query: %s\n",
                            __FILE__,
                            __LINE__,
                            queryrunner->running_query->str);
#endif
                     lock(queryrunner->mutex);
                     continue; /*recheck without waiting */
                  }
            }
         else if(queryrunner->catalog)
            {
               struct catalog *catalog = queryrunner->catalog;
               queryrunner->catalog=NULL;
               unlock(queryrunner->mutex);
               catalog_disconnect(catalog);
               lock(queryrunner->mutex);
               continue; /*recheck without waiting */
            }

#ifdef DEBUG
         printf("%s:%d thread: waiting on condition\n", __FILE__, __LINE__);
#endif
         g_cond_wait(queryrunner->cond,
                     queryrunner->mutex);
      }
   unlock(queryrunner->mutex);

#ifdef DEBUG
   printf("%s:%d thread cleanup\n", __FILE__, __LINE__);
#endif
   g_string_free(queryrunner->query, TRUE/*free content*/);
   g_string_free(queryrunner->running_query, TRUE/*free content*/);
   g_free((gpointer)queryrunner->path);
   g_free(queryrunner);
#ifdef DEBUG
   printf("%s:%d thread done\n", __FILE__, __LINE__);
#endif
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
#ifdef DEBUG
   printf("%s:%d start\n", __FILE__, __LINE__);
#endif

   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);
   lock(self->mutex);
   self->started=TRUE;
   g_cond_broadcast(self->cond);
   unlock(self->mutex);

}

static void wait_on_condition(struct catalog_queryrunner *self, int time_ms)
{
   GTimeVal timeval;
   g_get_current_time(&timeval);
   g_time_val_add(&timeval, time_ms*1000);

   lock(self->mutex);
   if(!query_has_changed(self))
      g_cond_timed_wait(self->cond, self->mutex, &timeval);
   unlock(self->mutex);

}
static gboolean result_callback(struct catalog *catalog,
                                float pertinence,
                                int entry_id,
                                const char *name,
                                const char *long_name,
                                const char *path,
                                int source_id,
                                const char *source_type,
                                void *userdata)
{
   g_return_val_if_fail(userdata!=NULL, FALSE);
   g_return_val_if_fail(name!=NULL, FALSE);
   g_return_val_if_fail(long_name!=NULL, FALSE);
   g_return_val_if_fail(path!=NULL, FALSE);
   g_return_val_if_fail(source_type!=NULL, FALSE);

   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(userdata);

   struct indexer *indexer = indexers_get(source_type);
   if(!indexer)
       {
           /* it can happen, because indexers may be removed, but it's
            * unusual and it might be ok. However, after an indexer is removed,
            * the catalog should be cleaned. I want to know about unclean
            * catalogs.
            */
           g_warning("unclean catalog: no indexer for source referenced "
                     "in catalog with id=%d type=%s\n",
                     source_id, source_type);
           return TRUE;
       }

   struct result *result = catalog_result_create(self->path,
                                                 indexer,
                                                 path,
                                                 name,
                                                 long_name,
                                                 entry_id);
   const char *query = self->running_query->str;
#ifdef DEBUG
   printf("%s:%d:query(%s) add %s\n",
          __FILE__, __LINE__, query, path);
#endif
   result_queue_add(self->queue,
                    QUERYRUNNER(self),
                    query,
                    pertinence,
                    result);
   int count = self->count;
   count++;
   self->count=count;
   if(count>=MAXIMUM)
      return FALSE;

   if(count==FIRST_BUNCH_SIZE)
       {
#ifdef DEBUG
           printf("%s:%d:query(%s) wait (1st bunch)\n",
                  __FILE__, __LINE__, query);
#endif
           wait_on_condition(self, AFTER_FIRST_BUNCH_TIMEOUT);
#ifdef DEBUG
           printf("%s:%d:query(%s) wait (1st bunch) done\n",
                  __FILE__, __LINE__, query);
#endif
       }
   else if(count%LATER_BUNCH_SIZE==0)
       {
#ifdef DEBUG
           printf("%s:%d:query(%s) wait (later bunch)\n",
                  __FILE__, __LINE__, query);
#endif
           wait_on_condition(self, LATER_BUNCH_TIMEOUT);
#ifdef DEBUG
           printf("%s:%d:query(%s) wait (later bunch) done\n",
                  __FILE__, __LINE__, query);
#endif
       }
   return TRUE;
}

static void run_query(struct queryrunner *_self, const char *query)
{
   g_return_if_fail(_self!=NULL);
   g_return_if_fail(query!=NULL);

#ifdef DEBUG
   printf("%s:%d:run_query(%s) enter\n",
          __FILE__, __LINE__, query);
#endif
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);
   lock(self->mutex);
   char stripped_query[strlen(query)+1];
   strcpy(stripped_query, query);
   g_strstrip(stripped_query);

   if(strcmp(self->query->str, stripped_query)!=0)
       {
           if(self->catalog)
               {
#ifdef DEBUG
                   printf("%s:%d:run_query(%s) interrupt previous query...\n",
                          __FILE__, __LINE__, query);
#endif
                   catalog_interrupt(self->catalog);
               }
           g_string_assign(self->query, stripped_query);

#ifdef DEBUG
           printf("%s:%d:run_query(%s) assigned, going to broadcast\n",
                  __FILE__, __LINE__, query);
#endif

           if(self->query->len>0)
               g_cond_broadcast(self->cond);
       }
   unlock(self->mutex);
#ifdef DEBUG
   printf("%s:%d:run_query(%s) all done\n",
          __FILE__, __LINE__, query);
#endif
}

static void consolidate(struct queryrunner *_self)
{
   g_return_if_fail(_self!=NULL);
   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);

}

static void stop(struct queryrunner *_self)
{
   g_return_if_fail(_self!=NULL);
#ifdef DEBUG
   printf("%s:%d stop\n", __FILE__, __LINE__);
#endif

   struct catalog_queryrunner *self = CATALOG_QUERYRUNNER(_self);

   lock(self->mutex);
   self->started=FALSE;
   g_string_truncate(self->query, 0);
   if(self->catalog)
      catalog_interrupt(self->catalog);
   g_cond_broadcast(self->cond);
   unlock(self->mutex);
}

