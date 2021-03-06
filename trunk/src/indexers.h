#ifndef INDEXERS_H
#define INDEXERS_H

/** \file global list of indexers */

#include "indexer.h"

/**
 * Get the null-terminated global list of indexers.
 * This function will always return the same list after
 * the first time (where it may need to look it up)
 */
struct indexer **indexers_list(void);

/**
 * Get an indexer by name.
 * @return an indexer or NULL if not found
 */
struct indexer *indexers_get(const char *name);

#endif /*INDEXERS_H*/
