#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <glib.h>
#include "locate.h"

/** \file
 * Implemetation of API defined in locate.h
 */

struct locate *locate_new(const char *query)
{
   g_return_val_if_fail(query!=NULL, NULL);

   return NULL;
}

void locate_delete(struct locate *locate)
{
   g_return_if_fail(locate!=NULL);
}

bool locate_has_more(struct locate *locate)
{
   g_return_val_if_fail(locate!=NULL, true);
}

bool locate_next(struct locate *locate, GString* dest)
{
   g_return_val_if_fail(locate!=NULL, false);
   g_return_val_if_fail(dest!=NULL, false);
}
