#ifndef OCHA_GCONF_H
#define OCHA_GCONF_H

#include <gconf/gconf-client.h>

/** \file standard access to gconf for ocha applications */

#define OCHA_GCONF_PREFIX "/apps/ocha"
#define OCHA_GCONF_INDEXERS OCHA_GCONF_PREFIX "/indexer"
#define OCHA_GCONF_INDEXER(name) OCHA_INDEXERS_PREFIX "/" ##name
#define OCHA_GCONF_INDEXER_KEY(indexer, key) OCHA_INDEXERS_PREFIX "/" ##indexer "/" ##key
#define OCHA_GCONF_ACCELERATOR_KEY OCHA_GCONF_PREFIX "/accelerator"
#define OCHA_GCONF_ACCELERATOR_KEY_DEFAULT "<Alt>space"
/**
 * Get a properly initialized GConf client.
 * The client will be available for as long
 * as the program is running and it won't change.
 */
GConfClient *ocha_gconf_get_client(void);


/**
 * Check whether configuration exists.
 * When this returns false, it means that
 * the current user is running the program for the
 * first time.
 */
gboolean ocha_gconf_exists(void);

/**
 * List sources for the given indexer.
 *
 * @param indexer indexer name
 * @param ids_out if non-null this array will contain the ids
 * of the sources of the given type. The array will have
 * to be released using g_free() by the caller. If there
 * are no sources of this type, *ids_out  will be null and
 * *ids_len_out will be 0
 * @param ids_len_out this integer will be set to the
 * number of entries in ids_out. Must not be null.
 */
void ocha_gconf_get_sources(const char *type, int **ids_out, int *ids_len_out);

/**
 * Get the value of a source attribute as a string.
 * @param source_id
 * @param attribute attribute name
 * @return a string to be g_free'd or NULL
 */
char *ocha_gconf_get_source_attribute(const char *type, int source_id, const char *attribute);

/**
 * Set the special 'system' flag that marks a source as
 * being read-only.
 */
void ocha_gconf_set_system(const char *type, int source_id, gboolean system);

/**
 * Get the special 'system' flag for a source
 * @return TRUE if it's a system source, FALSE otherwise
 */
gboolean ocha_gconf_is_system(const char *type, int source_id);

/**
 * Set the value of a source attribute as a string.
 * @param source_id
 * @param attribute attribute name
 * @param value attribute value (may be NULL)
 * @return true if it worked
 */
gboolean ocha_gconf_set_source_attribute(const char *type, int source_id, const char *attribute, const char *value);

/**
 * Get the gconf key to the directory of the given source.
 *
 * This function doesn't check whether the directory actually exists.
 * @param type indexer name
 * @param source_id
 * @return a key to be freed with g_free().
 */
gchar *ocha_gconf_get_source_key(const char *type, int source_id);

/**
 * Get the gconf key to an attribue of the given source.
 *
 * This function doesn't check whether the directory actually exists.
 * @param type indexer name
 * @param source_id
 * @param attribute
 * @return a key to be freed with g_free().
 */
gchar *ocha_gconf_get_source_attribute_key(const char *type, int source_id, const char *attribute);

#endif /*OCHA_GCONF_H*/
