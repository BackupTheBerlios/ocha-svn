/** \file Maintain a list of available indexers. */

#include "indexers.h"
#include "indexer_files.h"
#include "indexer_applications.h"
#include "indexer_mozilla.h"
#include <string.h>

static struct indexer *indexers[] = {
        &indexer_files,
        &indexer_applications,
        &indexer_mozilla,
        NULL
};

/* ------------------------- prototypes */

/* ------------------------- public functions */

struct indexer **indexers_list()
{
        return indexers;
}

struct indexer *indexers_get(const char *type)
{
        struct indexer **ptr;
        g_return_val_if_fail(type!=NULL, NULL);

        for(ptr=indexers; *ptr; ptr++) {
                if(strcmp(type, (*ptr)->name)==0)
                        return *ptr;
        }
        return NULL;
}

/* ------------------------- static functions */
