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

/** \file implementation of the API defined in indexer_utils.h */

/** default patterns, as strings */
static const char *DEFAULT_IGNORE_STRINGS = "CVS,*~,*.bak,#*#";

/** pattern spec created the 1st time catalog_index_directory() is called (and never freed) */
static GPatternSpec **DEFAULT_IGNORE;

static void catalog_index_init();
static gboolean getmode(const char *path, mode_t *mode);
static gboolean is_accessible_directory(mode_t mode);
static gboolean is_accessible_file(mode_t mode);
static gboolean is_readable(mode_t mode);
static gboolean is_executable(mode_t mode);

static gboolean to_ignore(const char *filename, GPatternSpec **patterns);

static void doze_off(gboolean really);
static int null_terminated_array_length(const char **patterns);
static DIR *opendir_witherrors(const char *path, GError **err);


/* ------------------------- public functions */
GQuark catalog_index_error_quark()
{
   static GQuark quark;
   static gboolean initialized;
   if(!initialized)
      {
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
   if(required && !(*value_out))
      {
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
gboolean catalog_addentry_witherrors(struct catalog *catalog, const char *path, const char *name, const char *long_name, int source_id, GError **err)
{
   if(!catalog_addentry(catalog, path, name, long_name, source_id, NULL/*id_out*/))
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

DIR *opendir_witherrors(const char *path, GError **err)
{
   DIR *retval = opendir(path);
   if(retval==NULL)
      {
         g_set_error(err,
                     INDEXER_ERROR,
                     INDEXER_INVALID_INPUT,
                     "could not open directory '%s': %s",
                     path,
                     strerror(errno));
      }
   return retval;
}

gboolean uri_exists(const char *text_uri)
{
   g_return_val_if_fail(text_uri!=NULL, FALSE);
   if(!g_str_has_prefix(text_uri, "file://"))
      return TRUE;
   struct stat buf;
   return stat(&text_uri[strlen("file://")], &buf)==0 && buf.st_size>0;
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
   if(maxdepth==0)
      return TRUE;
   if(maxdepth>0)
      maxdepth--;

   GPtrArray *subdirectories = NULL;
   if(maxdepth!=0)
       subdirectories = g_ptr_array_new();
   gboolean error=FALSE;
   struct dirent *dirent;
   while( !error && (dirent=readdir(dirhandle)) != NULL )
      {
         const char *filename = dirent->d_name;
         if(*filename=='.' || to_ignore(filename, DEFAULT_IGNORE) || to_ignore(filename, ignore_patterns))
            continue;

         char current_path[strlen(directory)+1+strlen(filename)+1];
         strcpy(current_path, directory);
         if(current_path[strlen(current_path)-1]!='/')
            strcat(current_path, "/");
         strcat(current_path, filename);

         mode_t mode;
         if(getmode(current_path, &mode))
            {
                gboolean acc_dir=is_accessible_directory(mode);
                gboolean acc_file=is_accessible_file(mode);

                if(acc_dir && maxdepth!=0)
                  {
                      char *current_path_dup = g_strdup(current_path);
                      g_ptr_array_add(subdirectories, current_path_dup);
                  }
                if(acc_dir || acc_file)
                  {
                      if(!callback(catalog, cmd, current_path, filename, err, userdata))
                        error=TRUE;
                  }
            }
      }
   closedir(dirhandle);

   if(subdirectories)
   {
       for(int i=0; i<subdirectories->len; i++)
       {
           char *current_path = subdirectories->pdata[i];
           DIR *subdir = opendir(current_path);
           if(subdir!=NULL)
           {
               if(!_recurse(catalog,
                            current_path,
                            subdir,
                            ignore_patterns,
                            maxdepth,
                            slow,
                            cmd,
                            callback,
                            userdata,
                            err))
               {
                   error=TRUE;
               }
               else
               {
                   doze_off(slow);
               }
           }
           g_free(current_path);
       }
       g_ptr_array_free(subdirectories, TRUE/*free segments*/);
   }

   return !error;
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
   catalog_index_init();

   DIR *dir=opendir_witherrors(directory, err);
   if(dir)
      {
         gboolean retval = _recurse(catalog,
                                directory,
                                dir,
                                ignore_patterns,
                                maxdepth,
                                slow,
                                cmd,
                                callback,
                                userdata,
                                err);
         return retval;
      }
   else
      {
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
                                                    err))
            {
               int depth = -1;
               if(depth_str)
                  {
                     depth=atoi(depth_str);
                     g_free(depth_str);
                  }
               char *ignore = NULL;
               if(catalog_get_source_attribute_witherrors(indexer,
                                                          source_id,
                                                          "ignore",
                                                          &ignore,
                                                          FALSE/*not required*/,
                                                          err))
                  {
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
   if(patterns==NULL || *patterns=='\0')
      return NULL;
   GPtrArray *array = g_ptr_array_new();
   const char *ptr = patterns;
   do
      {
         const char *cur=ptr;
         const char *end = strchr(ptr, ',');
         int len;
         if(end)
            {
               len=end-cur;
               ptr=end+1;
            }
         else
            {
               len=strlen(cur);
               ptr=NULL;
            }
         char buffer[len+1];
         strncpy(buffer, cur, len);
         buffer[len]='\0';
         g_ptr_array_add(array,
                         g_pattern_spec_new(g_strstrip(buffer)));

         ptr = end ? end+1:NULL;
      }
   while(ptr);
   g_ptr_array_add(array, NULL);

   GPatternSpec **retval = (GPatternSpec**)array->pdata;
   g_ptr_array_free(array, FALSE/*don't free data*/);
   return retval;
}
void free_patterns(GPatternSpec **patterns)
{
   if(patterns==NULL)
      return;
   for(int i=0; patterns[i]!=NULL; i++)
      g_pattern_spec_free(patterns[i]);
   g_free(patterns);
}


/* ------------------------- private functions */
static void catalog_index_init()
{
   if(!DEFAULT_IGNORE)
      DEFAULT_IGNORE=create_patterns(DEFAULT_IGNORE_STRINGS);
}

static gboolean getmode(const char *path, mode_t* mode)
{
   struct stat buf;
   if(stat(path, &buf)==0)
      {
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
   if(patterns==NULL)
      return FALSE;
   for(int i=0; patterns[i]!=NULL; i++)
      {
         if(g_pattern_match_string(patterns[i], filename))
            return TRUE;
      }
   return FALSE;
}

static int null_terminated_array_length(const char **patterns)
{
   int count;
   for(count=0; *patterns!=NULL; count++, patterns++);
   return count;
}

static void doze_off(gboolean really)
{
   if(really)
      sleep(3);
}
