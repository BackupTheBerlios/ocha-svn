#include "indexer.h"
#include "indexer_utils.h"
#include "indexer_files_view.h"
#include "result.h"
#include "ocha_gconf.h"
#include "launcher_open.h"
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

#define INDEXER_NAME "files"

/* ------------------------- prototypes: indexer_files */
static struct indexer_source *indexer_files_load_source(struct indexer *self, struct catalog *catalog, int id);
static gboolean indexer_files_execute(struct indexer *self, const char *name, const char *long_name, const char *text_uri, GError **err);
static gboolean indexer_files_validate(struct indexer *self, const char *name, const char *long_name, const char *text_uri);
static gboolean indexer_files_discover(struct indexer *indexer, struct catalog *catalog);
static struct indexer_source *indexer_files_new_source(struct indexer *indexer, struct catalog *catalog, GError **err);

/* ------------------------- prototypes: indexer_files_source */
static void indexer_files_source_release(struct indexer_source *source);
static gboolean indexer_files_source_index(struct indexer_source *self, struct catalog *catalog, GError **err);
static guint indexer_files_source_notify_add(struct indexer_source *source, struct catalog *catalog, indexer_source_notify_f notify, gpointer userdata);
static void indexer_files_source_notify_remove(struct indexer_source *source, guint id);

/* ------------------------- prototypes: other */
static gboolean add_source(struct catalog *catalog, const char *path, int depth, char *ignore, int *id_ptr);
static gboolean index_file_cb(struct catalog *catalog, int source_id, const char *path, const char *filename, GError **err, gpointer userdata);
static gboolean has_gnome_mime_command(const char *path);
static char *display_name(struct catalog *catalog, int id);

/* ------------------------- definitions */

/** Definition of the indexer */
struct indexer indexer_files = {
        INDEXER_NAME,
        "Files and Directories",

        /* description */
        "This indexer looks recursively into directories for files "
        "that GNOME knows how to open.\n"
        "By default, only the desktop and your home directory are "
        "indexed. You'll probably want to add new sources "
        "for the folders where you save the files you "
        "most often work with.",

        indexer_files_discover,
        indexer_files_load_source,
        indexer_files_execute,
        indexer_files_validate,
        indexer_files_new_source,
        indexer_files_view_new
};

/* ------------------------- member functions: indexer_files */

/**
 * Load a source from the catalog.
 *
 * This function will look for the attribute 'path', which
 * must be set to the base directory and the attribute 'depth',
 * which might be set (defaults to -1, infinite depth)
 */
