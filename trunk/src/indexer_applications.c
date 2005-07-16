
#include "indexer_applications.h"
#include "launcher_application.h"
#include "indexer_utils.h"
#include "ocha_gconf.h"
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
#include "indexer_files_view.h"
#include "string_set.h"

/**
 * \file index applications with a .desktop file
 */

#define DESKTOP_SECTION "Desktop Entry"

#define INDEXER_NAME "applications"

/* ------------------------- prototypes: indexer_application */

static struct indexer_source *indexer_application_load_source(struct indexer *self, struct catalog *catalog, int id);
static gboolean indexer_application_discover(struct indexer *indexer, struct catalog *catalog);

/* ------------------------- prototypes: indexer_application_source */
static void indexer_application_source_release(struct indexer_source *source);
static gboolean index_application_cb(struct catalog *catalog, int source_id, const char *path, const char *filename, GError **err, gpointer userdata);
static gboolean indexer_application_source_index(struct indexer_source *self, struct catalog *catalog, GError **err);
static guint indexer_application_source_notify_add(struct indexer_source *source, struct catalog *catalog, indexer_source_notify_f notify, gpointer userdata);
static void indexer_application_source_notify_remove(struct indexer_source *source, guint id);

/* ------------------------- prototypes: other */
static GSList *maybe_add_applications_directory(GSList *path, const char *directory);
static GSList *maybe_add_applications_directories(GSList *path, const char * const *directories);

/* ------------------------- definitions */

struct indexer indexer_applications = {
        INDEXER_NAME,
        "Applications"/*display_name*/,

        /* description */
        "This indexer looks into standard locations for "
        "application definitions (.desktop files).\n"
        "The application of this indexer should correspond"
        "to what you have in your application menu.\n",

        indexer_application_discover,
        indexer_application_load_source,
        NULL/*new_source*/,
        NULL/*new_source_for_uri*/,
        indexer_files_view_new_pseudo_view,
};

/* ------------------------- public functions */

/* ------------------------- member functions (indexer_application) */
static struct indexer_source *indexer_application_load_source(struct indexer *self, struct catalog *catalog, int id)
{
        struct indexer_source *retval = g_new(struct indexer_source, 1);
        retval->id=id;
        retval->indexer=self;
        retval->index=indexer_application_source_index;
        retval->system=ocha_gconf_is_system(INDEXER_NAME, id);
        retval->release=indexer_application_source_release;
        retval->display_name="Applications";
        retval->notify_display_name_change=indexer_application_source_notify_add;
        retval->remove_notification=indexer_application_source_notify_remove;
        return retval;
}
static gboolean indexer_application_discover(struct indexer *indexer, struct catalog *catalog)
{
        int source_id;
        gboolean retval = FALSE;
        /** list of char * to free with g_free */
        GSList *paths = NULL;

        paths=maybe_add_applications_directory(paths, g_get_user_data_dir());
        paths=maybe_add_applications_directories(paths, g_get_system_data_dirs());

        if(catalog_add_source(catalog, INDEXER_NAME, &source_id)) {
                char *paths_key = ocha_gconf_get_source_attribute_key(INDEXER_NAME,
                                                                      source_id,
                                                                      "paths");

                if(gconf_client_set_list(ocha_gconf_get_client(),
                                         paths_key,
                                         GCONF_VALUE_STRING,
                                         paths,
                                         NULL/*let gconf client handle errors*/)) {
                        retval = TRUE;
                }

                g_free(paths_key);

                ocha_gconf_set_system(INDEXER_NAME, source_id, TRUE);
        }

        g_slist_foreach(paths, (GFunc)g_free, NULL/*no userdata*/);
        g_slist_free(paths);
        return retval;
}

/* ------------------------- member functions (indexer_application_source) */
static guint indexer_application_source_notify_add(struct indexer_source *source,
                                        struct catalog *catalog,
                                        indexer_source_notify_f notify,
                                        gpointer userdata)
{
        return 19;
}
static void indexer_application_source_notify_remove(struct indexer_source *source,
                                                     guint id)
{

}
static void indexer_application_source_release(struct indexer_source *source)
{
        g_return_if_fail(source);
        g_free(source);
}

