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
 * Add a result to the list.
 *
 * @param pertinence
 * @param result
 */
void resultlist_add_result(float pertinence, struct result *);

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
