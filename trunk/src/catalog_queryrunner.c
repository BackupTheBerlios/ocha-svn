
/** \file
 * Implementation of the API defined in catalog_queryrunner.h
 */

#include "catalog_queryrunner.h"
#include "catalog.h"
#include "catalog_result.h"
#include "result_queue.h"
#include "launcher.h"
#include "launchers.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include "string_utils.h"

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

/**
 * Extension of the structure queryrunner for this implementation
 */
struct catalog_queryrunner
{
        struct queryrunner base;
        /**
         * Where the result will be sent
         */
        struct result_queue *queue;

        /**
         * Where the thread will read actions
         * from.
         * Content will be of the type catalog_queryrunner_msg and
         * will have to be freed by the receiver.
         */
        GAsyncQueue *incoming;

        /**
         * The catalog.
         *
         * Normally belongs to the query thread, but other
         * threads may call catalog_interrupt while
         * keeping a lock on the incoming message
         * queue.
         */
        struct catalog *catalog;

        /**
         * Catalog path.
         */
        char *path;

        /**
         * The thread, while it's running (joinable)
         */
        GThread *thread;

        /**
         * TRUE between start() and stop(), used
         * by the main thread exclusively
         */
        gboolean started;
};

/** catalog_queryrunner to queryrunner */
#define QUERYRUNNER(catalog_qr) (&(catalog_qr)->base)
/** queryrunner to catalog_queryrunner */
#define CATALOG_QUERYRUNNER(catalog) ((struct catalog_queryrunner *)(catalog))

enum CatalogQueryrunnerMessageAction {
        /** connect to the catalog */
        CATALOG_QUERYRUNNER_ACTION_CONNECT,
        /** run the query */
        CATALOG_QUERYRUNNER_ACTION_QUERY,
        /** disconnect from the catalog */
        CATALOG_QUERYRUNNER_ACTION_DISCONNECT,
        /** stop the thread */
        CATALOG_QUERYRUNNER_ACTION_SHUTDOWN
};

struct catalog_queryrunner_msg {
        /* What the thread should do */
        enum CatalogQueryrunnerMessageAction action;
        /* query to run, to be freed by g_free (or NULL) */
        char *query;
};


/**
 * Data used by the thread
 */
struct thread_data {
        struct catalog_queryrunner *queryrunner;

        /** Query being run or NULL */
        char *running_query;

        /** Number of results found so far */
        int count;

        /**
         * Message read by catalog_queryrunner_msg_wait()
         * that will be returned by the next call
         * to catalog_queryrunner_msg_next()
         */
        struct catalog_queryrunner_msg *next_msg;
};

/* ------------------------- prototypes */
static gpointer runquery_thread(gpointer userdata);
static gboolean result_callback(struct catalog *catalog, const struct catalog_query_result *result, void *userdata);
static void catalog_queryrunner_msg_send(struct catalog_queryrunner *self, enum CatalogQueryrunnerMessageAction action, const char *msg);
static void catalog_queryrunner_msg_free(struct catalog_queryrunner_msg *msg);
static gboolean catalog_queryrunner_msg_wait(struct thread_data *data, unsigned long timeout);
static struct catalog_queryrunner_msg *catalog_queryrunner_msg_next(struct thread_data *data);
static void handle_thread_error(struct catalog_queryrunner *self);

/* ------------------------- member functions (queryrunner) */
static void catalog_queryrunner_run_query(struct queryrunner *_self, const char *query);
static void catalog_queryrunner_consolidate(struct queryrunner *_self);
static void catalog_queryrunner_start(struct queryrunner *_self);
static void catalog_queryrunner_stop(struct queryrunner *_self);
static void catalog_queryrunner_release(struct queryrunner *_self);

/* ------------------------- public functions */
struct queryrunner *catalog_queryrunner_new(const char *path, struct result_queue *catalog_queryrunner_queue)
{
        struct catalog_queryrunner *queryrunner;
        struct catalog *catalog;

        catalog = catalog_new(path);
        if(!catalog_connect(catalog)) {
                fprintf(stderr,
                        "connection to catalog in %s failed: %s\n",
                        path,
                        catalog_error(catalog));
                catalog_free(catalog);
                return NULL;
        }
        catalog_disconnect(catalog);

        queryrunner = g_new(struct catalog_queryrunner, 1);

