#ifndef CATALOG_UPDATEDB_H
#define CATALOG_UPDATEDB_H
#include "catalog.h"

/** \file
 * Implementation of the catalog.h API based on updatedb/locate.
 *
 * This module implements a catalog that does its search using
 * locate. It's a slow method, but one that does not require
 * extra parsing of the catalog. 
 */

/**
 * Create a new catalog structure for 
 * searches based on updatedb
 */
struct catalog *catalog_updatedb_new(void);

#endif
