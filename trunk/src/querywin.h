#ifndef QUERYWIN_H
#define QUERYWIN_H

#include <gtk/gtk.h>

/**
 * Handle to interesting widgets
 */
struct querywin
{
   GtkWidget *querywin;
   GtkWidget *query_label;
   GtkWidget *treeview;
};

/**
 * Create the GtkWigets for the query window
 * and fill a structure querywin.
 * @param querywin a structure querywin to fill
 */
void querywin_create(struct querywin *);

#endif /*QUERYWIN_H*/
