#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <glib.h>
#include <string.h>

/**
 * Remove whitespaces (' ' and '\t') from the beginning
 * and end of a gstring
 * @param gstr gstring
 */
void strstrip_on_gstring(GString *gstr);

/**
 * Compare two strings, ignoring any whitespaces (' ' and '\t')
 * from the beginning or the end of either strings.
 * @param a
 * @param b
 * @return true if they're the same, false otherwise
 */
gboolean string_equals_ignore_spaces(const char *a, const char *b);

/**
 * Compare two strings
 * @param a
 * @param b
 * @return true if they're the same, false otherwise
 */
#define string_equals(a, b) (strcmp((a), (b))==0)

#endif /*STRING_UTILS_H*/
