#ifndef INDEXER_VIEWS_H
#define INDEXER_VIEWS_H

#include "indexer.h"
#include "catalog.h"

/** \file Manager indexer view (properties) windows.
 *
 */

/**
 * Create a new indexer view manager structure
 * @param catalog catalog connection that must remain open
 * as long as the views is open
 * @return new view manager to free with indexer_views_free
 */
struct indexer_views *indexer_views_new(struct catalog *catalog);

/**
 * Close all indexer views and free any memory associated
 * with the view list.
 * @param views structure created with indexer_views_new
 */
void indexer_views_free(struct indexer_views *views);

/**
 * Refresh one indexer view (if it's open).
 * @param views
 * @param indexer
 * @param source
 */
void indexer_views_refresh_source(struct indexer_views *views, struct indexer  *indexer, struct indexer_source  *source);

/**
 * Close one indexer view (if it's open).
 * @param views
 * @param indexer
 * @param source
 */
void indexer_views_delete_source(struct indexer_views *views, struct indexer  *indexer, struct indexer_source  *source);

/**
 * Open a new view for the given source, or bring an existing view to the front.
 * @param views
 * @param indexer
 * @param source
 */
void indexer_views_open(struct indexer_views  *views, struct indexer  *indexer, struct indexer_source  *source);

#endif /* INDEXER_VIEWS_H */
