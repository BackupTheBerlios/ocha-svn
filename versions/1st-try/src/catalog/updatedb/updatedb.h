#ifndef CATALOG_UPDATEDB_H
#define CATALOG_UPDATEDB_H
#include <stdbool.h>
#include <catalog.h>

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
struct catalog *updatedb_new(void);

/**
 * Check if 'locate' works on this machine.
 *
 * This test will fail if locate is not installed
 * on this machine, if the database does not
 * exist or if the database is invalid
 *
 * The updatedb catalog is useless unless
 * this test passes.
 *
 * @return true if locate works, false
 * otherwise
 */
bool updatedb_is_available(void);

#endif
