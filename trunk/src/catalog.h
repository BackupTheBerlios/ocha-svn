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
 * @param entry_id ID of the entry
 * @param name entry name, in UTF-8. released by the caller after the function returns
 * @param long_name long entry name, in UTF-8. released by the caller after the function returns
 * @param path path/uri, in UTF-8. released by the caller after the function returns
 * @param source_id ID of the source this entry belongs to
 * @param source_type type of the source this entry belongs to
 * @param userdata pointer passed to catalog_executequery()
 * @return TRUE to continue adding results, FALSE to stop looking for results
 */
typedef gboolean (*catalog_callback_f)(struct catalog *catalog,
                                       float pertinence,
                                       int entry_id,
                                       const char *name,
                                       const char *long_name,
                                       const char *path,
                                       int source_id,
                                       const char *source_type,
                                       void *userdata);

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
 * @return return FALSE if there was a fatal error, in
 * which case the catalog must be immediately
 * disconnected.
 */
gboolean catalog_executequery(struct catalog *catalog,
                              const char *query,
                              catalog_callback_f callback,
                              void *userdata);

/**
 * Get the content of a source as a query result.
 * If the catalog has been interrupted some time before, th
 * query will return immediately. Use catalog_restart() to
 * recover from an interruption.
 *
 * @param catalog the catalog
 * @param source_id source ID
 * @param callback function to call for every results
 * @param userdata userdata to pass to the callback
 * @return return FALSE if there was an error
 */
gboolean catalog_get_source_content(struct catalog *catalog,
                                    int source_id,
                                    catalog_callback_f callback,
                                    void *userdata);

/**
 * Get the size of the content of a source.
 *
 * @param catalog the catalog
 * @param source_id source ID
 * @param count_out a non-null pointer on an unsigned
 * integer that will be set by the function if it returns true
 * @return FALSE if there was an error
 */
gboolean catalog_get_source_content_count(struct catalog *catalog,
                int source_id,
                unsigned int *count_out);

/**
 * Update the timestamp of the given entry, because it
 * has just been chosen by the user.
 *
 * Entries with the most up-to-date timestamp will
 * appear first.
 *
 * @param catalog
 * @param entry_id
 * @return true if updating worked
 */
gboolean catalog_update_entry_timestamp(struct catalog *catalog, int entry_id);

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
 * Add a source into the catalog.
 * @param catalog
 * @param type source type
 * @param id_out this variable will be set to the generated ID of the new source. must not be null.
 * @return TRUE if source was added, FALSE otherwise
 */
gboolean catalog_add_source(struct catalog *catalog, const char *type, int *id_out);

/**
 * Get rid of a source and of all of its entries
 * @param catalog
 * @param id source id
 */
gboolean catalog_remove_source(struct catalog *catalog, int source_id);

/**
 * Add an entry into the catalog.
 * @param catalog
 * @param path local path or URI
 * @param name short name for the entry (what will be searched)
 * @param long_name long name for the entry/description. For a file it's often a path.
 * @param source_id id of the source that is responsible for this entry
 * @param id_out if non-null, this variable will be set to the generated ID of the entry
 * @return TRUE if the entry was added, FALSE otherwise
 */
gboolean catalog_add_entry(struct catalog *catalog, int source_id, const char *path, const char *name, const char *long_name, int *id_out);

/**
 * Remove a stale entry from the catalog
 * @param catalog
 * @param source_id source
 * @param path local path or URI
 * @return TRUE if the entry was remove, FALSE otherwise
 */
gboolean catalog_remove_entry(struct catalog *catalog, int source_id, const char *path);

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
