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

static void start(struct queryrunner *self);
static void run_query(struct queryrunner *self, const char *query);
static void consolidate(struct queryrunner *self);
static void stop(struct queryrunner *self);
static void release(struct queryrunner *self);


/* ------------------------- public functions */
struct queryrunner *compound_queryrunner_new(struct queryrunner **queryrunners, int queryrunners_len)
{
   struct compound_queryrunner *retval = g_malloc(sizeof(struct compound_queryrunner)+queryrunners_len*sizeof(struct queryrunner *));
   retval->base.start=start;
   retval->base.stop=stop;
   retval->base.run_query=run_query;
   retval->base.consolidate=consolidate;
   retval->base.release=release;
   retval->queryrunners_len=queryrunners_len;
   for(int i=0; i<queryrunners_len; i++)
      retval->queryrunners[i]=queryrunners[i];

   return TO_QUERYRUNNER(retval);
}

/* ------------------------- queryrunner */
static void start(struct queryrunner *_self)
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
static void stop(struct queryrunner *_self)
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
static void run_query(struct queryrunner *_self, const char *query)
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
static void consolidate(struct queryrunner *_self)
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
static void release(struct queryrunner *_self)
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
