#ifndef CATALOG_RESULT_H
#define CATALOG_RESULT_H

/** \file private catalog-based result implementation */

#include "catalog.h"
#include "launcher.h"

struct result *catalog_result_create(const char *catalog_path, struct launcher *launcher, const char *path, const char *name, const char *long_name, int entry_id);


#endif /*CATALOG_RESULT_H*/
