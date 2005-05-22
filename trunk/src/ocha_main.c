#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/** \file Main ocha program (except for the daemon).
 *
 * This program has three modes: 'preferences' (the default),
 * 'index' (re-index without a GUI) and 'install' (first-time
 * install, with a GUI)
 */

#include "mode_preferences.h"
#include "mode_index.h"
#include "mode_install.h"
#include "mode_stop.h"
#include "ocha_init.h"
#include "libgnome/libgnome.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------- main */
int main(int argc, char *argv[])
{
        char *mode;

        mode = argv[argc-1];
        if(argc==1 || mode[0] == '-' || mode[0] == '\0') {
                mode=NULL;
        } else {
                argc--;
        }

        if(mode==NULL || strcmp(mode, "preferences")==0) {
                return mode_preferences(argc, argv);
        } else if(strcmp(mode, "index")==0) {
                return mode_index(argc, argv);
        } else if(strcmp(mode, "install")==0) {
                return mode_install(argc, argv);
        } else if(strcmp(mode, "stop")==0) {
                return mode_stop(argc, argv);
        } else {
                fprintf(stderr,
                        "error: unknown mode: '%s' valid modes are: 'index', 'install', 'preferences'\n",
                        mode);
        }
        return 0;
}

/* ------------------------- static functions */
