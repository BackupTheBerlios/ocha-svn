#ifndef PREFERENCES_CATALOG_H
#define PREFERENCES_CATALOG_H

/** \file A UI that lets users access and modify the catalog.
 */
#include "catalog.h"
#include <gtk/gtk.h>

/**
 * Create a preference catalog object.
 *
 * The catalog should stay open until preferences_catalog_free()
 * is called.
 *
 * @param catalog an open catalog object that corresponds
 * to the current preference state. It must not be closed
 * as long as the preferences widget exists.
 * @return an initialized preferences_catalog structure
 * to free with preferences_catalog_free()
 */
struct preferences_catalog *preferences_catalog_new(struct catalog *catalog);

/**
 * Return an initialized GtkWidget that corresponds to the
 * catalog UI. It is expected that this widget be put into
 * some kind of global preference panel or in a tabbed
 * window. There's no OK or CANCEL button; everything happens
 * immediately.
 * @param prefs
 * @return a widget
 */
GtkWidget *preferences_catalog_get_widget(struct preferences_catalog *prefs);

/**
 * Free the preference catalog and release anything it held
 * @param prefs
 */
void preferences_catalog_free(struct preferences_catalog *prefs);

#endif /*PREFERENCES_CATALOG_H*/
