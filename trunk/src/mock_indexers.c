/** \file Empty indexer list. */

#include "indexers.h"
#include <check.h>
#include <stdio.h>
#include <string.h>

/* ------------------------- prototypes */
static gboolean mock_indexer_discover(struct indexer *, struct catalog *catalog);
static struct indexer_source *mock_indexer_load_source(struct indexer *self, struct catalog *catalog, int id);
static gboolean mock_indexer_execute(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err);
static gboolean mock_indexer_validate(struct indexer *, const char *name, const char *long_name, const char *path);

/* definitions */
static struct indexer mock_indexer = {
        "test",
        "Test Indexer",

        /* description */
        "Test Indexer Description",

        mock_indexer_discover,
        mock_indexer_load_source,
        mock_indexer_execute,
        mock_indexer_validate,
        NULL/*new_source*/
};

static struct indexer *indexers[] = {
        &mock_indexer,
        NULL
};

/* ------------------------- public functions */

struct indexer **indexers_list()
{
        return indexers;
}

struct indexer *indexers_get(const char *type)
{
        g_return_val_if_fail(type!=NULL, NULL);
        if(strcmp(type, "test")==0) {
                return &mock_indexer;
        }
        return NULL;
}

/* ------------------------- member functions: mock_indexer */
static gboolean mock_indexer_discover(struct indexer *indexer, struct catalog *catalog)
{
        fail_unless(indexer==&mock_indexer, "wrong indexer");
        return TRUE; /* all OK, indexer has no sources */
}

static struct indexer_source *mock_indexer_load_source(struct indexer *self, struct catalog *catalog, int id)
{
        fail_unless(self==&mock_indexer, "wrong indexer");
        fail("unexpected call to load_source");
        return NULL;
}
static gboolean mock_indexer_execute(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err)
{
        fail_unless(self==&mock_indexer, "wrong indexer");
        printf("execute %s (%s) %s\n",
               name,
               long_name,
               path);
        return FALSE;
}
static gboolean mock_indexer_validate(struct indexer *self, const char *name, const char *long_name, const char *path)
{
        fail_unless(self==&mock_indexer, "wrong indexer");
        /* always valid */
        return TRUE;
}

/* ------------------------- static functions  */

