#ifndef CATALOG_INDEXER_H
#define CATALOG_INDEXER_H

#include "catalog.h"
/** \file index directories and put that into the catalog */


/**
 * Index or re-index the content of the directory.
 *
 * @param catalog
 * @param directory full path of the directory
 * @param maxdepth maximum depth to look for files in,
 * 1 means that only the files in the given directory
 * will be taken into account.
 * @param slow slow down operation not to waste CPU cycles
 * @return true if all went well
 */
bool catalog_index_directory(struct catalog *catalog, const char *directory, int maxdepth, bool slow);

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
