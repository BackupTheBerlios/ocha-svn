/** \file implementation of the common parts of the API defined in indexer.h */

#include "indexer.h"
#include "ocha_gconf.h"

/* ------------------------- prototypes */

/* ------------------------- public functions */
gboolean indexer_source_destroy(struct indexer_source *source, struct catalog *catalog)
{
        g_return_val_if_fail(source, FALSE);

        gboolean success=TRUE;

        if(!catalog_remove_source(catalog, source->id)) {
                success=FALSE;
        }

        gchar *path = ocha_gconf_get_source_key(source->indexer->name,
                                                source->id);
        if(!gconf_client_recursive_unset(ocha_gconf_get_client(),
                                         path,
                                         GCONF_UNSET_INCLUDING_SCHEMA_NAMES,
                                         NULL/*err*/)) {
                success=FALSE;
        }
        gconf_client_suggest_sync(ocha_gconf_get_client(), NULL/*err*/);
        g_free(path);

        indexer_source_release(source);
        return success;
}

/* ------------------------- member functions */

/* ------------------------- static functions */
