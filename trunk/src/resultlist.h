#ifndef RESULTLIST_H
#define RESULTLIST_H

#include <gtk/gtk.h>
#include "result.h"

/** \file Maintain a gtk list view and model representing
 * the current result list.
 */

/**
 * Create the result list.
 *
 * Make sure you call this function before any other
 * in this module.
 */
void resultlist_init();

/**
 * Get the treeview widget that's the view of the result list.
 *
 * Make sure you've called resultlist_init() before this
 * function, otherwise the widget will not have been created.
 *
 * @return the view tree widget created by resultlist_init()
 */
GtkWidget *resultlist_get_widget(void);

/**
 * Get the currently selected result or NULL
 *
 * @return the selected result or NULL
 */
struct result *resultlist_get_selected(void);

/**
 * Remove all results from the list.
 */
void resultlist_clear(void);

/**
 * Add a result to the list.
 *
 * @param query query for this result
 * @param pertinence
 * @param result
 */
void resultlist_add_result(const char *query, float pertinence, struct result *);

/**
 * Tell the list that the given result has been executed.
 * <P>
 * The result should be on the list. It's usually the
 * one that's selected. (but there's no guarantee). The
 * result list may choose to display it differently.
 *
 * @param result the result
 */
void resultlist_executed(struct result *result);

/**
 * Set the current query, for highlighting it on
 * the result list.
 *
 * Newly-irrelevant results are removed by this function.
 * @param query new query string to highlight
 */
void resultlist_set_current_query(const char *query);

/**
 * Remove the results that have become invalid.
 * This function should be called after long inactivity
 * periods, during which files may have been deleted or
 * removed or windows closed.
 */
void resultlist_verify(void);

#endif
