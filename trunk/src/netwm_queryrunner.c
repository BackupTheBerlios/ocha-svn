#include "netwm_queryrunner.h"
#include "result.h"
#include "query.h"
#include "netwm.h"
#include <glib.h>
#include <gdk/gdk.h>
#include <stdio.h>

/** \file Implementation of the API defined in netwm_queryrunner.h */

/** info about a window that can be used later to generate a result */
struct window_info
{
        Window win;
        const char *title;
        const char *description;
};

/** Private extension of the structure queryrunner */
struct netwm_queryrunner
{
        struct queryrunner base;

        Display *display;
        struct result_queue *queue;

        /** array of struct window_inf, window list, set by start(), cleared by stop() */
        GArray *window_infos;
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

/* ------------------------- prototypes: netwm_queryrunner */
static void netwm_queryrunner_start(struct queryrunner *self);
static void netwm_queryrunner_run_query(struct queryrunner *self, const char *query);
static void netwm_queryrunner_consolidate(struct queryrunner *self);
static void netwm_queryrunner_stop(struct queryrunner *self);
static void netwm_queryrunner_release(struct queryrunner *self);

/* ------------------------- prototypes: netwm_queryrunner_result */

static struct result *netwm_queryrunner_result_new(Display *display, Window win, const char *title, const char *description);
static gboolean netwm_queryrunner_result_execute(struct result *self, GError **);
static gboolean netwm_queryrunner_result_validate(struct result *self);
static void netwm_queryrunner_result_release(struct result *self);

/* ------------------------- prototypes: other */

static void clear_windows_list(struct netwm_queryrunner *self);


/* ------------------------- public functions */
struct queryrunner *netwm_queryrunner_create(Display *display,
                                             struct result_queue *queue)
{
        struct netwm_queryrunner *retval = g_new(struct netwm_queryrunner, 1);