static gboolean indexer_application_source_index(struct indexer_source *self, struct catalog *catalog, GError **err)
{
        gboolean success = TRUE;
        char *paths_key;
        GSList *paths;
        GError *gconf_err = NULL;
        GSList *item;
        struct string_set *visited;

        g_return_val_if_fail(self!=NULL, FALSE);
        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);



        paths_key = ocha_gconf_get_source_attribute_key(INDEXER_NAME,
                                                        self->id,
                                                        "paths");
        paths = gconf_client_get_list(ocha_gconf_get_client(),
                                      paths_key,
                                      GCONF_VALUE_STRING,
                                      &gconf_err);
        g_free(paths_key);
        if(paths==NULL) {
                if(gconf_err) {
                        g_set_error(err,
                                    INDEXER_ERROR,
                                    INDEXER_EXTERNAL_ERROR,
                                    "getting path list failed: %s",
                                    gconf_err->message);
                        g_error_free(gconf_err);
                } else {
                        g_set_error(err,
                                    INDEXER_ERROR,
                                    INDEXER_INVALID_CONFIGURATION,
                                    "path list (paths) not configured or empty");
                }

                return FALSE;
        }

        catalog_begin_source_update(catalog, self->id);


        visited=string_set_new();

        for(item=paths; item!=NULL; item=g_slist_next(item)) {
                const char *directory;
                directory = (const char *)item->data;

                if(directory==NULL) {
                        g_warning("found NULL directory in item of path list for source %d", self->id);
                        continue;
                }
                success=recurse(catalog,
                                directory,
                                NULL/*ignore_patterns*/,
                                10/*MAXDEPTH*/,
                                FALSE/*not slow*/,
                                self->id,
                                index_application_cb,
                                visited/*userdata*/,
                                err);
        }

        string_set_free(visited);

        catalog_end_source_update(catalog, self->id);
        return success;
}

/* ------------------------- static functions */

static gboolean index_application_cb(struct catalog *catalog,
                                     int source_id,
                                     const char *path,
                                     const char *filename,
                                     GError **err,
                                     gpointer userdata)
{
        char *uri;
        GnomeDesktopFile *desktopfile;
        char *type = NULL;
        char *name = NULL;
        char *comment = NULL;
        char *generic_name = NULL;
        gboolean nodisplay = FALSE;
        gboolean hidden = FALSE;
        char *exec = NULL;
        char *description=NULL;
        gboolean retval;
        struct string_set *visited;
        const char *visited_key;

        if(!g_str_has_suffix(filename, ".desktop"))
                return TRUE;

        g_return_val_if_fail(userdata!=NULL, FALSE);
        visited = (struct string_set *)userdata;

        visited_key = g_basename(path);

        if(string_set_contains(visited, visited_key)) {
                return TRUE;
        }

        uri = g_strdup_printf("file://%s", path);

        desktopfile =  gnome_desktop_file_load(path,
                                               NULL/*ignore errors*/);
        if(desktopfile==NULL)
                return TRUE;


        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "Type",
                                      &type);

        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "Name",
                                      &name);

        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "Comment",
                                      &comment);

        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "GenericName",
                                      &generic_name);

        gnome_desktop_file_get_boolean(desktopfile,
                                       DESKTOP_SECTION,
                                       "NoDisplay",
                                       &nodisplay);


        gnome_desktop_file_get_boolean(desktopfile,
                                       DESKTOP_SECTION,
                                       "Hidden",
                                       &hidden);

        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "Exec",
                                      &exec);

        retval = TRUE;
        if((type==NULL || g_strcasecmp("Application", type)==0)) {
                /* set it now that I know the file is valid and that
                 * it is an application
                 */
                string_set_add(visited, visited_key);

                if(comment && generic_name)
                        description=g_strdup_printf("%s (%s)", comment, generic_name);


                if(!hidden && !nodisplay && exec!=NULL) {
                        const char *long_name=description;
                        struct catalog_entry entry;

                        if(!long_name)
                                long_name=comment;
                        if(!long_name)
                                long_name=generic_name;
                        if(!long_name)
                                long_name=path;

                        entry.name=(char *) ( name==NULL ? filename:name );
                        entry.path=uri;
                        entry.long_name=long_name;
                        entry.source_id=source_id;
                        entry.launcher=launcher_application.id;
                        retval=catalog_addentry_witherrors(catalog,
                                                           &entry,
                                                           err);
                }
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
        g_free(uri);

        gnome_desktop_file_free(desktopfile);

        return retval;
}

/**
 * If directory + '/applications' exists, add it into
 * the list.
 *
 * The string added into the list will need to be
 * g_free'd at some point...
 *
 * @param path the current list
 * @param directory the directory to check
 * @return the new current list
 */
static GSList *maybe_add_applications_directory(GSList *path, const char *directory)
{
        char *full_directory = g_strdup_printf("%s/applications", directory);

        if(g_file_test(full_directory, G_FILE_TEST_EXISTS)
                        && g_file_test(full_directory, G_FILE_TEST_IS_DIR))
        {
                return g_slist_append(path, (gpointer)full_directory);
        }
        g_free(full_directory);
        return path;
}

static GSList *maybe_add_applications_directories(GSList *path, const char * const *directories)
{
        const char * const *current;

        for(current=directories; *current!=NULL; current++) {
                path=maybe_add_applications_directory(path, *current);
        }

        return path;
}

