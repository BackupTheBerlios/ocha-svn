#ifndef INDEXER_FILES_H
#define INDEXER_FILES_H

#include "indexer.h"

/**
 * Create a new view for a source.
 * @param indexer files indexer (caller)
 * @return an instance of indexer_files_view
 */
struct indexer_source_view *indexer_files_view_new(struct indexer *indexer);

/**
 * Create a limited version of the indexer file view
 * that's appropriate for other types of view than 'file'.
 *
 * The only requirement of this view is that the source
 * has an attribute called "path" that contains a path/URI
 * and that can be accessed for reading. If the attribute
 * "system" is present, it may be taken into account.
 *
 * The view will always be read-only.
 *
 * @param indexer indexer this source view should be based on
 */
struct indexer_source_view *indexer_files_view_new_pseudo_view(struct indexer *indexer);

#endif /*INDEXER_FILES_H*/
