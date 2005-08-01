#ifndef CATALOG_H
#define CATALOG_H

#include "result.h"
#include "result_queue.h"
#include <string.h>

/** \file interface to an sqllite-based catalog
 */

/** error codes returned by catalog connect */
typedef enum
{
        CATALOG_CONNECTION_ERROR
} CatalogErrorCodes;

/** get the error quark for the errors returned by catalog */
GQuark catalog_error_quark(void);


/**
 * Information about an entry of the catalog
 */
struct catalog_entry
{
        /** short name */
        char *name;

        /** long name (description) */
        const char *long_name;

        /** path|uri */
        const char *path;

        /** launcher */
        const char *launcher;

        /** owner source ID */
        int source_id;
};

/**
 * Data passed to the query callback
 */
struct catalog_query_result
{
        /** entry id */
        int id;

        /** entry values */
        struct catalog_entry entry;

        /** result pertinence */
        float pertinence;

        /** query id */
        QueryId query_id;

        /**
         * TRUE if the entry is enabled.
         * Only enabled entries are sent by run_query, so
         * this field is always TRUE, then. When getting
         * the source content, on the other hand, this
         * field is meaningful.
         */
        gboolean enabled;
};

/**
 * Set sane default values for a catalog entry.
 *
 * Example:
 *
 * struct catalog_entry entry;
 * CATALOG_ENTRY_INIT(&entry)
 */
#define CATALOG_ENTRY_INIT(entry) memset((entry), 0, sizeof(struct catalog_entry))

/**
 * Create a new catalog structure and make a connection to an sqlite catalog,
 * creating it if necessary.
 *
 * This is equivalent to catalog_new() followed by catalog_connect().
 *
 * @param path path to the catalog file
 * @param if non-null and if this function returns
 * NULL, you'll find there an error message and
 * code explaining why connecting to the database
 * failed (to be freed with g_error_free())
 * @return NULL if the catalog could not be open/created
 */
struct catalog *catalog_new_and_connect(const char *path, GError **errs);

/**
 * Create a new catalog structure, linked to a catalog path, but
 * do not connect to it.
 *
 * Call catalog_connect() to connect to the catalog and make
 * this catalog usable.
 * @return a new catalog structure (never null)
 */
struct catalog *catalog_new(const char *path);

/**
 * (Re-)connect to the catalog, create it if necessary.
 *
 * @return FALSE if the catalog could not be open/created. Check
 * the error with catalog_error()
 */
gboolean catalog_connect(struct catalog *);

/**
 * Disconnect from the catalog.
 *
 * This ONLY disconnects the catalog. It will not
 * free the structure. Use catalog_free() if you want
 * to get rid of the catalog structure.
 * @param catalog the catalog
 */
void catalog_disconnect(struct catalog *catalog);

/**
 * Free any resources held by the catalog. Disconnect it
 * if necessary.
 *
 * Results created by this catalog are NOT freed
 * automatically.
 * @param catalog catalog returned by catalog_connect()
 */
void catalog_free(struct catalog *catalog);

/**
 * Check whether the catalog is currently connected.
 *
 * @return TRUE if a connection is open, FALSE if there's
 * no connections
 */
gboolean catalog_is_connected(struct catalog *catalog);

/**
 * Receive the results from catalog_executequery().
 *
 * @param catalog
 * @param pertinence pertinence of the result, between 0.0 and 1.0
 * @param entry the entry structure, read-only, fully initialized
 * @param userdata pointer passed to catalog_executequery()
 * @return TRUE to continue adding results, FALSE to stop looking for results
 */
typedef gboolean (*catalog_callback_f)(struct catalog *catalog,
                                       const struct catalog_query_result *result,
                                       void *userdata);

/**
 * Get the 'last indexed' timestamp.
 *
 * The timestamp is not updated automatically; update
 * is done using catalog_timestamp_update()
 *
 * This method will always return 0 while the catalog
 * is disconnected.
 *
 * @param catalog
 * @return timestamp, 0 if it was never set
 */
gulong catalog_timestamp_get(struct catalog *catalog);

/**
 * Update the 'last indexed' timestamp.
 *
 * The current date (# of seconds since the UNIX epoch)
 * is used.
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @return return FALSE if there was an error
 */
gboolean catalog_timestamp_update(struct catalog *gcatalog);

