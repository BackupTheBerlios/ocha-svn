#ifndef CONTENT_VIEW_H
#define CONTENT_VIEW_H

/** \file Manage a view on the content of a source
 *
 */
#include "catalog.h"
#include <gtk/gtk.h>

/**
 * Create a new content view and link it with a catalog.
 *
 * The view is empty, because it's not linked to any source. Link
 * it to a source with content_view_attach() and unlink it with
 * content_view_detach()
 *
 * @param catalog a catalog that must stay open for
 * as long as the view is in use
 * @return a content view, get the widget using content_view_get_widget()
 */
struct content_view *content_view_new(struct catalog *catalog);

/**
 * Get rid of a view .
 *
 * You must call this even though you've destroyed the container
 * of the widget.
 *
 * @param view the view to delete
 */
void content_view_destroy(struct content_view *view);

/**
 * Attach the content view to a source.
 *
 * If it was attached to a source, it'll be detached automatically.
 * @param view
 * @param source_id source ID
 */
void content_view_attach(struct content_view *view, int source_id);

/**
 * If the view is currently attached to a source,
 * re-run the query and update the content.
 */
void content_view_refresh(struct content_view *view);

/**
 * Detach the content view from any source.
 * @param view
 */
void content_view_detach(struct content_view *view);

/**
 * Get this view's widget
 * @param view
 * @return a widget
 */
GtkWidget *content_view_get_widget(struct content_view *view);

#endif /* CONTENT_VIEW_H */
