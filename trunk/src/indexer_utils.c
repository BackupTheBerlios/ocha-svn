#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <libgnome/gnome-url.h>
#include <libgnomevfs/gnome-vfs.h>

#include "ocha_gconf.h"
#include "indexer_utils.h"
#include "indexers.h"
#include "launchers.h"

/** \file implementation of the API defined in indexer_utils.h */

/** default patterns, as strings */
static const char *DEFAULT_IGNORE_STRINGS = "CVS,*~,*.bak,#*#";

struct attribute_change_notify_userdata
{
        struct indexer *indexer;
        indexer_source_notify_f callback;
        gpointer userdata;
        struct catalog *catalog;
        int source_id;
};

/**
 * pattern spec created the 1st time
 * catalog_index_directory() is called (and never freed)
 */
static GPatternSpec **DEFAULT_IGNORE;

/* ------------------------- prototypes */
static void catalog_index_init(void);
static DIR *opendir_witherrors(const char *path, GError **err);
static gboolean _recurse(struct catalog *catalog, const char *directory, DIR *dirhandle, GPatternSpec **ignore_patterns, int maxdepth, gboolean slow, int cmd, handle_file_f callback, gpointer userdata, GError **err);
static gboolean getmode(const char *path, mode_t* mode);
static gboolean is_accessible_directory(mode_t mode);
static gboolean is_accessible_file(mode_t mode);
static gboolean is_readable(mode_t mode);
static gboolean is_executable(mode_t mode);
static gboolean to_ignore(const char *filename, GPatternSpec **patterns);
static void doze_off(gboolean really);
static void attribute_change_notify_cb(GConfClient *client, guint id, GConfEntry *entry, gpointer _userdata);

/* ------------------------- public functions */
GQuark catalog_index_error_quark()
{
        static GQuark quark;
        static gboolean initialized;
        if(!initialized) {
                quark=g_quark_from_static_string(INDEXER_ERROR_DOMAIN_NAME);
                initialized=TRUE;
        }
        return quark;
}

