#include "launchers.h"
#include "launcher_open.h"
#include "launcher_openurl.h"
#include "launcher_application.h"
#include <string.h>

/** \file Keep a list of launchers available on the system
 *
 */

/* ------------------------- prototypes: static functions */

/* ------------------------- definitions */
static struct launcher *list[] = {
        &launcher_open,
        &launcher_openurl,
        &launcher_application,
        NULL
};

/* ------------------------- public functions */
struct launcher **launchers_list(void)
{
        return list;
}

struct launcher *launchers_get(const char *id)
{
        int i;
        for(i=0; list[i]!=NULL; i++)
        {
                if(strcmp(id, list[i]->id)==0)
                        return list[i];
        }
        return NULL;
}

/* ------------------------- static functions */

