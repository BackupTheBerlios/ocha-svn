#ifndef INDEXER_VIEW_H
#define INDEXER_VIEW_H

#include "indexer.h"


/** \file
 * A reusable properties window for preferences_catalog.c
 * that displays and controls sources and indexers.
 */

/**
 * Create a new indexer view
 * in detached mode (invisible)
 *
 * @param catalog a catalog that must remain open
 * for the lifetime of this view
 */
struct indexer_view *indexer_view_new(struct catalog *catalog);

/**
 * Destroy the indexer view,
 *
 * If the view was to something
 * it is detached.
 *
 * @param view
 */
void indexer_view_destroy(struct indexer_view *view);

/**
 * Make the indexer view display the given indexer.
 *
 * If the view was already attached to something
 * else, it is detached before re-attaching it.
 * @param view
 * @param indexer
 * @param activate if TRUE, activate the window after
 * attaching
 */
void indexer_view_attach_indexer(struct indexer_view *view,
                                 struct indexer *indexer, gboolean activate);

/**
 * Make the indexer view display the
 *
 * If the view was already attached to something
 * else, it is detached before re-attaching it.
 *
 * @param view
 * @param indexer
 * @param source_id
 * @param activate if TRUE, activate the window after
 * attaching
 */
void indexer_view_attach_source(struct indexer_view *view,
                                struct indexer *indexer,
                                int source_id,
                                gboolean activate);

/**
 * Put the indexer view in detached mode (invisible, no current indexer or source)
 * @param view
 */
void indexer_view_detach(struct indexer_view *view);

/**
 * Check whether the view window is currently open
 * @param view
 * @return true if the window is open
 */
gboolean indexer_view_is_visible(struct indexer_view *view);
#endif /* INDEXER_VIEW_H */