static struct indexer_source *indexer_files_load_source(struct indexer *self,
                                                        struct catalog *catalog,
                                                        int id)
{
        struct indexer_source *retval;

        g_return_val_if_fail(catalog!=NULL, NULL);
        g_return_val_if_fail(self==&indexer_files, NULL);

        retval =  g_new(struct indexer_source, 1);
        retval->id=id;
        retval->indexer=self;
        retval->index=indexer_files_source_index;
        retval->release=indexer_files_source_release;
        retval->display_name=display_name(catalog, id);
        retval->notify_display_name_change=indexer_files_source_notify_add;
        retval->remove_notification=indexer_files_source_notify_remove;
        return retval;
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
static gboolean indexer_files_execute(struct indexer *self,
                                      const char *name,
                                      const char *long_name,
                                      const char *text_uri,
                                      GError **err)
{
        GError *gnome_err;
        g_return_val_if_fail(text_uri!=NULL, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);
        g_return_val_if_fail(self==&indexer_files, FALSE);

        if(!indexer_files_validate(self, name, long_name, text_uri)) {
                g_set_error(err,
                            RESULT_ERROR,
                            RESULT_ERROR_INVALID_RESULT,
                            "file not found: %s",
                            text_uri);
                return FALSE;
        }

        printf("opening %s...\n", text_uri);
        gnome_err =  NULL;
        if(!gnome_url_show(text_uri, &gnome_err)) {
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
static gboolean indexer_files_validate(struct indexer *self,
                                       const char *name,
                                       const char *long_name,
                                       const char *text_uri)
{
        g_return_val_if_fail(text_uri!=NULL, FALSE);
        g_return_val_if_fail(self==&indexer_files, FALSE);

        return uri_exists(text_uri);
}


static gboolean indexer_files_discover(struct indexer *indexer,
                                       struct catalog *catalog)
{
        char *home_Desktop = gnome_util_prepend_user_home("Desktop");
        char *home_dot_gnome_desktop = gnome_util_prepend_user_home(".gnome-desktop");
        const char *home = g_get_home_dir();
        gboolean has_desktop = FALSE;
        gboolean retval = FALSE;

        if(g_file_test(home_Desktop, G_FILE_TEST_EXISTS)
                        && g_file_test(home_Desktop, G_FILE_TEST_IS_DIR))
        {
                if(!add_source(catalog, home_Desktop, 2/*depth*/, NULL/*ignore*/, NULL/*id_ptr*/))
                        goto error;
                has_desktop=TRUE;
        }

        if(g_file_test(home_dot_gnome_desktop, G_FILE_TEST_EXISTS)
                        && g_file_test(home_dot_gnome_desktop, G_FILE_TEST_IS_DIR))
        {
                if(!add_source(catalog, home_dot_gnome_desktop, 2/*depth*/, NULL/*ignore*/, NULL/*id_ptr*/))
                        goto error;
                has_desktop=TRUE;
        }

        if(!has_desktop)
        {
                /* yes, I'm paranoid... */
                if(g_file_test(home, G_FILE_TEST_EXISTS)
                                && g_file_test(home, G_FILE_TEST_IS_DIR)) {
                        if(!add_source(catalog, home, 2/*depth*/, "Desktop", NULL/*id_ptr*/))
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


static struct indexer_source *indexer_files_new_source(struct indexer *indexer,
                                                       struct catalog *catalog,
                                                       GError **err)
{
        int id=-1;

        g_return_val_if_fail(indexer, NULL);
        g_return_val_if_fail(err==NULL || (*err==NULL), NULL);

        if(add_source(catalog,
                      NULL/*no path*/,
                      0/*ignore content*/,
                      NULL/*default ignore*/,
                      &id)) {
                return indexer_files_load_source(indexer, catalog, id);
        }
        return NULL;
}



/* ------------------------- member functions: indexer_files_source */
static void indexer_files_source_release(struct indexer_source *source)
{
        g_return_if_fail(source!=NULL);
        g_free((gpointer)source->display_name);
        g_free(source);
}
/**
 * (re)index the directory of the source
 */
static gboolean indexer_files_source_index(struct indexer_source *self,
                                           struct catalog *catalog,
                                           GError **err)
{
        g_return_val_if_fail(self!=NULL, FALSE);
        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

        remove_invalid_entries(&indexer_files, self->id, catalog);
        return index_recursively(INDEXER_NAME,
                                 catalog,
                                 self->id,
                                 index_file_cb,
                                 self/*userdata*/,
                                 err);
}


static guint indexer_files_source_notify_add(struct indexer_source *source,
                                             struct catalog *catalog,
                                             indexer_source_notify_f notify,
                                             gpointer userdata)
{
        g_return_val_if_fail(source, 0);
        g_return_val_if_fail(notify, 0);
        return source_attribute_change_notify_add(&indexer_files,
                                                  source->id,
                                                  "path",
                                                  catalog,
                                                  notify,
                                                  userdata);
}
static void indexer_files_source_notify_remove(struct indexer_source *source,
                                               guint id)
{
        source_attribute_change_notify_remove(id);
}


/* ------------------------- static functions */

static gboolean add_source(struct catalog *catalog,
                           const char *path,
                           int depth,
                           char *ignore,
                           int *id_ptr)
{
        int id;
        if(!catalog_add_source(catalog, INDEXER_NAME, &id))
                return FALSE;
        if(id_ptr)
                *id_ptr=id;
        if(path) {
                if(!ocha_gconf_set_source_attribute(INDEXER_NAME, id, "path", path))
                        return FALSE;
        }
        if(depth!=-1) {
                char *depth_str = g_strdup_printf("%d", depth);
                gboolean ret = ocha_gconf_set_source_attribute(INDEXER_NAME, id, "depth", depth_str);
                g_free(depth_str);
                if(!ret)
                        return FALSE;
        }
        if(ignore) {
                if(!ocha_gconf_set_source_attribute(INDEXER_NAME, id, "ignore", ignore))
                        return FALSE;
        }
        return TRUE;
}

static gboolean index_file_cb(struct catalog *catalog,
                              int source_id,
                              const char *path,
                              const char *filename,
                              GError **err,
                              gpointer userdata)
{
        char *uri;
        gboolean retval;

        if(!has_gnome_mime_command(path)) {
                return TRUE;
        }

        uri = g_strdup_printf("file://%s", path);
        retval = catalog_addentry_witherrors(catalog,
                                             uri,
                                             filename,
                                             path/*long_name*/,
                                             source_id,
                                             &launcher_open,
                                             err);
        g_free(uri);
        return retval;
}

static gboolean has_gnome_mime_command(const char *path)
{
        gboolean retval;
        char *mimetype;
        GString *uri;

        uri =  g_string_new("file://");
        if(*path!='/') {
                const char *cwd = g_get_current_dir();
                g_string_append(uri, cwd);
                g_free((void *)cwd);
                if(uri->str[uri->len-1]!='/')
                        g_string_append_c(uri, '/');
        }
        g_string_append(uri, path);

        retval = FALSE;
        mimetype =  gnome_vfs_get_mime_type(uri->str);
        if(mimetype) {
                char *app = gnome_vfs_mime_get_default_desktop_entry(mimetype);
                if(app) {
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
        char *uri=ocha_gconf_get_source_attribute(INDEXER_NAME, id, "path");
        char *retval=NULL;
        if(uri==NULL)
                retval=g_strdup(indexer_files.display_name);
        else
        {
                char *path=NULL;
                if(g_str_has_prefix(uri, "file://")) {
                        path=&uri[strlen("file://")];
                } else if(*uri=='/') {
                        path=uri;
                }
                if(path) {
                        const char *home = g_get_home_dir();
                        if(strcmp(home, path)==0) {
                                retval=g_strdup("Home");
                        } else if(g_str_has_prefix(path, home)
                                  && strcmp(&path[strlen(home)], "/Desktop")==0) {
                                retval=g_strdup("Desktop");
                        } else if(g_str_has_prefix(path, home)
                                  && strcmp(&path[strlen(home)], "/.gnome-desktop")==0) {
                                retval=g_strdup("GNOME Desktop");
                        } else if(g_str_has_prefix(path, home)) {
                                retval=g_strdup(&path[strlen(home)+1]);
                        } else if(strcmp("/", path)==0) {
                                retval=g_strdup("Filesystem");
                        } else {
                                char *last_slash = strrchr(path, '/');
                                retval=g_strdup(last_slash+1);
                        }
                }
                if(!retval) {
                        retval=uri;
                        uri=NULL;
                }
        }
        if(uri) {
                g_free(uri);
        }
        return retval;
}

