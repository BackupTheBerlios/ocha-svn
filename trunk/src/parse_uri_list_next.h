#ifndef PARSE_URI_LIST_NEXT_H
#define PARSE_URI_LIST_NEXT_H

/** \file a function to parse URI-list and return
 * only valid URIs.
 */
#include <glib.h>

/**
 * Iterate over the entries in a uri list and return
 * URIs.
 *
 * Attempts will be made to 'fix' invalid file: URI. The separator
 * is supposed to be : \r\n, but just \r or just \n is supported anyway.
 * If a 'URI' starts with \, it'll be transformed into a file URI
 * using gnome (which doesn't just add file:/// at the beginnig...).
 *
 * @param data_ptr pointer to the beginnig of the data (will be modified)
 * @param length_ptr length of the data (will be modified)
 * @param dest GString that will receive the next URI
 * @return TRUE if a URI was found and written into dest, FALSE
 * if the end of the list was reached
 */
gboolean parse_uri_list_next(gchar **data_ptr, gint *length_ptr, GString *dest);



#endif /* PARSE_URI_LIST_NEXT_H */
