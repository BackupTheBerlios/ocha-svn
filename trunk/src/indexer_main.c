#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "catalog.h"
#include "indexer.h"
#include "catalog_queryrunner.h"
#include <libgnome/gnome-init.h>

/** \file run the indexer
 */

static void usage(FILE *out)
{
   fprintf(out,
           "USAGE: indexer [--quiet]\n");
}

int main(int argc, char *argv[])
{
   gboolean slow=FALSE;
   gboolean verbose=TRUE;
   int maxdepth=-1;


   gnome_program_init ("ocha_indexer",
                       "0.1",
                       LIBGNOME_MODULE,
                       argc,
                       argv,
                       NULL);


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

   for(struct indexer **indexer_ptr = catalog_queryrunner_get_indexers();
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
                     struct indexer_source *source = indexer->load_indexer_source(indexer,
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
