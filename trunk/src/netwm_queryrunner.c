#include "netwm_queryrunner.h"
#include "result.h"
#include "query.h"
#include "netwm.h"
#include <glib.h>
#include <gdk/gdk.h>

/** \file Implementation of the API defined in netwm_queryrunner.h */

/** Private extension of the structure queryrunner */
struct netwm_queryrunner
{
   struct queryrunner base;

   Display *display;
   struct result_queue *queue;

   /** window list, set by start(), cleared by stop() */
   Window *windows;

   /** array of UTF8 window titles, to be freed by g_free() of the same length as windows, some may be null */
   char **titles;

   /** length of the window list. windows==NULL <=> windows_len==0 */
   unsigned long windows_len;
};

/** struct netwm_queryrunner to struct queryrunner */
#define TO_QUERYRUNNER(q) (&((q)->base))
/** struct queryrunner to struct netwm_queryrunner */
#define TO_NETWM_QUERYRUNNER(q) ((struct netwm_queryrunner *)(q))

/** A result corresponding to a window */
struct window_result
{
   struct result base;
   Display *display;
   Window win;
};
/** window_result -> result */
#define TO_RESULT(wr) (&((wr)->base))
/** result -> window_result */
#define TO_WINDOW_RESULT(r) ((struct window_result *)(r))

static void clear_windows_list(struct netwm_queryrunner *self);

static void start(struct queryrunner *self);
static void run_query(struct queryrunner *self, const char *query);
static void consolidate(struct queryrunner *self);
static void stop(struct queryrunner *self);
static void release(struct queryrunner *self);

static struct result *result_new(Display *display, Window win, const char *title);
static bool result_execute(struct result *self);
static void result_release(struct result *self);

/* ------------------------- public functions */
struct queryrunner *netwm_queryrunner_create(Display *display, struct result_queue *queue)
{
   struct netwm_queryrunner *retval = g_new(struct netwm_queryrunner, 1);

   retval->base.start=start;
   retval->base.stop=stop;
   retval->base.run_query=run_query;
   retval->base.consolidate=consolidate;
   retval->base.release=release;
   retval->display=display;
   retval->queue=queue;
   retval->windows=NULL;
   retval->windows_len=0;
   retval->titles=NULL;
   return TO_QUERYRUNNER(retval);
}

/* ------------------------- static */
static void clear_windows_list(struct netwm_queryrunner *self)
{
   if(self->windows)
      {
         g_free(self->windows);
         self->windows=NULL;
      }
   if(self->titles)
      {
         unsigned long len = self->windows_len;
         for(int i=0; i<len; i++)
            g_free(self->titles[i]);
         g_free(self->titles);
         self->titles=NULL;
      }
   self->windows_len=0;
}

/* ------------------------- queryrunner */
static void start(struct queryrunner *_self)
{
   struct netwm_queryrunner *self = TO_NETWM_QUERYRUNNER(_self);
   g_return_if_fail(self);

   if(self->windows)
      return;

   unsigned long len=0;

   self->windows=netwm_get_client_list(self->display, &len);
   self->windows_len=len;
   if(self->windows_len==0)
      clear_windows_list(self);
   else
      {
         self->titles=g_malloc(sizeof(char *)*len);
         for(unsigned long i=0; i<len; i++)
            {
               gdk_error_trap_push();
               self->titles[i]=netwm_get_window_title(self->display,
                                                      self->windows[i]);
               if(gdk_error_trap_pop())
                  {
                     self->titles[i]=NULL;
                  }
            }
      }
}
static void stop(struct queryrunner *_self)
{
   struct netwm_queryrunner *self = TO_NETWM_QUERYRUNNER(_self);
   g_return_if_fail(self);

   clear_windows_list(self);
}
static void run_query(struct queryrunner *_self, const char *query)
{
   struct netwm_queryrunner *self = TO_NETWM_QUERYRUNNER(_self);
   g_return_if_fail(self);
   g_return_if_fail(query);

   Window *wins = self->windows;
   char **titles = self->titles;
   unsigned long len = self->windows_len;
   for(unsigned long i=0; i<len; i++)
      {
         Window win = wins[i];
         const char *title = titles[i];
         if(!title)
            continue;
         if(query_ismatch(query, title))
            {
               result_queue_add(self->queue,
                                TO_QUERYRUNNER(self),
                                query,
                                0.5/*pertinence*/,
                                result_new(self->display, win, title));
            }
      }

}
static void consolidate(struct queryrunner *self)
{
}
static void release(struct queryrunner *_self)
{
   struct netwm_queryrunner *self = TO_NETWM_QUERYRUNNER(_self);
   g_return_if_fail(self);
   clear_windows_list(self);
   g_free(self);
}

/* ------------------------- result */
static struct result *result_new(Display *display, Window win, const char *title)
{
   g_return_val_if_fail(display, NULL);
   g_return_val_if_fail(title, NULL);

   struct window_result *retval = g_new(struct window_result, 1);

   retval->base.name=g_strdup(title);
   retval->base.path=g_strdup_printf("x-win:0x%lx", (unsigned long)win);
   retval->base.execute=result_execute;
   retval->base.release=result_release;
   retval->display=display;
   retval->win=win;

   return TO_RESULT(retval);
}

static bool result_execute(struct result *_self)
{
   struct window_result *self = TO_WINDOW_RESULT(_self);
   g_return_val_if_fail(self, false);

   return netwm_activate_window(self->display,
                                self->win,
                                true/*switch desktop*/);
}
static void result_release(struct result *_self)
{
   struct window_result *self = TO_WINDOW_RESULT(_self);
   g_return_if_fail(self);

   g_free((void *)self->base.name);
   g_free((void *)self->base.path);
   g_free(self);
}
