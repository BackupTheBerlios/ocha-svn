#include "catalog_index.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

/** \file implementation of the API defined in catalog_index.h */

static bool getmode(const char *path, mode_t *mode);
static bool is_accessible_directory(mode_t mode);
static bool is_accessible_file(mode_t mode);
static bool is_readable(mode_t mode);
static bool is_executable(mode_t mode);

static bool catalog_index_directory_recursive(struct catalog *catalog, const char *directory, int maxdepth, bool slow, int cmd);

/* ------------------------- public functions */

bool catalog_index_directory(struct catalog *catalog, const char *directory, int maxdepth, bool slow)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(directory!=NULL, false);
   g_return_val_if_fail(maxdepth==-1 || maxdepth>0, false);

   int cmd = -1;
   catalog_addcommand(catalog, "gnome-open", "gnome-open '%f'", &cmd);

   return catalog_index_directory_recursive(catalog, directory, maxdepth, slow, cmd);
}

bool catalog_index_applications(struct catalog *catalog, const char *directory, int maxdepth, bool slow)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(directory!=NULL, false);
   return false;
}

bool catalog_index_bookmarks(struct catalog *catalog, const char *bookmark_file)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(bookmark_file!=NULL, false);
   return false;
}

/* ------------------------- private functions */

static bool catalog_index_directory_recursive(struct catalog *catalog, const char *directory, int maxdepth, bool slow, int cmd)
{
   if(maxdepth==0)
      return true;
   if(maxdepth>0)
      maxdepth--;

   bool retval=true;
   DIR *dir = opendir(directory);
   if(dir==NULL)
      return false;

   struct dirent *dirent;
   while( (dirent=readdir(dir)) != NULL )
      {
         const char *filename = dirent->d_name;
         if(*filename=='.')
            continue;

         char buffer[strlen(directory)+1+strlen(filename)+1];
         strcpy(buffer, directory);
         strcat(buffer, "/");
         strcat(buffer, filename);

         mode_t mode;
         if(getmode(buffer, &mode))
            {
               if(is_accessible_directory(mode))
                  {
                     if(maxdepth!=0)
                        {
                           if(!catalog_index_directory_recursive(catalog, buffer, maxdepth, slow, cmd))
                              retval=false;
                        }
                  }
               else if(is_accessible_file(mode))
                  catalog_addentry(catalog,
                                   directory,
                                   filename,
                                   filename,
                                   cmd,
                                   NULL/*id_out*/);
            }
      }

   closedir(dir);

   return retval;
}


static bool getmode(const char *path, mode_t* mode)
{
   struct stat buf;
   if(stat(path, &buf)==0)
      {
         *mode=buf.st_mode;
         return true;
      }
   return false;
}

static bool is_accessible_directory(mode_t mode)
{
   if(!S_ISDIR(mode))
      return false;
   return is_readable(mode) && is_executable(mode);
}

static bool is_accessible_file(mode_t mode)
{
   if(!S_ISREG(mode))
      return false;
   return is_readable(mode);
}

static bool is_readable(mode_t mode)
{
   return true;
}

static bool is_executable(mode_t mode)
{
   return true;
}
