#ifndef INDEXER_UTILS_H
#define INDEXER_UTILS_H

/** \file Functions shared by more than one indexer */

#include "catalog.h"
#include "indexer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Index a file
 *
 * @param catalog
 * @param source_id source that will own the new entries
 * @param path full file path
 * @param filename just the filename (a part of path)
 * @param err error to set in case of failure
 * @return true if all went well, false otherwise (err is set)
 */
typedef gboolean (*handle_file_f)(struct catalog *catalog,
                                  int source_id,
                                  const char *path,
                                  const char *filename,
                                  GError **err,
                                  gpointer userdata);
/**
 * Go through the files in the given directory and index them.
 *
 * @param catalog catalog to index into
 * @param directory directory to go through
 * @param ignore_patterns patterns to ignore in addition to the hardcoded
 * patterns (null-terminated, may be null)
 * @param maxdepth maximum recursion depth, may be -1 for unlimited
 * @param slow set to true if you want the indexing to
 * be slowed down artificially
 * @param source_id source that will own the new entries
 * @param calllback file handler
 * @param userdata userdata for the file handler callback
 * @param err error to be set by the file handler if something
 * goes wrong
 * @return false if something goes wrong (check err, then), true
 * otherwise
 */
gboolean recurse(struct catalog *catalog,
                 const char *directory,
                 GPatternSpec **ignore_patterns,
                 int maxdepth,
                 gboolean slow,
                 int source_id,
                 handle_file_f callback,
                 gpointer userdata,
                 GError **err);

/**
 * Add an entry, with error handling
 * @param catalog
 * @param path
 * @param nam
 * @param long_name
 * @param source_id
 * @param err error to set if something went wrong
 * @return true if all went well, false otherwise
 */
gboolean catalog_addentry_witherrors(struct catalog *catalog, const char *path, const char *name, const char *long_name, int sourc_id, GError **err);

/** Set source attribute, set error if something goes wrong */
gboolean catalog_get_source_attribute_witherrors(struct catalog *catalog, int source_id, const char *attribute, char **value_out, gboolean required, GError **err);

/** Get the error quark for this domain, usually used through INDEXER_ERROR */
GQuark catalog_index_error_quark(void);

/** Error quark for this domain. */
#define INDEXER_ERROR catalog_index_error_quark()

/** Create a null-terminated array of patterns, given a colon-separated list of patterns */
GPatternSpec **create_patterns(const char *patterns);
/** Free a null-terminated array of patterns */
void free_patterns(GPatternSpec **patterns);

#endif /*INDEXER_UTILS_H*/
