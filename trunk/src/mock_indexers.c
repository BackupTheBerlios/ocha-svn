/** \file Empty indexer list. */

#include "indexers.h"
#include <check.h>
#include <stdio.h>

static gboolean discover(struct indexer *, struct catalog *catalog);
static struct indexer_source *load_source(struct indexer *self, struct catalog *catalog, int id);
static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err);
static gboolean validate(struct indexer *, const char *name, const char *long_name, const char *path);

static struct indexer mock_indexer =
{
    "test",
    discover,
    load_source,
    execute,
    validate
};

static struct indexer *indexers[] =
   {
       &mock_indexer,
       NULL
   };

struct indexer **indexers_list()
{
    return indexers;
}


static gboolean discover(struct indexer *indexer, struct catalog *catalog)
{
    fail_unless(indexer==&mock_indexer, "wrong indexer");
    return TRUE; /* all OK, indexer has no sources */
}

static struct indexer_source *load_source(struct indexer *self, struct catalog *catalog, int id)
{
    fail_unless(self==&mock_indexer, "wrong indexer");
    fail("unexpected call to load_source");
}
static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err)
{
    fail_unless(self==&mock_indexer, "wrong indexer");
    printf("execute %s (%s) %s\n",
           name,
           long_name,
           path);
}
static gboolean validate(struct indexer *self, const char *name, const char *long_name, const char *path)
{
    fail_unless(self==&mock_indexer, "wrong indexer");
    /* always valid */
    return TRUE;
}
