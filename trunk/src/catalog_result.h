#ifndef CATALOG_RESULT_H
#define CATALOG_RESULT_H

/** \file private catalog-based result implementation */

#include "catalog.h"

struct result *catalog_result_create(const char *catalog_path, const char *path, const char *name, const char *long_name, const char *execute, int entry_id);


#endif /*CATALOG_RESULT_H*/
