
#include "indexer_applications.h"
#include "indexer_utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-util.h>
#include "desktop_file.h"


/**
 * \file index applications with a .desktop file
 */

static struct indexer_source *load(struct indexer *self, struct catalog *catalog, int id);
static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err);
static gboolean validate(struct indexer *, const char *name, const char *long_name, const char *path);
static gboolean index(struct indexer_source *, struct catalog *, GError **);
static void release(struct indexer_source *);
static gboolean discover(struct indexer *, struct catalog *catalog);

#define INDEXER_NAME "applications"
struct indexer indexer_applications =
{
   .name=INDEXER_NAME,
   .load_source=load,
   .execute=execute,
   .validate=validate,
   .discover=discover,
};

static struct indexer_source *load(struct indexer *self, struct catalog *catalog, int id)
{
   struct indexer_source *retval = g_new(struct indexer_source, 1);
   retval->id=id;
   retval->index=index;
   retval->release=release;
}
static void release(struct indexer_source *source)
{
   g_return_if_fail(source);
   g_free(source);
}

static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *uri, GError **err)
{
   g_return_val_if_fail(self!=NULL, FALSE);
   g_return_val_if_fail(name!=NULL, FALSE);
   g_return_val_if_fail(long_name!=NULL, FALSE);
   g_return_val_if_fail(uri!=NULL, FALSE);
   g_return_val_if_fail(g_str_has_prefix(uri, "file://"), FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   GError *gnome_err = NULL;

   gboolean retval = FALSE;
   GnomeDesktopFile *desktopfile = gnome_desktop_file_load(&uri[strlen("file://")],
                                                           &gnome_err);
   if(desktopfile==NULL)
      {
         g_set_error(err,
                     RESULT_ERROR,
                     RESULT_ERROR_INVALID_RESULT,
                     "entry for application %s not found in %s: %s",
                     name,
                     uri,
                     gnome_err->message);
         g_error_free(gnome_err);
      }
   else
      {
         char *exec = NULL;
         gnome_desktop_file_get_string(desktopfile,
                                       "Desktop Entry",
                                       "Exec",
                                       &exec);
         gboolean terminal = FALSE;
         gnome_desktop_file_get_boolean(desktopfile,
                                        "Desktop Entry",
                                        "Terminal",
                                        &terminal);


         if(!exec)
            {
               g_set_error(err,
                           RESULT_ERROR,
                           RESULT_ERROR_INVALID_RESULT,
                           "entry for application %s found in %s has no 'Exec' defined",
                           name,
                           uri);
            }
         else
            {
               int pid;
               printf("execute: '%s' terminal=%d\n",
                      exec,
                      (int)terminal);
               errno=0;
               if(terminal)
                  pid=gnome_execute_terminal_shell(NULL, exec);
               else
                  pid=gnome_execute_shell(NULL, exec);
               if(pid==-1)
                  {
                     g_set_error(err,
                                 RESULT_ERROR,
                                 RESULT_ERROR_MISSING_RESOURCE,
                                 "launching '%s' failed: %s",
                                 exec,
                                 errno!=0 ? strerror(errno):"unknown error");
                  }
               else
                  {
                     printf("started application %s with PID %d\n",
                            exec,
                            pid);
                     retval=TRUE;
                  }
            }
         if(exec)
            g_free(exec);
         gnome_desktop_file_free(desktopfile);
      }
   return retval;
}

static gboolean validate(struct indexer *self, const char *name, const char *long_name, const char *text_uri)
{
   g_return_val_if_fail(text_uri!=NULL, FALSE);
   g_return_val_if_fail(self==&indexer_applications, FALSE);
   return uri_exists(text_uri);
}


static gboolean index_application_cb(struct catalog *catalog,
                                     int source_id,
                                     const char *path,
                                     const char *filename,
                                     GError **err,
                                     gpointer userdata)
{
   if(!g_str_has_suffix(filename, ".desktop"))
      return TRUE;

   char uri[strlen("file://")+strlen(path)+1];
   strcpy(uri, "file://");
   strcat(uri, path);

   GnomeDesktopFile *desktopfile = gnome_desktop_file_load(path,
                                                           NULL/*ignore errors*/);
   if(desktopfile==NULL)
      return TRUE;

   char *type = NULL;
   gnome_desktop_file_get_string(desktopfile,
                                 "Desktop Entry",
                                 "Type",
                                 &type);
   char *name = NULL;
   gnome_desktop_file_get_string(desktopfile,
                                 "Desktop Entry",
                                 "Name",
                                 &name);
   char *comment = NULL;
   gnome_desktop_file_get_string(desktopfile,
                                 "Desktop Entry",
                                 "Comment",
                                 &comment);

   char *generic_name = NULL;
   gnome_desktop_file_get_string(desktopfile,
                                 "Desktop Entry",
                                 "GenericName",
                                 &generic_name);
   gboolean nodisplay = FALSE;
   gnome_desktop_file_get_boolean(desktopfile,
                                  "Desktop Entry",
                                  "NoDisplay",
                                  &nodisplay);

   gboolean hidden = FALSE;
   gnome_desktop_file_get_boolean(desktopfile,
                                  "Desktop Entry",
                                  "Hidden",
                                  &hidden);

   char *exec = NULL;
   gnome_desktop_file_get_string(desktopfile,
                                 "Desktop Entry",
                                 "Exec",
                                 &exec);

   char *description=NULL;
   if(comment && generic_name)
      description=g_strdup_printf("%s (%s)", comment, generic_name);

   gboolean retval = TRUE;
   if((type==NULL || g_strcasecmp("Application", type)==0)
      && !hidden
      && !nodisplay
      && exec!=NULL
      && strstr(exec, "%")==NULL)
      {
         const char *long_name=description;
         if(!long_name)
            long_name=comment;
         if(!long_name)
            long_name=generic_name;
         if(!long_name)
            long_name=path;
         retval=catalog_addentry_witherrors(catalog,
                                            uri,
                                            name==NULL ? filename:name,
                                            long_name,
                                            source_id,
                                            err);
      }

   if(description)
      g_free(description);
   if(comment)
      g_free(comment);
   if(generic_name)
      g_free(generic_name);
   if(name)
      g_free(name);
   if(exec)
      g_free(exec);

   gnome_desktop_file_free(desktopfile);
   return retval;
}

static gboolean index(struct indexer_source *self, struct catalog *catalog, GError **err)
{
   g_return_val_if_fail(self!=NULL, FALSE);
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   return index_recursively(catalog,
                            self->id,
                            index_application_cb,
                            self/*userdata*/,
                            err);
}

static gboolean add_source(struct catalog *catalog, const char *path, int depth, char *ignore)
{
   int id;
   if(!catalog_add_source(catalog, INDEXER_NAME, &id))
      return FALSE;
   if(!catalog_set_source_attribute(catalog, id, "path", path))
      return FALSE;
   if(depth!=-1)
      {
         char *depth_str = g_strdup_printf("%d", depth);
         gboolean ret = catalog_set_source_attribute(catalog, id, "depth", depth_str);
         g_free(depth_str);
         return ret;
      }
   if(ignore)
      {
         if(catalog_set_source_attribute(catalog, id, "ignore", ignore))
            return FALSE;
      }
   return TRUE;
}

static gboolean add_source_for_directory(struct catalog *catalog, const char *possibility)
{
   if(g_file_test(possibility, G_FILE_TEST_EXISTS)
      && g_file_test(possibility, G_FILE_TEST_IS_DIR))
      {
         return add_source(catalog,
                           possibility,
                           10/*depth*/,
                           "locale:man:themes:doc:fonts:perl:pixmaps");
      }
   return TRUE;
}
static gboolean discover(struct indexer *indexer, struct catalog *catalog)
{
   gboolean retval = FALSE;
   char *home = gnome_util_prepend_user_home(".local/share");
   char *possibilities[] =
      {
         home,
         "/usr/share",
         "/usr/local/share",
         "/opt/gnome/share",
         "/opt/kde/share",
         "/opt/kde2/share",
         "/opt/kde3/share",
         NULL
      };

   for(char **ptr=possibilities; *ptr; ptr++)
      {
         gboolean ok=TRUE;
         char *possibility = *ptr;
         if(!add_source_for_directory(catalog, possibility))
            goto error;
      }
   retval=TRUE;
 error:
   g_free(home);
   return retval;
}
