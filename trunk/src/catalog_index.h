#ifndef CATALOG_INDEXER_H
#define CATALOG_INDEXER_H

#include "catalog.h"
/** \file index directories and put that into the catalog */

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
 * @return true if all went well
 */
bool catalog_index_directory(struct catalog *catalog, const char *directory, int maxdepth, const char **ignore, bool slow);

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
 * @return true if all went well
 */
bool catalog_index_applications(struct catalog *catalog, const char *directory, int maxdepth, bool slow);

/**
 * Index or re-index the entries in a bookmark file.
 *
 * For now, only mozilla bookmarks are supported.
 * @param catalog
 * @param bookmark_file mozila bookmark file
 * @return true if all went well
 */
bool catalog_index_bookmarks(struct catalog *catalog, const char *bookmark_file);

#endif /*CATALOG_INDEXER_H*/