        queryrunner->base.start=catalog_queryrunner_start;
        queryrunner->base.run_query=catalog_queryrunner_run_query;
        queryrunner->base.consolidate=catalog_queryrunner_consolidate;
        queryrunner->base.stop=catalog_queryrunner_stop;
        queryrunner->base.release=catalog_queryrunner_release;
        queryrunner->path=g_strdup(path);
        queryrunner->queue=catalog_queryrunner_queue;
        queryrunner->incoming=g_async_queue_new();
        queryrunner->catalog=catalog;
        queryrunner->started=FALSE;
        queryrunner->thread=g_thread_create(runquery_thread,
                                            queryrunner/*userdata*/,
                                            TRUE/*joinable*/,
                                            NULL/*error*/);
        return QUERYRUNNER(queryrunner);
}

/* ------------------------- member functions */
/**
 * Open a connection to the catalog.
 *
 * @param self
 */
static void catalog_queryrunner_start(struct queryrunner *_self)
{
        struct catalog_queryrunner *self;

        g_return_if_fail(_self!=NULL);
#ifdef DEBUG
        printf("%s:%d start\n", __FILE__, __LINE__);
#endif

        self = CATALOG_QUERYRUNNER(_self);

        if(!self->started) {
                catalog_queryrunner_msg_send(self, CATALOG_QUERYRUNNER_ACTION_CONNECT, NULL/*no query*/);
                self->started=TRUE;
        }
}

static void catalog_queryrunner_stop(struct queryrunner *_self)
{
        struct catalog_queryrunner *self;

        g_return_if_fail(_self!=NULL);
#ifdef DEBUG

        printf("%s:%d stop\n", __FILE__, __LINE__);
#endif

        self = CATALOG_QUERYRUNNER(_self);

        if(self->started) {
                catalog_queryrunner_msg_send(self, CATALOG_QUERYRUNNER_ACTION_DISCONNECT, NULL/*no query*/);
                self->started=FALSE;
        }
}


static void catalog_queryrunner_run_query(struct queryrunner *_self, const char *query)
{
        struct catalog_queryrunner *self;

        g_return_if_fail(_self!=NULL);
        g_return_if_fail(query!=NULL);


#ifdef DEBUG

        printf("%s:%d:run_query(%s) enter\n",
               __FILE__, __LINE__, query);
#endif

        self = CATALOG_QUERYRUNNER(_self);
        g_return_if_fail(self->started);

        if(self->started) {
                catalog_queryrunner_msg_send(self, CATALOG_QUERYRUNNER_ACTION_QUERY, query);
        }
}

static void catalog_queryrunner_consolidate(struct queryrunner *_self)
{
}

static void catalog_queryrunner_release(struct queryrunner *_self)
{
        struct catalog_queryrunner *self;

        g_return_if_fail(_self!=NULL);
        self = CATALOG_QUERYRUNNER(_self);

#ifdef DEBUG
        printf("%s:%d stopping", __FILE__, __LINE__);
#endif

        catalog_queryrunner_msg_send(self, CATALOG_QUERYRUNNER_ACTION_SHUTDOWN, NULL);
        g_thread_join(self->thread);

        g_async_queue_unref(self->incoming);
        catalog_free(self->catalog);
        g_free(self->path);
        g_free(self);

#ifdef DEBUG
        printf("%s:%d released\n", __FILE__, __LINE__);
#endif
}

