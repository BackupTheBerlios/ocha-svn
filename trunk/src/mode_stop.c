#include "mode_stop.h"
#include "ocha_init.h"
#include <stdio.h>

/** \file Stop ocha
 *
 */

/* ------------------------- prototypes: static functions */

/* ------------------------- definitions */

/* ------------------------- public functions */
int mode_stop(int argc, char *argv[])
{
        if(ocha_init_kill()) {
                printf("ocha: killed\n");
                return 0;
        } else {
                return 1;
        }
}

/* ------------------------- static functions */

