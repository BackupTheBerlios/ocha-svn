#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "catalog.h"
#include "indexer.h"
#include "indexers.h"
#include "catalog_queryrunner.h"
#include <libgnome/gnome-init.h>
#include <libgnome/gnome-program.h>
#include <libgnome/libgnome.h>

/** \file run the indexer
 */

static void usage(FILE *out);
static gboolean first_time(gboolean verbose, struct catalog *catalog);

static void usage(FILE *out)
{
   fprintf(out,
           "USAGE: indexer [--quiet]\n");
}


static char *get_catalog_path(void)
{
   GString *catalog_path = g_string_new(getenv("HOME"));
   g_string_append(catalog_path, "/.ocha");
   mkdir(catalog_path->str, 0700);
   g_string_append(catalog_path, "/catalog");
   char *retval = catalog_path->str;
   g_string_free(catalog_path, FALSE);
   return retval;
}

int main(int argc, char *argv[])
{
   gboolean slow=FALSE;
   gboolean verbose=TRUE;
   int maxdepth=-1;


   gnome_program_init ("ocha",
                       "0.1",
                       LIBGNOME_MODULE,
                       argc,
                       argv,
                       GNOME_PARAM_CREATE_DIRECTORIES, TRUE,
                       GNOME_PARAM_NONE);


   int curarg;
   for(curarg=1; curarg<argc; curarg++)
      {
         const char *arg=argv[curarg];

         if(strcmp("-h", arg)==0 || strcmp("-help", arg)==0 || strcmp("--help", arg)==0)
            {
               usage(stdout);
               exit(0);
            }
         else if(strcmp("--quiet", arg)==0)
            verbose=FALSE;
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

   if(curarg!=argc)
      {
         fprintf(stderr,
                 "error: too many arguments\n");
         usage(stderr);
         exit(111);
      }
   char *catalog_path = get_catalog_path();

   int retval=0;
   gboolean catalog_existed = g_file_test(catalog_path, G_FILE_TEST_EXISTS);
   GError *err = NULL;
   struct catalog *catalog = catalog_connect(catalog_path, &err);
   if(catalog==NULL)
      {
         fprintf(stderr, "error: could not open or create create catalog at '%s': %s\n",
                 catalog_path,
                 err->message);
         exit(114);
      }

   if(!catalog_existed)
      {
         if(!first_time(verbose, catalog))
            {
               catalog_disconnect(catalog);
               unlink(catalog_path);
               exit(123);
            }
      }

   for(struct indexer **indexer_ptr = indexers_list();
       *indexer_ptr;
       indexer_ptr++)
      {
         struct indexer *indexer = *indexer_ptr;
         if(verbose)
            printf("indexing %s...\n",
                   indexer->name);
         int *source_ids = NULL;
         int source_ids_len = 0;
         if(catalog_list_sources(catalog, indexer->name, &source_ids, &source_ids_len))
            {
               for(int i=0; i<source_ids_len; i++)
                  {
                     int source_id = source_ids[i];
                     struct indexer_source *source = indexer->load_source(indexer,
                                                                                  catalog,
                                                                                  source_id);
                     if(source)
                        {
                           if(verbose)
                              printf("indexing source %s-%d...\n",
                                     indexer->name,
                                     source_id);

                           GError *err = NULL;
                           if(!source->index(source, catalog, &err))
                              {
                                 fprintf(stderr,
                                         "error indexing list for %s-%d: %s\n",
                                         indexer->name,
                                         source_id,
                                         err->message);
                                 g_error_free(err);
                                 retval++;
                              }
                        }
                  }
               if(source_ids)
                  g_free(source_ids);
            }
         else
            {
               fprintf(stderr,
                       "error getting source list for %s: %s",
                       indexer->name,
                       catalog_error(catalog));
               retval++;
            }
      }

   return retval;
}

static gboolean first_time(gboolean verbose, struct catalog *catalog)
{
   if(verbose)
      {
         printf("First time startup. I'll need to create the catalog and index your files.\n");
         printf("This could take a while, please be patient.\n");

         printf("Configuring catalog...\n");
      }

   for(struct indexer **indexer_ptr = indexers_list();
       *indexer_ptr;
       indexer_ptr++)
      {
         struct indexer *indexer = *indexer_ptr;
         if(verbose)
            printf("First-time configuration of module %s...\n",
                   indexer->name);
         if(!indexer->discover(indexer, catalog))
            {
               fprintf(stderr,
                       "error: first-time configuration of module '%s' failed : %s\n",
                       indexer->name,
                       catalog_error(catalog));
               return FALSE;
            }
      }

   if(verbose)
      printf("First time configuration done. Now indexing.\n");
   return TRUE;
}
