#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "catalog_index.h"

/** \file run the indexer
 */

static void usage(FILE *out)
{
   fprintf(out,
           "USAGE: indexer [--quiet] [--slow] [--maxdepth=integer] directory|applications|bookmarks path\n");
}

static void print_indexing_error(const char *type, const char *path, GError **err)
{
   const char *message = (*err)==NULL?"unknown error":(*err)->message;
   fprintf(stderr,
           "error indexing %s in %s: %s\n",
           type,
           path,
           message);
   if(*err)
      g_error_free(*err);
   *err=NULL;
}

static void trace_cb(const char *path, const char *name, gpointer userdata)
{
   gint *entry_count = (gint *)userdata;
   g_return_if_fail(entry_count);
   printf("added entry: %s \"%s\"\n", path, name);
   (*entry_count)++;
}

int main(int argc, char *argv[])
{
   gboolean slow=FALSE;
   gboolean verbose=TRUE;
   int maxdepth=-1;

   catalog_index_init();

   int curarg;
   for(curarg=1; curarg<argc; curarg++)
      {
         const char *arg=argv[curarg];

         if(strcmp("-h", arg)==0 || strcmp("-help", arg)==0 || strcmp("--help", arg)==0)
            {
               usage(stdout);
               exit(0);
            }
         else if(strcmp("--slow", arg)==0)
            slow=TRUE;
         else if(strcmp("--quiet", arg)==0)
            verbose=FALSE;
         else if(strncmp("--maxdepth=", arg, strlen("--maxdepth="))==0)
            {
               maxdepth=atoi(arg+strlen("--maxdepth="));
               if(maxdepth<=0)
                  {
                     fprintf(stderr,
                             "error: invalid value for --maxdepth: %s. maxdepth should be a positive integer\n",
                             arg);
                     exit(109);
                  }
            }
         else if(*arg=='-')
            {
               fprintf(stderr,
                       "error: unknown option: %s\n",
                       arg);
               usage(stderr);
               exit(110);
            }
         else
            break;
      }

   if((curarg+2)>argc)
      {
         fprintf(stderr,
                 "error: expected at least two arguments\n");
         usage(stderr);
         exit(111);
      }

   char *type=argv[curarg];
   curarg++;

   GString *catalog_path = g_string_new(getenv("HOME"));
   g_string_append(catalog_path, "/.ocha");
   mkdir(catalog_path->str, 0600);
   g_string_append(catalog_path, "/catalog");


   int retval=0;
   struct catalog *catalog = catalog_connect(catalog_path->str, NULL/*errors*/);
   if(catalog==NULL)
      {
         fprintf(stderr, "error: could not open catalog at '%s'\n",
                 catalog_path->str);
         exit(114);
      }

   int entry_count=0;
   if(verbose)
      catalog_index_set_trace_callback(trace_cb, &entry_count);

   GError *err=NULL;
   if(strcmp("applications", type)==0)
      {
         for(int i=curarg; i<argc; i++)
            {
               if(verbose)
                  printf("indexing applications in %s...\n", argv[i]);
               if(!catalog_index_applications(catalog, argv[i], maxdepth, slow, &err))
                  {
                     print_indexing_error(type, argv[i], &err);
                     retval++;
                  }
            }
      }
   else if(strcmp("directory", type)==0)
      {
         for(int i=curarg; i<argc; i++)
            {
               if(verbose)
                  printf("indexing files in %s...\n", argv[i]);
               if(!catalog_index_directory(catalog, argv[i], maxdepth, NULL/*ignore*/, slow, &err))
                  {
                     print_indexing_error(type, argv[i], &err);
                     retval++;
                  }
            }
      }
   else if(strcmp("bookmarks", type)==0)
      {
         for(int i=curarg; i<argc; i++)
            {
               if(verbose)
                  printf("indexing bookmarks in %s...\n", argv[i]);
               if(!catalog_index_bookmarks(catalog, argv[i], &err))
                  {
                     print_indexing_error(type, argv[i], &err);
                     retval++;
                  }
            }
      }
   else
      {
         fprintf(stderr,
                 "error: invalid type '%s'. Type should be 'applications', 'directory' or 'bookmarks'.\n",
                 type);
         usage(stderr);
         retval=12;
      }

   catalog_disconnect(catalog);

   if(verbose)
      printf("%d entries were added or refreshed by this call.\n",
             entry_count);

   return retval;
}
