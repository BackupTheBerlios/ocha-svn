#ifndef QUERY_H
#define QUERY_H

#include "result.h"

/** \file
 * The functions in this file define how a query
 * is to be interpreted given a result.
 *
 * Other implementations of query matching may exist
 * that are more appropriate to how the result are
 * stored (it can be an SQL query, for example), but
 * they should all be compatible with the query
 * matching algorithm defined here.
 *
 */

/**
 * Return TRUE if the query matches the given name.
 *
 * If you've got a result already, use query_result_ismatch()
 * instead.
 *
 * @param query
 * @param name
 * @return TRUE if the name matches the query
 */
gboolean query_ismatch(const char *query, const char *name);

/**
 * Return TRUE if the query matches the given result.
 *
 * @param query
 * @param result
 * @return TRUE if the name matches the query
 */
gboolean query_result_ismatch(const char *query, const struct result *result);

/**
 * Highlight a string that's queried on using pango markup.
 *
 * @param query the query
 * @param str the string to compare the query with
 * @param highlight_on pango markup that opens a highlighted zone
 * @param highlight_off pango markup that closes a highlighted zone
 * @return a newly-allocated string that contains valid pango markup, to
 * free with g_free()
 */
const char *query_pango_highlight(const char *query, const char *str, const char *highlight_on, const char *highlight_off);
#endif /*QUERY_H*/
