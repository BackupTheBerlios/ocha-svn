#ifndef CATALOG_INDEXER_H
#define CATALOG_INDEXER_H

#include "catalog.h"
/** \file index directories and put that into the catalog */

/**
 * Trace callback.
 *
 * Pass such a function to catalog_index_set_callback() to
 * be told whenever a new entry is added.
 * @param path entry path or URI
 * @param name display name
 * @param userdata
 */
typedef void (*catalog_index_trace_callback_f)(const char *path, const char *name, gpointer userdata);

/**
 * Set a callback to be told whenever a new entry is added.
 *
 * @param callback the callback function to call or NULL for
 * no callback
 * @param userdata pointer to pass to the callback
 */
void catalog_index_set_trace_callback(catalog_index_trace_callback_f callback, gpointer userdata);

/** Call this function before any other functions in this module */
void catalog_index_init(void);

/**
 * Index or re-index the content of the directory.
 *
 * @param catalog
 * @param directory full path of the directory
 * @param maxdepth maximum depth to look for files in,
 * 1 means that only the files in the given directory
 * will be taken into account.
 * @param ignore additional files to ignore (in addition to the
 * hardcoded ones). This can either be NULL or be a NULL-terminated
 * array of unix glob patterns (*.bak). If a file or directory matches
 * one of the patterns, it'll be ignored (together with its content, if
 * it's a directory). Only * and ? are supported (no [..] character ranges)
 * @param slow slow down operation not to waste CPU cycles
 * @param err if this function returns FALSE and if this is non-null,
 * this will contain an error code and message (to be freed with g_error_free())
 * @return TRUE if all went well
 */
gboolean catalog_index_directory(struct catalog *catalog, const char *directory, int maxdepth, const char **ignore, gboolean slow, GError **err);

/**
 * Index or re-index the applications in the directory.
 *
 * This function only looks for files ending with .desktop.
 *
 * @param catalog
 * @param directory full path of the application directory
 * @param maxdepth maximum depth to look for files in,
 * 1 means that only the files in the given directory
 * will be taken into account.
 * @param slow slow down operation not to waste CPU cycles
 * @param err if this function returns FALSE and if this is non-null,
 * this will contain an error code and message (to be freed with g_error_free())
 * @return TRUE if all went well
 */
gboolean catalog_index_applications(struct catalog *catalog, const char *directory, int maxdepth, gboolean slow, GError **err);

/**
 * Index or re-index the entries in a bookmark file.
 *
 * For now, only mozilla bookmarks are supported.
 * @param catalog
 * @param bookmark_file mozila bookmark file
 * @param err if this function returns FALSE and if this is non-null,
 * this will contain an error code and message (to be freed with g_error_free())
 * @return TRUE if all went well
 */
gboolean catalog_index_bookmarks(struct catalog *catalog, const char *bookmark_file, GError **err);

/** Get the error quark for this domain, usually used through CATALOG_INDEX_ERROR */
GQuark catalog_index_error_quark(void);

/** Error quark for this domain. */
#define CATALOG_INDEX_ERROR catalog_index_error_quark()

/** Error codes */
typedef enum
   {
      /** error that comes directly or indirectly from the catalog */
      CATALOG_INDEX_CATALOG_ERROR,
      /** file passed to the function cannot be cataloged */
      CATALOG_INDEX_INVALID_INPUT,
      /** an error from some third-party module */
      CATALOG_INDEX_EXTERNAL_ERROR
   } CatalogIndexErrorCode;
#endif /*CATALOG_INDEXER_H*/
