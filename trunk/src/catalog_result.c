#include "catalog_result.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

/** \file implementation of the private catalog-based result implementation defined in catalog_result.h */

/**
 * Full structure of the results passed to the callback
 */
struct catalog_result
{
        /** catalog_result are result */
        struct result base;
        int entry_id;
        struct launcher *launcher;
        const char *catalog_path;
};

/* ------------------------- prototypes */

static gboolean catalog_result_validate(struct result *_self);
static void update_entry_timestamp(const char *catalog_path, int entry_id);
static gboolean catalog_result_execute(struct result *_self, GError **err);
static void catalog_result_free(struct result *self);

/* ------------------------- public function */

struct result *catalog_result_create(const char *catalog_path,
                                     struct launcher *launcher,
                                     const char *path,
                                     const char *name,
                                     const char *long_name,
                                     int entry_id)
{
        struct catalog_result *result;

        g_return_val_if_fail(catalog_path, NULL);
        g_return_val_if_fail(path, NULL);
        g_return_val_if_fail(name, NULL);
        g_return_val_if_fail(launcher, NULL);

        result =  g_new(struct catalog_result, 1);
        result->entry_id=entry_id;
        result->launcher=launcher;
        result->base.path=g_strdup(path);
        result->base.name=g_strdup(name);
        result->base.long_name=g_strdup(long_name);
        result->catalog_path=g_strdup(catalog_path);

        result->base.execute=catalog_result_execute;
        result->base.validate=catalog_result_validate;
        result->base.release=catalog_result_free;

        return &result->base;
}



/* ------------------------- static functions */
static gboolean catalog_result_validate(struct result *_self)
{
        struct catalog_result *self = (struct catalog_result *)_self;
        g_return_val_if_fail(self, FALSE);
        g_return_val_if_fail(self->launcher, FALSE);

        return launcher_validate(self->launcher,
                                 self->base.path);
}

static void update_entry_timestamp(const char *catalog_path, int entry_id)
{
        struct catalog *catalog = catalog_connect(catalog_path, NULL/*errmsg*/);
        if(catalog) {
                if(!catalog_update_entry_timestamp(catalog, entry_id)) {
                        fprintf(stderr,
                                "warning: lastuse timestamp update failed: %s\n",
                                catalog_error(catalog));
                }
                catalog_disconnect(catalog);
        }
}

static gboolean catalog_result_execute(struct result *_self, GError **err)
{
        struct catalog_result *self = (struct catalog_result *)_self;
        g_return_val_if_fail(self, FALSE);
        g_return_val_if_fail(self->launcher, FALSE);

        if(launcher_execute(self->launcher,
                            self->base.name,
                            self->base.long_name,
                            self->base.path,
                            err))
        {
                update_entry_timestamp(self->catalog_path, self->entry_id);
                return TRUE;
        }
        return FALSE;
}

static void catalog_result_free(struct result *_self)
{
        struct catalog_result *self = (struct catalog_result *)_self;
        g_return_if_fail(self);

        g_free((gpointer)self->base.path);
        g_free((gpointer)self->base.name);
        g_free((gpointer)self->base.long_name);
        g_free((gpointer)self->catalog_path);
        g_free(self);
}
