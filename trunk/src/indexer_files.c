#include "indexer.h"
#include "indexer_utils.h"
#include "result.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>

/** index files that can be executed by gnome, using gnome_vfs.
 *
 * This is an implementation of the API defined in indexer.h.
 *
 * Get the indexer using the extern defined in indexer_files.h.
 *
 * @see indexer.h
 */
static struct indexer_source *load(struct indexer *self, struct catalog *catalog, int id);
static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err);
static gboolean validate(struct indexer *self, const char *name, const char *long_name, const char *path);
static gboolean index(struct indexer_source *self, struct catalog *catalog, GError **err);
static gboolean discover(struct indexer *, struct catalog *catalog);
static gboolean has_gnome_mime_command(const char *path);
static char *display_name(struct catalog *catalog, int id);
static void release_source(struct indexer_source *source);
#define INDEXER_NAME "files"

/** Definition of the indexer */
struct indexer indexer_files =
{
   .name = INDEXER_NAME,
   .display_name = "Files and Directories",
   .execute = execute,
   .validate = validate,
   .load_source = load,
   .discover = discover,
};

/* ------------------------- private functions */

/**
 * Load a source from the catalog.
 *
 * This function will look for the attribute 'path', which
 * must be set to the base directory and the attribute 'depth',
 * which might be set (defaults to -1, infinite depth)
 */
static struct indexer_source *load(struct indexer *self, struct catalog *catalog, int id)
{
   g_return_val_if_fail(catalog!=NULL, NULL);
   g_return_val_if_fail(self==&indexer_files, NULL);

   struct indexer_source *retval = g_new(struct indexer_source, 1);
   retval->id=id;
   retval->index=index;
   retval->release=release_source;
   retval->display_name=display_name(catalog, id);
   return retval;
}
static void release_source(struct indexer_source *source)
{
    g_return_if_fail(source!=NULL);
    g_free((gpointer)source->display_name);
    g_free(source);
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
static gboolean discover(struct indexer *indexer, struct catalog *catalog)
{
   char *home_Desktop = gnome_util_prepend_user_home("Desktop");
   char *home_dot_gnome_desktop = gnome_util_prepend_user_home(".gnome-desktop");
   const char *home = g_get_home_dir();
   gboolean has_desktop = FALSE;
   gboolean retval = FALSE;

   if(g_file_test(home_Desktop, G_FILE_TEST_EXISTS)
      && g_file_test(home_Desktop, G_FILE_TEST_IS_DIR))
      {
         if(!add_source(catalog, home_Desktop, 2/*depth*/, NULL/*ignore*/))
            goto error;
         has_desktop=TRUE;
      }

   if(g_file_test(home_dot_gnome_desktop, G_FILE_TEST_EXISTS)
      && g_file_test(home_dot_gnome_desktop, G_FILE_TEST_IS_DIR))
      {
         if(!add_source(catalog, home_dot_gnome_desktop, 2/*depth*/, NULL/*ignore*/))
            goto error;
         has_desktop=TRUE;
      }

   if(!has_desktop)
      {
         /* yes, I'm paranoid... */
         if(g_file_test(home, G_FILE_TEST_EXISTS)
            && g_file_test(home, G_FILE_TEST_IS_DIR))
            {
               if(!add_source(catalog, home, 2/*depth*/, "Desktop"))
                  goto error;
            }
      }
   retval=TRUE;
 error:
   g_free(home_Desktop);
   g_free(home_dot_gnome_desktop);
   /* not: g_free(home); */
   return retval;
}
static gboolean index_file_cb(struct catalog *catalog,
                              int source_id,
                              const char *path,
                              const char *filename,
                              GError **err,
                              gpointer userdata)
{

   if(!has_gnome_mime_command(path))
      return TRUE;

   char uri[strlen("file://")+strlen(path)+1];
   strcpy(uri, "file://");
   strcat(uri, path);

   return catalog_addentry_witherrors(catalog,
                                      uri,
                                      filename,
                                      path/*long_name*/,
                                      source_id,
                                      err);
}

/**
 * (re)index the directory of the source
 */
static gboolean index(struct indexer_source *self, struct catalog *catalog, GError **err)
{
   g_return_val_if_fail(self!=NULL, FALSE);
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   return index_recursively(catalog,
                            self->id,
                            index_file_cb,
                            self/*userdata*/,
                            err);
}

/**
 * Execute a file using gnome_vfs
 *
 * @param name file display name
 * @param long long file display name
 * @param text_uri file:// URL (file://<absolute path>) with three "/", then. (called 'path' elsewhere)
 * @param err error to set if execution failed
 * @return true if it worked
 */
static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *text_uri, GError **err)
{
   g_return_val_if_fail(text_uri!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);
   g_return_val_if_fail(self==&indexer_files, FALSE);

   if(!validate(self, name, long_name, text_uri))
      {
         g_set_error(err,
                     RESULT_ERROR,
                     RESULT_ERROR_INVALID_RESULT,
                     "file not found: %s",
                     text_uri);
         return FALSE;
      }

   printf("opening %s...\n", text_uri);
   GError *gnome_err = NULL;
   if(!gnome_url_show(text_uri, &gnome_err))
      {
         g_set_error(err,
                     RESULT_ERROR,
                     RESULT_ERROR_MISSING_RESOURCE,
                     "error opening %s: %s",
                     text_uri,
                     gnome_err->message);
         g_error_free(gnome_err);
         return FALSE;
      }
   return TRUE;
}

