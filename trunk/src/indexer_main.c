#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "catalog.h"
#include "indexer.h"
#include "indexers.h"
#include "ocha_init.h"
#include "ocha_gconf.h"
#include "first_time.h"
#include "catalog_queryrunner.h"
#include <libgnome/gnome-init.h>
#include <libgnome/gnome-program.h>
#include <libgnome/libgnome.h>

/** \file run the indexer
 */

/* ------------------------- prototypes */
static void usage(FILE *out);
static int index_everything(struct catalog  *catalog, gboolean verbose);

/* ------------------------- main */
static void usage(FILE *out)
{
        fprintf(out,
                "USAGE: indexer [--quiet]\n");
}

int main(int argc, char *argv[])
{
        int curarg;
        gboolean verbose=TRUE;
        struct configuration config;
        const char *catalog_path;
        int retval = 0;
        GError *err = NULL;
        struct catalog *catalog;
        gboolean first_time=FALSE;

        for(curarg=1; curarg<argc; curarg++) {
                const char *arg=argv[curarg];

                if(strcmp("-h", arg)==0
                   || strcmp("-help", arg)==0
                   || strcmp("--help", arg)==0) {
                        usage(stdout);
                        exit(0);
                } else if(strcmp("--quiet", arg)==0) {
                        verbose=FALSE;
                } else if(*arg=='-') {
                        fprintf(stderr,
                                "error: unknown option: %s\n",
                                arg);
                        usage(stderr);
                        exit(110);
                } else {
                        break;
                }
        }

        ocha_init(PACKAGE "_indexer", argc, argv, TRUE/*gui*/, &config);

        catalog_path =  config.catalog_path;
        catalog =  catalog_connect(catalog_path, &err);

        if(curarg!=argc) {
                fprintf(stderr,
                        "error: too many arguments\n");
                usage(stderr);
                exit(111);
        }

        first_time=!ocha_gconf_exists();
        if(catalog==NULL) {
                fprintf(stderr, "error: could not open or create create catalog at '%s': %s\n",
                        catalog_path,
                        err->message);
                exit(114);
        }


        if(first_time) {
                if(!first_time_run(catalog)) {
                        unlink(catalog_path);
                        retval=128;
                }
        } else {
                retval = index_everything(catalog, verbose);
        }

        catalog_timestamp_update(catalog);
        catalog_disconnect(catalog);
        return retval;
}

/* ------------------------- static functions */
static int index_everything(struct catalog  *catalog, gboolean verbose)
{
        int retval=0;
        struct indexer  **indexer_ptr;
        for(indexer_ptr = indexers_list();
            *indexer_ptr;
            indexer_ptr++) {
                struct indexer *indexer = *indexer_ptr;
                int *source_ids = NULL;
                int source_ids_len = 0;
                int i;
                ocha_gconf_get_sources(indexer->name, &source_ids, &source_ids_len);
                for(i=0; i<source_ids_len; i++) {
                        int source_id = source_ids[i];
                        struct indexer_source *source;

                        source = indexer_load_source(indexer,
                                                     catalog,
                                                     source_id);
                        if(source) {
                                GError *err = NULL;
                                if(verbose) {
                                        printf("indexing %s: %s...\n",
                                               indexer->display_name,
                                               source->display_name);
                                }
                                if(!catalog_check_source(catalog, indexer->name, source_id)) {
                                        fprintf(stderr,
                                                "error: failed to re-create source %s (%d): %s\n",
                                                source->display_name,
                                                source_id,
                                                catalog_error(catalog));
                                        retval++;
                                } else {
                                        if(!indexer_source_index(source, catalog, &err)) {
                                                fprintf(stderr,
                                                        "error: error indexing list for %s (ID %d): %s\n",
                                                        source->display_name,
                                                        source_id,
                                                        err->message);
                                                g_error_free(err);
                                                retval++;
                                        }

                                        if(verbose) {
                                                unsigned int size=0;
                                                if(catalog_get_source_content_count(catalog,
                                                                                    source_id,
                                                                                    &size)) {
                                                        printf("indexing %s: %s: %d entries\n",
                                                               indexer->display_name,
                                                               source->display_name,
                                                               size);
                                                }
                                        }
                                }
                                indexer_source_release(source);
                        }
                }
                if(source_ids)
                        g_free(source_ids);
        }
        return(retval);
}
