#ifndef INDEXER_VIEW_H
#define INDEXER_VIEW_H

#include <gtk/gtk.h>
#include "indexer.h"


/** \file
 * A reusable properties window for preferences_catalog.c
 * that displays and controls sources and indexers.
 */

/**
 * Create an empty indexer view
 *
 * @param catalog a catalog that must remain open
 * for the lifetime of this view
 */
struct indexer_view *indexer_view_new(struct catalog *catalog);

/**
 * Get this view's widget
 * @param view
 * @return the widget, always the same for a given indexer_vier
 */
GtkWidget *indexer_view_widget(struct indexer_view *view);

/**
 * Destroy the indexer view.
 *
 * If the view was attached to something
 * it is first detached.
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
 */
void indexer_view_attach_indexer(struct indexer_view *view,
                                 struct indexer *indexer);

/**
 * Make the indexer view display the
 *
 * If the view was already attached to something
 * else, it is detached before re-attaching it.
 *
 * @param view
 * @param indexer
 * @param source
 */
void indexer_view_attach_source(struct indexer_view *view,
                                struct indexer *indexer,
                                struct indexer_source *source);

/**
 * Put the view back in 'detached' (empty) mode.
 * @param view
 */
void indexer_view_detach(struct indexer_view *view);

#endif /* INDEXER_VIEW_H */
