#include "compound_queryrunner.h"
#include <glib.h>

/** \file implementation of the APi defined in compound_queryrunner.h */

/** query runner structure with private fields */
struct compound_queryrunner
{
        struct queryrunner base;
        int queryrunners_len;
        struct queryrunner *queryrunners[];
};

/** compound_queryrunner -> queryrunner */
#define TO_QUERYRUNNER(cq) (&((cq)->base))
/** queryrunner -> compound_queryrunner */
#define TO_COMPOUND_QUERYRUNNER(q) ((struct compound_queryrunner *)(q))

/* ------------------------- prototypes */
static void compound_queryrunner_start(struct queryrunner *self);
static void compound_queryrunner_stop(struct queryrunner *self);
static void compound_queryrunner_run_query(struct queryrunner *self, const char *query);
static void compound_queryrunner_consolidate(struct queryrunner *self);
static void compound_queryrunner_release(struct queryrunner *self);


/* ------------------------- public functions */
struct queryrunner *compound_queryrunner_new(struct queryrunner **queryrunners, int queryrunners_len)
{
        struct compound_queryrunner *retval = g_malloc(sizeof(struct compound_queryrunner)+queryrunners_len*sizeof(struct queryrunner *));
        retval->base.start=compound_queryrunner_start;
        retval->base.stop=compound_queryrunner_stop;
        retval->base.run_query=compound_queryrunner_run_query;
        retval->base.consolidate=compound_queryrunner_consolidate;
        retval->base.release=compound_queryrunner_release;
        retval->queryrunners_len=queryrunners_len;
        for(int i=0; i<queryrunners_len; i++)
                retval->queryrunners[i]=queryrunners[i];

        return TO_QUERYRUNNER(retval);
}

/* ------------------------- member functions */
static void compound_queryrunner_start(struct queryrunner *_self)
{
        struct compound_queryrunner *self = TO_COMPOUND_QUERYRUNNER(_self);
        g_return_if_fail(self);

        int len = self->queryrunners_len;
        for(int i=0; i<len; i++)
        {
                struct queryrunner *runner = self->queryrunners[i];
                runner->start(runner);
        }
}
static void compound_queryrunner_stop(struct queryrunner *_self)
{
        struct compound_queryrunner *self = TO_COMPOUND_QUERYRUNNER(_self);
        g_return_if_fail(self);

        int len = self->queryrunners_len;
        for(int i=0; i<len; i++)
        {
                struct queryrunner *runner = self->queryrunners[i];
                runner->stop(runner);
        }
}
static void compound_queryrunner_run_query(struct queryrunner *_self, const char *query)
{
        struct compound_queryrunner *self = TO_COMPOUND_QUERYRUNNER(_self);
        g_return_if_fail(self);

        int len = self->queryrunners_len;
        for(int i=0; i<len; i++)
        {
                struct queryrunner *runner = self->queryrunners[i];
                runner->run_query(runner, query);
        }

}
static void compound_queryrunner_consolidate(struct queryrunner *_self)
{
        struct compound_queryrunner *self = TO_COMPOUND_QUERYRUNNER(_self);
        g_return_if_fail(self);

        int len = self->queryrunners_len;
        for(int i=0; i<len; i++)
        {
                struct queryrunner *runner = self->queryrunners[i];
                runner->consolidate(runner);
        }
}
static void compound_queryrunner_release(struct queryrunner *_self)
{
        struct compound_queryrunner *self = TO_COMPOUND_QUERYRUNNER(_self);
        g_return_if_fail(self);

        int len = self->queryrunners_len;
        for(int i=0; i<len; i++)
        {
                struct queryrunner *runner = self->queryrunners[i];
                runner->release(runner);
        }
        g_free(self);
}

/* ------------------------- static functions */

