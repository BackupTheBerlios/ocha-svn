#ifndef CONTENTCONTENTLIST_H
#define CONTENTCONTENTLIST_H

#include <gtk/gtk.h>

/** \file Custom implementation of a GtkTreeModel that stores entries (API)
 *
 * This contentlist stores a fixed number of entries into an array contentlist,
 * possibly with undefined entries. Entries are always sorted
 * alphabetically (by the database).
 */

#define CONTENTLIST_TYPE            (contentlist_get_type ())
#define CONTENTLIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CONTENTLIST_TYPE, ContentList))
#define CONTENTLIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  CONTENTLIST_TYPE, ContentListClass))
#define IS_CONTENTLIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CONTENTLIST_TYPE))
#define IS_CONTENTLIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  CONTENTLIST_TYPE))
#define CONTENTLIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  CONTENTLIST_TYPE, ContentListClass))

/** Columns */
typedef enum
{
        /** True if there's something interesting in the other columns */
        CONTENTLIST_COL_DEFINED = 0,

        /** The entry's name */
        CONTENTLIST_COL_NAME,

        /** The entry's description */
        CONTENTLIST_COL_LONG_NAME,

        /** Total number of columns */
        CONTENTLIST_N_COLUMNS
} ContentListColumn;

/** Content list type */
typedef struct _ContentList ContentList;
/** Content list class type */
typedef struct _ContentListClass  ContentListClass;

/** Content list GType */
GType contentlist_get_type (void);

/**
 * Create a new ContentList.
 * @param size number of entries on the list (fixed once and for all)
 * @return the new contentlist, with the correctt number of entries,
 * but all unset.
 */
ContentList *contentlist_new (guint size);

/**
 * Define an entry given its index.
 *
 * Defining an entry can only be done once. Any
 * later calls are ignored.
 * @param contentlist
 * @param index between 0 and size-1
 * @param name short name
 * @param long_name description
 * @param entry id
 */
void contentlist_set(ContentList *contentlist, guint index, int id, const char *name, const char *long_name);

/**
 * Define an entry given its iter.
 *
 * Defining an entry can only be done once. Any
 * later calls are ignored.
 * @param contentlist
 * @param index between 0 and size-1
 * @param name short name
 * @param long_name description
 * @param entry id
 */
void contentlist_set_at_iter(ContentList *contentlist, GtkTreeIter *iter, int id, const char *name, const char *long_name);

/**
 * Get an entry's values given its index.
 *
 * If the entry's set, this function returns true with all
 * non-null fields set to the correct value. Otherwise,
 * the function returns false.
 *
 * @param contentlist
 * @param index between 0 and size-1
 * @param id_out if non-null and if the entry has been set, the entry id
 * @param name_out if non-null and if the entry has been set, the entry name. It'll
 * be valid for as long as the content list exists. Don't free or modify it.
 * @param long_name_out if non-null and if the entry has been set, the entry name. It'll
 * be valid for as long as the content list exists. Don't free or modify it.
 * @return true if the entry has been set, false otherwise
 */
gboolean contentlist_get(ContentList *contentlist, guint index, int *id_out, char **name_out, char **long_name_out);

/**
 * Get an entry's values given its iter.
 *
 * If the entry's set, this function returns true with all
 * non-null fields set to the correct value. Otherwise,
 * the function returns false.
 *
 * @param contentlist
 * @param index between 0 and size-1
 * @param id_out if non-null and if the entry has been set, the entry id
 * @param name_out if non-null and if the entry has been set, the entry name. It'll
 * be valid for as long as the content list exists. Don't free or modify it.
 * @param long_name_out if non-null and if the entry has been set, the entry name. It'll
 * be valid for as long as the content list exists. Don't free or modify it.
 * @return true if the entry has been set, false otherwise
 */
gboolean contentlist_get_at_iter(ContentList *contentlist, GtkTreeIter *iter, int *id_out, char **name_out, char **long_name_out);

/**
 * Get the size of the list.
 *
 * This is always the size that's been passed to the constructor.
 * @return number of rows on the list
 */
guint contentlist_get_size(ContentList *contentlist);
#endif
