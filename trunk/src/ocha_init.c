#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ocha_init.h"
#include <libgnome/gnome-init.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnome/gnome-program.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>


/** \file Generic initialization */
#ifndef PACKAGE
#define PACKAGE "ocha"
#endif
#ifndef VERSION
#define VERSION "0.0"
#endif

/* ------------------------- prototypes */
static char *get_catalog_path(void);

/* ------------------------- public functions */

void ocha_init(int argc, char **argv, gboolean ui, struct configuration *config)
{
        g_thread_init(NULL/*vtable*/);
        gnome_program_init (PACKAGE,
                            VERSION,
                            ui ? LIBGNOMEUI_MODULE : LIBGNOME_MODULE,
                            argc,
                            argv,
                            NULL);
        config->catalog_path=get_catalog_path();
}
void ocha_init_requires_catalog(const char *catalog_path)
{
        if(!g_file_test(catalog_path, G_FILE_TEST_EXISTS)) {
                fprintf(stderr, "No catalog: please start the indexer first\n");
                exit(10);
        }
}

/* ------------------------- static functions */
static char *get_catalog_path(void)
{
        GString *catalog_path = g_string_new("");
        const char *homedir = g_get_home_dir();
        g_string_append(catalog_path, homedir);

        g_string_append(catalog_path, "/.ocha");
        mkdir(catalog_path->str, 0700);
        g_string_append(catalog_path, "/catalog");
        char *retval = catalog_path->str;
        g_string_free(catalog_path, FALSE);
        return retval;
}

