#ifndef CATALOG_QUERYRUNNER_H
#define CATALOG_QUERYRUNNER_H

/** \file
 * Implementation of a queryrunner based
 * on the catalog of catalog.h
 */

#include "queryrunner.h"
#include "result_queue.h"

/**
 * Create a new catalog-based queryrunner.
 * @param path path of the catalog, which will be created if it doesn't exist yet
 * @return a new queryrunner or NULL (error)
 */
struct queryrunner *catalog_queryrunner_new(const char *path, struct result_queue *queue);

#endif /*CATALOG_QUERYRUNNER_H*/