        retval->base.start=netwm_queryrunner_start;
        retval->base.stop=netwm_queryrunner_stop;
        retval->base.run_query=netwm_queryrunner_run_query;
        retval->base.consolidate=netwm_queryrunner_consolidate;
        retval->base.release=netwm_queryrunner_release;
        retval->display=display;
        retval->queue=queue;
        retval->window_infos=g_array_new(FALSE/*not zero_terminated*/,
                                         TRUE/*clear*/,
                                         sizeof(struct window_info));
        return TO_QUERYRUNNER(retval);
}

/* ------------------------- member functions: netwm_queryrunner */
static void netwm_queryrunner_start(struct queryrunner *_self)
{
        struct netwm_queryrunner *self = TO_NETWM_QUERYRUNNER(_self);
        g_return_if_fail(self);

        if(self->window_infos->len>0)
                return;

        unsigned long len=0;
        Window *windows=netwm_get_client_list(self->display, &len);

        if(len>0)
        {
                for(int i=0; i<len; i++) {
                        Window win = windows[i];
                        gdk_error_trap_push();
                        const char *title = netwm_get_window_title(self->display, win);
                        const char *class = netwm_get_window_class(self->display, win);
                        const char *host = netwm_get_window_host(self->display, win);
                        gdk_flush();
                        if(gdk_error_trap_pop()==0) {
                                struct window_info info;
                                info.win=win;
                                info.title=g_strdup(title);
                                info.description=g_strdup_printf("Window \"%s\" "
                                                                 "of class %s on %s (0x%lx)",
                                                                 title,
                                                                 class,
                                                                 host,
                                                                 (unsigned long)win);
                                g_array_append_val(self->window_infos, info);
                        }
                }
        }

        if(windows)
                g_free(windows);

}
static void netwm_queryrunner_stop(struct queryrunner *_self)
{
        struct netwm_queryrunner *self = TO_NETWM_QUERYRUNNER(_self);
        g_return_if_fail(self);

        clear_windows_list(self);
}
static void netwm_queryrunner_run_query(struct queryrunner *_self, const char *query)
{
        struct netwm_queryrunner *self = TO_NETWM_QUERYRUNNER(_self);
        g_return_if_fail(self);
        g_return_if_fail(query);

        int len = self->window_infos->len;
        if(len==0) {
                return;
        }

        struct window_info *info = (struct window_info *)self->window_infos->data;
        for(int i=0; i<len; i++, info++) {
                Window win = info->win;
                const char *title = info->title;
                if(query_ismatch(query, title)) {
                        result_queue_add(self->queue,
                                         TO_QUERYRUNNER(self),
                                         query,
                                         0.5/*pertinence*/,
                                         netwm_queryrunner_result_new(self->display,
                                                    win,
                                                    title,
                                                    info->description));
                }
        }
}

static void netwm_queryrunner_consolidate(struct queryrunner *self)
{}

static void netwm_queryrunner_release(struct queryrunner *_self)
{
        struct netwm_queryrunner *self = TO_NETWM_QUERYRUNNER(_self);
        g_return_if_fail(self);
        clear_windows_list(self);
        g_array_free(self->window_infos, TRUE/*free content*/);
        g_free(self);
}

/* ------------------------- member functions: netwm_queryrunner_result */
static struct result *netwm_queryrunner_result_new(Display *display,
                                                   Window win,
                                                   const char *title,
                                                   const char *description)
{
        g_return_val_if_fail(display, NULL);
        g_return_val_if_fail(title, NULL);

        struct window_result *retval = g_new(struct window_result, 1);

        retval->base.name=g_strdup(title);
        retval->base.long_name=g_strdup(description);
        retval->base.path=g_strdup_printf("x-win:0x%lx", (unsigned long)win);
        retval->base.execute=netwm_queryrunner_result_execute;
        retval->base.validate=netwm_queryrunner_result_validate;
        retval->base.release=netwm_queryrunner_result_release;
        retval->display=display;
        retval->win=win;

        return TO_RESULT(retval);
}

static gboolean netwm_queryrunner_result_execute(struct result *_self,
                                                 GError **err)
{
        struct window_result *self = TO_WINDOW_RESULT(_self);
        g_return_val_if_fail(self, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

        printf("activate window %s '%s'\n",
               self->base.path,
               self->base.name);
        gboolean retval;
        gdk_error_trap_push();
        retval=netwm_activate_window(self->display,
                                     self->win,
                                     TRUE/*switch desktop*/);
        gdk_flush();
        gint xerror = gdk_error_trap_pop();
        if(xerror)
        {
                g_set_error(err,
                            RESULT_ERROR,
                            xerror==BadWindow
                            ? RESULT_ERROR_INVALID_RESULT
                            : RESULT_ERROR_MISSING_RESOURCE,
                            "window activation failed (X error %d)\n",
                            xerror);
                return FALSE;
        }
        return retval;
}

static gboolean netwm_queryrunner_result_validate(struct result *_self)
{
        gboolean retval = TRUE;
        struct window_result *self = TO_WINDOW_RESULT(_self);
        g_return_val_if_fail(self, FALSE);

        gdk_error_trap_push();
        char *title = netwm_get_window_title(self->display, self->win);
        if(title==NULL)
                retval=FALSE;
        else
                g_free(title);
        gdk_flush();
        if(gdk_error_trap_pop())
                retval=FALSE;

        return retval;
}

static void netwm_queryrunner_result_release(struct result *_self)
{
        struct window_result *self = TO_WINDOW_RESULT(_self);
        g_return_if_fail(self);

        g_free((void *)self->base.name);
        g_free((void *)self->base.long_name);
        g_free((void *)self->base.path);
        g_free(self);
}

/* ------------------------- static functions */

static void clear_windows_list(struct netwm_queryrunner *self)
{
        GArray *array = self->window_infos;
        int len = array->len;
        if(len>0) {
                struct window_info *info = (struct window_info *)array->data;
                for(int i=0; i<len; i++, info++) {
                        g_free((void *)info->title);
                        g_free((void *)info->description);
                }
                g_array_set_size(array, 0);
        }
}