/**
 * Make sure the file still exists.
 *
 * @param name file display name
 * @param path long file display name, the path
 * @param text_uri file:// URL (file://<absolute path>) with three "/", then. (called 'path' elsewhere)
 */
static gboolean validate(struct indexer *self, const char *name, const char *long_name, const char *text_uri)
{
   g_return_val_if_fail(text_uri!=NULL, FALSE);
   g_return_val_if_fail(self==&indexer_files, FALSE);

   return uri_exists(text_uri);
}

static gboolean has_gnome_mime_command(const char *path)
{
   GString *uri = g_string_new("file://");
   if(*path!='/')
      {
         const char *cwd = g_get_current_dir();
         g_string_append(uri, cwd);
         g_free((void *)cwd);
         if(uri->str[uri->len-1]!='/')
            g_string_append_c(uri, '/');
      }
   g_string_append(uri, path);

   gboolean retval=FALSE;
   char *mimetype = gnome_vfs_get_mime_type(uri->str);
   if(mimetype)
      {
         char *app = gnome_vfs_mime_get_default_desktop_entry(mimetype);
         if(app)
            {
               g_free(app);
               retval=TRUE;
            }
         g_free(mimetype);
      }
   g_string_free(uri, TRUE/*free content*/);
   return retval;
}

static char *display_name(struct catalog *catalog, int id)
{
    char *uri=NULL;
    char *retval=NULL;
    if(!catalog_get_source_attribute(catalog, id, "path", &uri) || uri==NULL)
        retval=g_strdup("Invalid");
    else
    {
        char *path=NULL;
        if(g_str_has_prefix(uri, "file://"))
            path=&uri[strlen("file://")];
        else if(*uri=='/')
            path=uri;
        if(path)
        {
            const char *home = g_get_home_dir();
            if(strcmp(home, path))
                retval=g_strdup("Home");
            else if(g_str_has_prefix(path, home) && strcmp(&path[strlen(home)], "/Desktop"))
                retval=g_strdup("Desktop");
            else if(g_str_has_prefix(path, home) && strcmp(&path[strlen(home)], "/.gnome-desktop"))
                retval=g_strdup("GNOME Desktop");
            else if(g_str_has_prefix(path, home))
                retval=g_strdup(&path[strlen(home)+1]);
            else
            {
                retval=path;
                uri=NULL;
            }
        }
        if(!retval)
        {
            retval=uri;
            uri=NULL;
        }
    }
    if(uri)
        g_free(uri);
    return retval;
}