gboolean catalog_get_source_attribute_witherrors(const char *indexer,
                int source_id,
                const char *attribute,
                char **value_out,
                gboolean required,
                GError **err)
{
        g_return_val_if_fail(indexer, FALSE);
        g_return_val_if_fail(attribute, FALSE);
        g_return_val_if_fail(value_out, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

        *value_out = ocha_gconf_get_source_attribute(indexer,
                        source_id,
                        attribute);
        if(required && !(*value_out)) {
                g_set_error(err,
                            INDEXER_ERROR,
                            INDEXER_INVALID_CONFIGURATION,
                            "missing required attribute %s for source %d",
                            attribute,
                            source_id);
                return FALSE;
        }
        return TRUE;
}

gboolean catalog_addentry_witherrors(struct catalog *catalog,
                                     const char *path,
                                     const char *name,
                                     const char *long_name,
                                     int source_id,
                                     struct launcher *launcher,
                                     GError **err)
{
        g_return_val_if_fail(launcher, FALSE);
        if(!catalog_add_entry(catalog, source_id, launcher->id, path, name, long_name, NULL/*id_out*/))
        {
                g_set_error(err,
                            INDEXER_ERROR,
                            INDEXER_CATALOG_ERROR,
                            "could not add/refresh entry %s in catalog: %s",
                            path,
                            catalog_error(catalog));
                return FALSE;
        }
        return TRUE;
}

gboolean uri_exists(const char *text_uri)
{
        struct stat buf;

        g_return_val_if_fail(text_uri!=NULL, FALSE);
        if(!g_str_has_prefix(text_uri, "file://"))
                return TRUE;
        return stat(&text_uri[strlen("file://")], &buf)==0 && buf.st_size>0;
}

gboolean recurse(struct catalog *catalog,
                 const char *directory,
                 GPatternSpec **ignore_patterns,
                 int maxdepth,
                 gboolean slow,
                 int cmd,
                 handle_file_f callback,
                 gpointer userdata,
                 GError **err)
{
        DIR *dir;

        catalog_index_init();

        dir = opendir_witherrors(directory, err);
        if(dir) {
                return _recurse(catalog,
                                directory,
                                dir,
                                ignore_patterns,
                                maxdepth,
                                slow,
                                cmd,
                                callback,
                                userdata,
                                err);
        } else {
                return FALSE;
        }
}
gboolean index_recursively(const char *indexer,
                           struct catalog *catalog,
                           int source_id,
                           handle_file_f callback,
                           gpointer userdata,
                           GError **err)
{
        char *path = NULL;
        gboolean retval = FALSE;


        if(catalog_get_source_attribute_witherrors(indexer,
                        source_id,
                        "path",
                        &path,
                        TRUE/*required*/,
                        err))
        {
                char *depth_str = NULL;
                if(catalog_get_source_attribute_witherrors(indexer,
                                source_id,
                                "depth",
                                &depth_str,
                                FALSE/*not required*/,
                                err)) {
                        char *ignore = NULL;
                        int depth = -1;
                        if(depth_str) {
                                depth=atoi(depth_str);
                                g_free(depth_str);
                        }
                        if(catalog_get_source_attribute_witherrors(indexer,
                                        source_id,
                                        "ignore",
                                        &ignore,
                                        FALSE/*not required*/,
                                        err)) {
                                GPatternSpec **ignore_patterns = create_patterns(ignore);
                                retval=recurse(catalog,
                                               path,
                                               ignore_patterns,
                                               depth,
                                               FALSE/*not slow*/,
                                               source_id,
                                               callback,
                                               userdata,
                                               err);
                                free_patterns(ignore_patterns);
                        }
                }
                g_free(path);
        }
        return retval;
}

GPatternSpec **create_patterns(const char *patterns)
{
        GPtrArray *array;
        const char *ptr;
        GPatternSpec **retval;

        if(patterns==NULL || *patterns=='\0')
                return NULL;
        array =  g_ptr_array_new();
        ptr =  patterns;
        do {
                const char *cur=ptr;
                const char *end = strchr(ptr, ',');
                int len;
                char *buffer;

                if(end) {
                        len=end-cur;
                        ptr=end+1;
                } else {
                        len=strlen(cur);
                        ptr=NULL;
                }

                buffer = g_malloc(len+1);
                strncpy(buffer, cur, len);
                buffer[len]='\0';
                g_ptr_array_add(array,
                                g_pattern_spec_new(g_strstrip(buffer)));
                g_free(buffer);
                ptr = end ? end+1:NULL;
        } while(ptr);
        g_ptr_array_add(array, NULL);

        retval =  (GPatternSpec**)array->pdata;
        g_ptr_array_free(array, FALSE/*don't free data*/);
        return retval;
}
void free_patterns(GPatternSpec **patterns)
{
        int i;
        if(patterns==NULL) {
                return;
        }
        for(i=0; patterns[i]!=NULL; i++) {
                g_pattern_spec_free(patterns[i]);
        }
        g_free(patterns);
}

void attribute_change_notify_cb(GConfClient *client, guint id, GConfEntry *entry, gpointer _userdata)
{
        struct attribute_change_notify_userdata *userdata;
        struct indexer_source *source;

        g_return_if_fail(_userdata);

        userdata = (struct attribute_change_notify_userdata *)_userdata;
        source =  indexer_load_source(userdata->indexer,
                                      userdata->catalog,
                                      userdata->source_id);
        if(source) {
                userdata->callback(source, userdata->userdata);
                indexer_source_release(source);
        }
}

guint source_attribute_change_notify_add(struct indexer *indexer,
                int source_id,
                const char *attribute,
                struct catalog *catalog,
                indexer_source_notify_f callback,
                gpointer callback_userdata)
{
        gboolean retval;
        struct attribute_change_notify_userdata *userdata;
        char *key;

        key =  ocha_gconf_get_source_attribute_key(indexer->name, source_id, attribute);

        userdata=g_new(struct attribute_change_notify_userdata, 1);
        userdata->callback=callback;
        userdata->userdata=callback_userdata;
        userdata->indexer=indexer;
        userdata->source_id=source_id;
        userdata->catalog=catalog;

        retval = TRUE;
        if(!gconf_client_notify_add(ocha_gconf_get_client(),
                                    key,
                                    attribute_change_notify_cb,
                                    userdata,
                                    g_free/*free userdata*/,
                                    NULL/*err*/))
        {
                g_free(userdata);
                retval=FALSE;
        }
        g_free(key);
        return retval;
}


void source_attribute_change_notify_remove(guint id)
{
        gconf_client_notify_remove(ocha_gconf_get_client(),
                                   id);
}

/* ------------------------- static functions */

static void catalog_index_init()
{
        if(!DEFAULT_IGNORE)
                DEFAULT_IGNORE=create_patterns(DEFAULT_IGNORE_STRINGS);
}

static DIR *opendir_witherrors(const char *path, GError **err)
{
        DIR *retval = opendir(path);
        if(retval==NULL) {
                g_set_error(err,
                            INDEXER_ERROR,
                            INDEXER_INVALID_INPUT,
                            "could not open directory '%s': %s",
                            path,
                            strerror(errno));
        }
        return retval;
}

/**
 * Recurse through directories.
 *
 * @param catalog
 * @param directory full path to the directory represented by dirhandle
 * @param dirhandle handle on a directory (which will be closed by this function)
 * @param ignore_patterns additional patterns to ignore (or NULL)
 * @param maxdepth maximum depth to go through 0=> do not look into sub directories, -1=> infinite
 * @param slow if true, slow down search
 * @param cmd source ID
 * @param callback
 * @param userdata
 * @parma err
 * @return true if it worked
 */
static gboolean _recurse(struct catalog *catalog,
                         const char *directory,
                         DIR *dirhandle,
                         GPatternSpec **ignore_patterns,
                         int maxdepth,
                         gboolean slow,
                         int cmd,
                         handle_file_f callback,
                         gpointer userdata,
                         GError **err)
{
        GPtrArray *subdirectories;
        gboolean error;
        struct dirent *dirent;

        if(maxdepth==0) {
                return TRUE;
        }
        if(maxdepth>0) {
                maxdepth--;
        }

        subdirectories =  NULL;
        if(maxdepth!=0)
                subdirectories = g_ptr_array_new();
        error = FALSE;
        while( !error && (dirent=readdir(dirhandle)) != NULL )
        {
                const char *filename;
                char *current_path;
                mode_t mode;

                filename =  dirent->d_name;
                if(*filename=='.' || to_ignore(filename, DEFAULT_IGNORE) || to_ignore(filename, ignore_patterns))
                        continue;

                current_path=g_malloc(strlen(directory)+1+strlen(filename)+1);
                strcpy(current_path, directory);
                if(current_path[strlen(current_path)-1]!='/')
                        strcat(current_path, "/");
                strcat(current_path, filename);

                if(getmode(current_path, &mode)) {
                        gboolean acc_dir=is_accessible_directory(mode);
                        gboolean acc_file=is_accessible_file(mode);

                        if(acc_dir && maxdepth!=0) {
                                char *current_path_dup = g_strdup(current_path);
                                g_ptr_array_add(subdirectories, current_path_dup);
                        }
                        if(acc_dir || acc_file) {
                                if(!callback(catalog, cmd, current_path, filename, err, userdata))
                                        error=TRUE;
                        }
                }
                g_free(current_path);
        }
        closedir(dirhandle);

        if(subdirectories)
        {
                int i;
                for(i=0; i<subdirectories->len; i++) {
                        char *current_path = subdirectories->pdata[i];
                        DIR *subdir = opendir(current_path);
                        if(subdir!=NULL) {
                                if(!_recurse(catalog,
                                                current_path,
                                                subdir,
                                                ignore_patterns,
                                                maxdepth,
                                                slow,
                                                cmd,
                                                callback,
                                                userdata,
                                                err)) {
                                        error=TRUE;
                                } else {
                                        doze_off(slow);
                                }
                        }
                        g_free(current_path);
                }
                g_ptr_array_free(subdirectories, TRUE/*free segments*/);
        }

        return !error;
}

static gboolean getmode(const char *path, mode_t* mode)
{
        struct stat buf;
        if(stat(path, &buf)==0) {
                *mode=buf.st_mode;
                return TRUE;
        }
        return FALSE;
}

static gboolean is_accessible_directory(mode_t mode)
{
        if(!S_ISDIR(mode))
                return FALSE;
        return is_readable(mode) && is_executable(mode);
}

static gboolean is_accessible_file(mode_t mode)
{
        if(!S_ISREG(mode))
                return FALSE;
        return is_readable(mode);
}

static gboolean is_readable(mode_t mode)
{
        return TRUE;
}

static gboolean is_executable(mode_t mode)
{
        return TRUE;
}

static gboolean to_ignore(const char *filename, GPatternSpec **patterns)
{
        int i;
        if(patterns==NULL) {
                return FALSE;
        }
        for(i=0; patterns[i]!=NULL; i++) {
                if(g_pattern_match_string(patterns[i], filename))
                        return TRUE;
        }
        return FALSE;
}

static void doze_off(gboolean really)
{
        if(really)
                sleep(3);
}