/**
 * Execute a query and add the results into
 * the query runner.
 *
 * If the catalog has been interrupted some time before, th
 * query will return immediately. Use catalog_restart() to
 * recover from an interruption.
 *
 * This method will always fail while the catalog
 * is disconnected.
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
 * This method will always fail while the catalog
 * is disconnected.
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
 * This method will always fail while the catalog
 * is disconnected.
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
 * This method will always fail while the catalog
 * is disconnected.
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
 * This function does nothing if the catalog is disconnected. It's
 * safe to call it at any time, though, whether it's connected or not.
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
 * This function does nothing if the catalog is disconnected. It's
 * safe to call it at any time, though, whether it's connected or not.
 *
 * @param catalog the catalog
 */
void catalog_restart(struct catalog *catalog);

/**
 * Add a source into the catalog.
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param type source type
 * @param id_out this variable will be set to the generated ID of the new source. must not be null.
 * @return TRUE if source was added, FALSE otherwise
 */
gboolean catalog_add_source(struct catalog *catalog, const char *type, int *id_out);

/**
 * Make sure a source with this ID exists, and that it's of the correct type.
 *
 * If the source exists with this type, it'll be left alone.
 * If the source exists but is of the wrong type, its content will be removed
 * and the source re-created. If the source does not exist, it'll be created.
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param type source type
 * @param source_id source id
 * @return TRUE if the source now exists and is of this type, FALSE if some
 * error was detected (check catalog_error())
 */
gboolean catalog_check_source(struct catalog *catalog, const char *type, int source_id);

/**
 * Get rid of a source and of all of its entries
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param id source id
 */
gboolean catalog_remove_source(struct catalog *catalog, int source_id);

/**
 * Add an entry into the catalog or refresh/confirm it if it already exists.
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param entry the entry to add
 * @param id_out if non-null, this variable will be set to the generated ID of the entry
 * @return TRUE if the entry was added, FALSE otherwise
 */
gboolean catalog_add_entry(struct catalog *catalog, const struct catalog_entry *entry, int *id_out);

/**
 * Start updating a source entries.
 *
 * To update source entries, make calls to catalog_add_entry(), even
 * for existing entries. When you later call catalog_end_source_update(),
 * the entries that existed before catalog_begin_source_update() that
 * haven't updated will be removed.
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param source_id
 * @return TRUE if the update can start, FALSE otherwise
 */
gboolean catalog_begin_source_update(struct catalog *catalog, int source_id);

/**
 * Declare a source update as done, remove stale entries.
 *
 * This function marks a source update as finished and then
 * gets rid of all the entries that have been added before
 * the call to catalog_begin_source_update() that haven't
 * been updated using catalog_add_entry()
 *
 * See also catalog_start_source_update()
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param source_id
 * @return TRUE if all has been deleted successfully, FALSE otherwise
 */
gboolean catalog_end_source_update(struct catalog *catalog, int source_id);

/**
 * Remove a stale entry from the catalog
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param source_id source
 * @param path local path or URI
 * @return TRUE if the entry was remove, FALSE otherwise
 */
gboolean catalog_remove_entry(struct catalog *catalog, int source_id, const char *path);

/**
 * Enable/disable an entry.
 *
 * Disabled entries will not be returned when a query is run. They'll
 * kept on the database and will be returned when getting the source
 * content.
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param entry_id ID of the entry, usally from a query on source content
 * @param enable TRUE => enable, FALSE=> disable
 * @return TRUE if modification was done, FALSE otherwise (check error)
 */
gboolean catalog_entry_set_enabled(struct catalog *catalog, int entry_id, gboolean enabled);


/**
 * Enable/disable a source
 *
 * Entries from disabled sources will not be returned when a query is run. They'll
 * kept on the database and will be returned when getting the source
 * content.
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param source_id ID of the source
 * @param enable TRUE => enable, FALSE=> disable
 * @return TRUE if modification was done, FALSE otherwise (check error)
 */
gboolean catalog_source_set_enabled(struct catalog *catalog, int source_id, gboolean enabled);

/**
 * Check whether a source is enabled.
 *
 * This method will always fail while the catalog
 * is disconnected.
 *
 * @param catalog
 * @param source_id
 * @param enabled_out set to TRUE if it is enable, FALSE otherwise (if return value is TRUE)
 * @return TRUE if query worked, FALSE otherwise (check error)
 */
gboolean catalog_source_get_enabled(struct catalog *catalog, int source_id, gboolean *enabled_out);

/**
 * Get a pointer on the last error that happened with
 * this catalog.
 *
 * @param catalog catalog to query
 * @return a pointer to the last error message, never NULL. this
 * pointer is only valid until the next call to a catalog_ function
 * on the same catalog
 */
const char *catalog_error(struct catalog *catalog);

#endif /* CATALOG_H */
