#ifndef INDEXERS_H
#define INDEXERS_H

/** \file global list of indexers */

#include "indexer.h"

/**
 * Get the null-terminated global list of indexers.
 * This function will always return the same list after
 * the first time (where it may need to look it up)
 */
struct indexer **indexers_list();

#endif /*INDEXERS_H*/
