#ifndef CATALOG_H
#define CATALOG_H

#include "result.h"
#include "result_queue.h"

/** \file interface to an sqllite-based catalog
 */

/**
 * Make a connection to an sqlite catalog,
 * creating it if necessary.
 *
 * @param path path to the catalog file
 * @param if non-null and if this function returns
 * NULL, you'll find there an error message (in english)
 * explaining why the query failed. This is a static
 * pointer; it is always valid and does not need to be freed.
 * @return NULL if the catalog could not be open/created
 */
struct catalog *catalog_connect(const char *path, char **errormsg);

/**
 * Disconnect from the catalog, free any
 * resource.
 *
 * Results created by this catalog are NOT freed
 * automatically.
 * @param catalog catalog returned by catalog_connect()
 */
void catalog_disconnect(struct catalog *catalog);

/**
 * Receive the results from catalog_executequery().
 *
 * @param catalog
 * @param pertinence pertinence of the result, between 0.0 and 1.0
 * @param result the result itself. the callback is responsible for
 * releasing it eventually
 * @return true to continue adding results, false to stop looking for results
 */
typedef bool (*catalog_callback_f)(struct catalog *catalog, float pertinence, struct result *result);

/**
 * Interrupt any currently running query.
 *
 * This function can be called from another
 * thread than the one running catalog_executequery.
 * It is the only function that's safe to call
 * from another thread...
 *
 * If no query is running, this call does nothing.
 * @param catalog the catalog
 */
void catalog_interrupt(struct catalog *catalog);

/**
 * Execute a query and add the results into
 * the query runner.
 *
 * @param catalog the catalog
 * @param query query to run
 * @param callback function to call for every results
 * @param userdata userdata to pass to the callback
 * @param if non-null and if this function returns
 * false, you'll find there an error message (in english)
 * explaining why the query failed. This is a static
 * pointer; it is always valid and does not need to be freed.
 * @return return false if there was a fatal error, in
 * which case the catalog must be immediately
 * disconnected.
 */
bool catalog_executequery(struct catalog *catalog,
                          const char *query,
                          catalog_callback_f callback,
                          char **errormsg);

#endif /* CATALOG_H */
