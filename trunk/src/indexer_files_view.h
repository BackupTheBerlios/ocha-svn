#ifndef INDEXER_FILES_H
#define INDEXER_FILES_H

#include "indexer.h"

/**
 * Create a new view for a source.
 * @param indexer files indexer (caller)
 * @return an instance of indexer_files_view
 */
struct indexer_source_view *indexer_files_view_new(struct indexer *indexer);

#endif /*INDEXER_FILES_H*/
