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
   const char *execute;
   const char *catalog_path;

   /** buffer for the path, catalog path, display name and execute string */
   char buffer[];
};

static gboolean catalog_result_validate(struct result *_self);
static void catalog_result_free(struct result *self);
static gboolean catalog_result_execute(struct result *_self, GError **);
static gboolean exists(const char *path);
static void execute_parse(GString *gstr, const char *pattern, const char *path);

/* ------------------------- public function */
struct result *catalog_result_create(const char *catalog_path, const char *path, const char *name, const char *long_name, const char *execute, int entry_id)
{
   g_return_val_if_fail(catalog_path, NULL);
   g_return_val_if_fail(path, NULL);
   g_return_val_if_fail(name, NULL);
   g_return_val_if_fail(execute, NULL);

   struct catalog_result *result = g_malloc(sizeof(struct catalog_result)
                                            +strlen(path)+1
                                            +strlen(name)+1
                                            +strlen(long_name)+1
                                            +strlen(execute)+1
                                            +strlen(catalog_path)+1);
   result->entry_id=entry_id;
   char *buf = result->buffer;

   result->base.path=buf;
   strcpy(buf, path);
   buf+=strlen(path)+1;

   result->base.name=buf;
   strcpy(buf, name);
   buf+=strlen(name)+1;

   result->base.long_name=buf;
   strcpy(buf, long_name);
   buf+=strlen(long_name)+1;

   result->execute=buf;
   strcpy(buf, execute);
   buf+=strlen(execute)+1;

   result->catalog_path=buf;
   strcpy(buf, catalog_path);

   result->base.execute=catalog_result_execute;
   result->base.validate=catalog_result_validate;
   result->base.release=catalog_result_free;

   return &result->base;
}



/* ------------------------- private functions */
static gboolean exists(const char *path)
{
   struct stat buf;
   return stat(path, &buf)==0 && buf.st_size>0;
}

static gboolean catalog_result_validate(struct result *_self)
{
   struct catalog_result *self = (struct catalog_result *)_self;
   g_return_val_if_fail(self, FALSE);

   if(self->base.path[0]=='/')
      {
         /* it's a file */
         gboolean retval = exists(self->base.path);
         if(!retval)
            printf("invalid result: %s\n", self->base.path);
         return retval;
      }
   /* otherwise It's an URL and I can't check that... */
   return TRUE;
}
static gboolean catalog_result_execute(struct result *_self, GError **err)
{
   struct catalog_result *self = (struct catalog_result *)_self;
   g_return_val_if_fail(self, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   pid_t pid = fork();
   if(pid<0)
      {
         g_set_error(err,
                     RESULT_ERROR,
                     RESULT_ERROR_MISSING_RESOURCE,
                     "failed at fork(): %s\n",
                     strerror(errno));
         return FALSE;
      }

   if(pid==0)
      {
         /* the child */
         GString *gstr = g_string_new("");
         execute_parse(gstr, self->execute, self->base.path);
         printf("execute: /bin/sh -c '%s'\n", gstr->str);
         execl("/bin/sh", "/bin/sh", "-c", gstr->str, NULL);
         fprintf(stderr, "result.execute() failed at exec(%s): %s\n",
                 gstr->str,
                 strerror(errno));
         exit(99);
      }
   /* the parent */

   struct catalog *catalog = catalog_connect(self->catalog_path, NULL/*errmsg*/);
   if(catalog)
      {
         if(!catalog_update_entry_timestamp(catalog, self->entry_id))
            {
               fprintf(stderr,
                       "warning: lastuse timestamp update failed: %s\n",
                       catalog_error(catalog));
            }
         catalog_disconnect(catalog);
      }

   return TRUE;
}

static void catalog_result_free(struct result *self)
{
   g_return_if_fail(self);
   g_free(self);
}

/**
 * Replace %f with the path.
 *
 * @param pattern pattern with %f
 * @param path path to replace %f with
 * @return a string to free with g_free
 */
static void execute_parse(GString *gstr, const char *pattern, const char *path)
{
   const char *current=pattern;
   const char *found;
   while( (found=strstr(current, "%f")) != NULL )
      {
         gssize len = found-current;
         g_string_append_len(gstr,
                             current,
                             len);
         current+=len+2;
         g_string_append(gstr, path);
      }
   g_string_append(gstr, current);
}
