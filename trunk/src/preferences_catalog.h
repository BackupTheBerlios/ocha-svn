#ifndef PREFERENCES_CATALOG_H
#define PREFERENCES_CATALOG_H

/** \file A UI that lets users access and modify the catalog.
 */
#include "catalog.h"
#include <gtk/gtk.h>

/**
 * Return an initialized GtkWidget that corresponds to the
 * catalog UI. It is expected that this widget be put into
 * some kind of global preference panel or in a tabbed
 * window. There's no OK or CANCEL button; everything happens
 * immediately.
 *
 * @param catalog an open catalog object that corresponds
 * to the current preference state. It must not be closed
 * as long as the preferences widget exists.
 * @return a new widget corresponding to the catalog preferences
 * Remember to call unref() to release this widget and the resources
 * associated with it.
 */
GtkWidget *preferences_catalog_new(struct catalog *catalog);

#endif /*PREFERENCES_CATALOG_H*/
