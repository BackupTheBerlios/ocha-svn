#ifndef STRING_SET_H
#define STRING_SET_H

#include <glib.h>

/** \file Simple string set API.
 *
 * This defines a simple string set based on
 * a GHashtable
 */

/**
 * Create a new empty string set.
 */
struct string_set *string_set_new(void);

/**
 * Free the string set and any memory it used
 * @param set
 */
void string_set_free(struct string_set *set);

/**
 * Add a string into the set.
 *
 * If the string already is in the set, this
 * function does nothing.
 *
 * @param set
 * @param entry a string to add (will be copied)
 */
void string_set_add(struct string_set *set, const char *str);

/**
 * Remove a string from the set.
 *
 * If the string is not in the set, this
 * function does nothing.
 *
 * @param set
 * @param entry a string to remove
 */
void string_set_remove(struct string_set *set, const char *str);

/**
 * Check whether a string is in the set.
 *
 * @param set
 * @param entry a string
 * @return true if the string is in the set, false otherwise
 */
gboolean string_set_contains(struct string_set *set, const char *str);

#endif /* STRING_SET_H */
