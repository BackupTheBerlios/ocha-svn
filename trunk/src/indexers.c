/** \file Maintain a list of available indexers. */

#include "indexers.h"
#include "indexer_files.h"
#include "indexer_applications.h"
#include "indexer_mozilla.h"

static struct indexer *indexers[] =
   {
      &indexer_files,
      &indexer_mozilla,
      &indexer_applications,
      NULL
   };

struct indexer **indexers_list()
{
    return indexers;
}
