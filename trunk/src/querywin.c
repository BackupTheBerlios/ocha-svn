#include "querywin.h"

/** \file
 * Create a query window and initialize the querwin structure.
 */


void querywin_create(struct querywin *retval)
{
  GtkWidget *querywin;
  GtkWidget *vbox1;
  GtkWidget *query;
  GtkWidget *scrolledwindow1;
  GtkWidget *list;

  querywin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (querywin, 320, 200);
  gtk_window_set_title (GTK_WINDOW (querywin), "window1");
  gtk_window_set_position (GTK_WINDOW (querywin), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_resizable (GTK_WINDOW (querywin), FALSE);
  gtk_window_set_decorated (GTK_WINDOW (querywin), FALSE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (querywin), TRUE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (querywin), TRUE);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (querywin), vbox1);

  query = gtk_label_new ("");
  gtk_widget_show (query);
  gtk_box_pack_start (GTK_BOX (vbox1), query, FALSE, FALSE, 0);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow1);
  gtk_box_pack_start (GTK_BOX (vbox1), scrolledwindow1, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  list = gtk_tree_view_new();
  gtk_widget_show (list);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), list);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list), FALSE);

  /* fill in structure */
  retval->querywin=querywin;
  retval->query=query;
  retval->list=list;
}


