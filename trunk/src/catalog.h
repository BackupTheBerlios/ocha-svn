#ifndef CATALOG_H
#define CATALOG_H

#include "result.h"
#include "result_queue.h"

/** \file interface to an sqllite-based catalog
 */

/** error codes returned by catalog connect */
typedef enum
   {
      CATALOG_CONNECTION_ERROR,
      CATALOG_CANNOT_CREATE_TABLES
   } CatalogErrorCodes;

/** get the error quark for the errors returned by catalog */
GQuark catalog_error_quark();

/**
 * Make a connection to an sqlite catalog,
 * creating it if necessary.
 *
 * @param path path to the catalog file
 * @param if non-null and if this function returns
 * NULL, you'll find there an error message and
 * code explaining why connecting to the database
 * failed (to be freed with g_error_free())
 * @return NULL if the catalog could not be open/created
 */
struct catalog *catalog_connect(const char *path, GError **errs);

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
 * @param userdata pointer passed to catalog_executequery()
 * @return TRUE to continue adding results, FALSE to stop looking for results
 */
typedef gboolean (*catalog_callback_f)(struct catalog *catalog, float pertinence, struct result *result, void *userdata);

/**
 * Execute a query and add the results into
 * the query runner.
 *
 * If the catalog has been interrupted some time before, th
 * query will return immediately. Use catalog_restart() to
 * recover from an interruption.
 *
 * @param catalog the catalog
 * @param query query to run
 * @param callback function to call for every results
 * @param userdata userdata to pass to the callback
 * @param if non-null and if this function returns
 * FALSE, you'll find there an error message (in english)
 * explaining why the query failed. This is a static
 * pointer; it is always valid and does not need to be freed.
 * @param userdata pointer to pass to the callback
 * @return return FALSE if there was a fatal error, in
 * which case the catalog must be immediately
 * disconnected.
 */
gboolean catalog_executequery(struct catalog *catalog,
                          const char *query,
                          catalog_callback_f callback,
                          void *userdata);

/**
 * Interrupt any currently running query.
 *
 * Calling catalog_interrupt() does not only stop the current query; it stops
 * all subsequent queries as well until catalog_restart() is called. See
 * catalog_restart()
 *
 * This function can be called from another
 * thread than the one running catalog_executequery.
 * catalog_interrupt() and catalog_restart() are the only
 * function that are safe to call from another thread
 * than the one running the queries.
 *
 * @param catalog the catalog
 */
void catalog_interrupt(struct catalog *catalog);

/**
 * Put the catalog back into a normal state after a call to catalog_interrupt().
 *
 * Calling catalog_interrupt() does not only stop the current query; it stops
 * all subsequent queries as well - until this method is called. The rationale
 * for this is to make interruption from other threads manageable from outside,
 * that is, to make the caller responsible for handling synchronization. The
 * alternative would have been to reset the interrupted flag when a new query
 * starts. This would have been difficult to handle in a multithreaded scenario.
 *
 * This function can be called from another
 * thread than the one running catalog_executequery.
 * catalog_interrupt() and catalog_restart() are the only
 * function that are safe to call from another thread
 * than the one running the queries.
 *
 * @param catalog the catalog
 */
void catalog_restart(struct catalog *catalog);

/**
 * Add a command into the catalog.
 * @param catalog
 * @param name command name (used to find it afterwards)
 * @param execute command to execute, with %s to replace with the file name
 * @param id_out if non-null, this variable will be set to the generated ID of the command
 * @return TRUE if the command was added, FALSE otherwise
 */
gboolean catalog_addcommand(struct catalog *catalog, const char *name, const char *execute, int *id_out);

/**
 * Find a command by name.
 * @param catalog
 * @param name command name (used to find it afterwards)
 * @param id_out if non-null, this variable will be set to the generated ID of the command
 * @return TRUE if the command was found, FALSE otherwise
 */
gboolean catalog_findcommand(struct catalog *catalog, const char *name, int *id_out);

/**
 * Find an entry by path.
 * @param catalog
 * @param path entry path
 * @param id_out if non-null, this variable will be set to the generated ID of the command
 * @return TRUE if the command was found, FALSE otherwise
 */
gboolean catalog_findentry(struct catalog *catalog, const char *path, int *id_out);

/**
 * Add an entry into the catalog.
 * @param catalog
 * @param path local path or URI
 * @param display_name
 * @param command_id id of a command to use to execute the file
 * @param id_out if non-null, this variable will be set to the generated ID of the entry
 * @return TRUE if the entry was added, FALSE otherwise
 */
gboolean catalog_addentry(struct catalog *catalog, const char *path, const char *display_name, int command_id, int *id_out);

/**
 * Get a pointer on the last error that happened with
 * this catalog.
 * @param catalog catalog to query
 * @return a pointer to the last error message, never NULL. this
 * pointer is only valid until the next call to a catalog_ function
 * on the same catalog
 */
const char *catalog_error(struct catalog *catalog);
#endif /* CATALOG_H */
