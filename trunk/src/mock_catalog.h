#ifndef MOCK_CATALOG
#define MOCK_CATALOG

/** \file interface of an incomplete mock catalog (used for testing) */

#include "catalog.h"

/** Create a new mock catalog, with no expectations. */
struct catalog *mock_catalog_new(void);

/**
 * Expect catalog_addcommand() calls.
 * @param catalog
 * @param name command name
 * @param execute command
 * @param id id to assign to the command
 */
void mock_catalog_expect_addcommand(struct catalog *catalog, const char *name, const char *execute, int id);

/**
 * Expect catalog_addentry() calls.
 * @param catalog
 * @param path path or URI, unique id of the entry
 * @param name display name of the entry
 * @param long_name description or full name of the entry
 * @param command_id command to link the entry to
 * @param id id to assign to the entry
 */
void mock_catalog_expect_addentry(struct catalog *catalog, const char *path, const char *name, const char *long_name, int command_id, int id);

/**
 * Fail unless all calls have been made as expected.
 *
 * If there's an error, it'll be printed to stderr and
 * the function will return FALSE
 * @param catalog
 */
void mock_catalog_assert_expectations_met(struct catalog *catalog);

#endif /*MOCK_CATALOG*/