/* ------------------------- static functions */
static gpointer runquery_thread(gpointer userdata)
{
        struct catalog_queryrunner *queryrunner;
        struct catalog_queryrunner_msg *msg;
        gboolean shutdown = FALSE;
        struct catalog *catalog;
        GAsyncQueue *queue;
        struct thread_data data;
        memset(&data, 0, sizeof(struct thread_data));

#ifdef DEBUG
        printf("%s:%d thread start\n", __FILE__, __LINE__);
#endif

        queryrunner = (struct catalog_queryrunner *)userdata;
        data.queryrunner=queryrunner;
        g_return_val_if_fail(queryrunner!=NULL, NULL);
        catalog = queryrunner->catalog;
        queue = g_async_queue_ref(queryrunner->incoming);

        do {
                msg = catalog_queryrunner_msg_next(&data);

                switch(msg->action) {
                case CATALOG_QUERYRUNNER_ACTION_CONNECT:
#ifdef DEBUG
                        printf("%s:%d: thread:connect\n", /*@nocommit@*/
                               __FILE__,
                               __LINE__
                               );
#endif

                        if(!catalog_is_connected(catalog)) {
                                if(!catalog_connect(catalog)) {
                                        handle_thread_error(queryrunner);
                                }
                        }
                        break;

                case CATALOG_QUERYRUNNER_ACTION_QUERY:
#ifdef DEBUG
                        printf("%s:%d: thread:query(%s) %d\n", /*@nocommit@*/
                               __FILE__,
                               __LINE__,
                               msg->query,
                               catalog_is_connected(catalog)
                               );
#endif
                        if(msg->query!=NULL && strlen(msg->query)>0) {
                                if(catalog_is_connected(catalog)) {
                                        catalog_restart(catalog);
                                        data.running_query=msg->query;
                                        data.count=0;
                                        if(!catalog_executequery(catalog,
                                                                 msg->query,
                                                                 result_callback,
                                                                 &data)) {
                                                handle_thread_error(queryrunner);
                                        }
                                        data.running_query=NULL;
                                } else {
                                        g_warning("not connected, "
                                                  "CATALOG_QUERYRUNNER_ACTION_CONNECT not "
                                                  "sent before _QUERY");
                                }
                        }
                        printf("%s:%d: query done: %s\n", /*@nocommit@*/
                               __FILE__,
                               __LINE__,
                               msg->query
                               );

                        break;

                case CATALOG_QUERYRUNNER_ACTION_SHUTDOWN:
#ifdef DEBUG
                        printf("%s:%d: thread:shutdown\n", /*@nocommit@*/
                               __FILE__,
                               __LINE__
                               );
#endif
                        shutdown=TRUE;
                        /* fallthrough */

                case CATALOG_QUERYRUNNER_ACTION_DISCONNECT:
#ifdef DEBUG
                        printf("%s:%d: thread:disconnect\n", /*@nocommit@*/
                               __FILE__,
                               __LINE__
                               );
#endif
                        if(catalog_is_connected(catalog)) {
                                catalog_disconnect(catalog);
                        }
                        break;
                }
                catalog_queryrunner_msg_free(msg);
        } while(!shutdown);

        g_async_queue_unref(queryrunner->incoming);

#ifdef DEBUG
        printf("%s:%d thread end\n", __FILE__, __LINE__);
#endif
        return NULL;
}

static gboolean result_callback(struct catalog *catalog,
                                const struct catalog_query_result *qresult,
                                void *userdata)
{
        const struct catalog_entry *entry = &qresult->entry;
        struct thread_data *data;
        struct catalog_queryrunner *queryrunner;
        struct launcher *launcher;
        const char *query;
        struct result *result;
        int count;

        g_return_val_if_fail(userdata!=NULL, FALSE);
        g_return_val_if_fail(entry!=NULL, FALSE);

        data = (struct thread_data *)userdata;
        queryrunner = data->queryrunner;
        g_return_val_if_fail(userdata, FALSE);

        launcher = launchers_get(entry->launcher);
        if(!launcher)
        {
                /* it can happen, because launchers may be removed, but it's
                 * unusual and it might be ok. However, after an launcher is removed,
                 * the catalog should be cleaned. I want to know about unclean
                 * catalogs.
                 */
                g_warning("unclean catalog: no launcher for source referenced "
                          "in catalog with source_id=%d launcher=%s\n",
                          entry->source_id, entry->launcher);
                return TRUE;
        }

        printf("%s:%d: create result: %s, %d\n", /*@nocommit@*/
               __FILE__,
               __LINE__,
               launcher->id,
               qresult->id
               );

        result = catalog_result_create(queryrunner->path,
                                       launcher,
                                       qresult);
        query = data->running_query;
#ifdef DEBUG

        printf("%s:%d:query(%s) add %s\n",
               __FILE__, __LINE__, query, entry->path);
#endif

        result_queue_add(queryrunner->queue,
                         QUERYRUNNER(queryrunner),
                         query,
                         qresult->pertinence,
                         result);
        count = data->count;
        count++;
        data->count=count;
        if(count>=MAXIMUM) {
                return FALSE;
        }

        if(count==FIRST_BUNCH_SIZE) {
#ifdef DEBUG
                printf("%s:%d:query(%s) wait (1st bunch)\n",
                       __FILE__, __LINE__, query);
#endif
                if(catalog_queryrunner_msg_wait(data, AFTER_FIRST_BUNCH_TIMEOUT)) {
                        return FALSE;
                }
#ifdef DEBUG

                printf("%s:%d:query(%s) wait (1st bunch) done\n",
                       __FILE__, __LINE__, query);
#endif

        } else if(count%LATER_BUNCH_SIZE==0) {
#ifdef DEBUG
                printf("%s:%d:query(%s) wait (later bunch)\n",
                       __FILE__, __LINE__, query);
#endif

                if(catalog_queryrunner_msg_wait(data, LATER_BUNCH_TIMEOUT)) {
                        return FALSE;
                }
#ifdef DEBUG

                printf("%s:%d:query(%s) wait (later bunch) done\n",
                       __FILE__, __LINE__, query);
#endif
        }

        return TRUE;
}

