/** \file implementation of the API defined in catalog.h */

#include "catalog.h"
#include <sqlite.h>

/** Hidden catalog structure */
struct catalog
{
   bool stop;
};

struct catalog *catalog_connect(const char *path, char **errmsg)
{
   g_return_val_if_fail(path, NULL);

   struct catalog *catalog = g_new(struct catalog, 1);
   catalog->stop=false;

   return catalog;
}

void catalog_disconnect(struct catalog *catalog)
{
   g_return_if_fail(catalog!=NULL);
   g_free(catalog);
}

void catalog_interrupt(struct catalog *catalog)
{
   g_return_if_fail(catalog!=NULL);
   catalog->stop=true;
}

bool catalog_executequery(struct catalog *catalog,
                          const char *query,
                          catalog_callback_f callback,
                          char **errmsg)

{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(query!=NULL, false);
   g_return_val_if_fail(callback!=NULL, false);
   return false;
}
