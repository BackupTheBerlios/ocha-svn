#ifndef RESULTLIST_H
#define RESULTLIST_H

#include <gtk/gtk.h>
#include "result.h"

/** \file Maintain a gtk list view and model representing
 * the current result list.
 */
void resultlist_init();

GtkWidget *resultlist_get_widget();
struct result *resultlist_get_selected();
void resultlist_add_result(float pertinence, struct result *);
void resultlist_clear(void);
void resultlist_set_current_query(const char *query);

#endif