static void catalog_queryrunner_msg_send(struct catalog_queryrunner *self, enum CatalogQueryrunnerMessageAction action, const char *query)
{
        struct catalog_queryrunner_msg *msg;

        g_return_if_fail(self);

        msg = g_new(struct catalog_queryrunner_msg, 1);
        msg->action = action;
        msg->query = query ? g_strdup(query):NULL;


#ifdef DEBUG
        printf("%s:%d:push(%d, %s)\n",
               __FILE__, __LINE__, action, query);
#endif
        g_async_queue_lock(self->incoming);
        {
                g_async_queue_push_unlocked(self->incoming, msg);
                catalog_interrupt(self->catalog);

        }
        g_async_queue_unlock(self->incoming);

#ifdef DEBUG
        printf("%s:%d:pushed(%d, %s)\n",
               __FILE__, __LINE__, action, query);
#endif
}
static void catalog_queryrunner_msg_free(struct catalog_queryrunner_msg *msg)
{
        g_return_if_fail(msg);

        if(msg->query) {
                g_free((char *)msg->query);
        }
        g_free(msg);
}

static struct catalog_queryrunner_msg *catalog_queryrunner_msg_next(struct thread_data *data)
{
        struct catalog_queryrunner_msg *msg;
        GAsyncQueue *queue;

#ifdef DEBUG
        printf("%s:%d:pop())\n",
               __FILE__, __LINE__);
#endif
        queue = data->queryrunner->incoming;

        g_async_queue_lock(queue);
        if(data->next_msg) {
                msg=data->next_msg;
                data->next_msg=NULL;
        } else {
                msg=g_async_queue_pop_unlocked(queue);
        }

        while(msg->action==CATALOG_QUERYRUNNER_ACTION_QUERY) {
                struct catalog_queryrunner_msg *msg2;
                msg2 = (struct catalog_queryrunner_msg *)g_async_queue_try_pop_unlocked(queue);
                if(msg2==NULL) {
                        break;
                } else {
                        catalog_queryrunner_msg_free(msg);
                        msg=msg2;
                }
        }
        g_async_queue_unlock(queue);
#ifdef DEBUG
        printf("%s:%d:poped(): %d, %s\n",
               __FILE__, __LINE__, msg->action, msg->query);
#endif

        return msg;
}

/**
 * Ideally there would be a message I could send through the queue
 * to tell the UI there was something wrong.
 */
static void handle_thread_error(struct catalog_queryrunner *self)
{
        g_return_if_fail(self);
        fprintf(stderr,
                "SQL ERROR: %s\n",
                catalog_error(self->catalog));
}

/**
 * Wait for a message.
 *
 * This function will wait for a message and return TRUE if one was found at
 * the end of the timeout. The message will be kept inside the thread data
 *
 * @param data thread data
 * @param timeout timeout, in milliseconds
 * @return TRUE if a message was found on the queue
 */
static gboolean catalog_queryrunner_msg_wait(struct thread_data *data, unsigned long timeout)
{
        struct catalog_queryrunner_msg *msg;
        GTimeVal tv;

        if(data->next_msg) {
                return TRUE;
        }

        g_get_current_time(&tv);
        g_time_val_add(&tv, timeout*1000);

        msg = (struct catalog_queryrunner_msg *)g_async_queue_timed_pop(data->queryrunner->incoming, &tv);
        if(msg!=NULL) {
                data->next_msg=msg;
                return TRUE;
        }
        return FALSE;
}
