#ifndef INDEXER_H
#define INDEXER_H

/** \file catalog-based indexing plugins.
 *
 * Entries added into a catalog always come
 * from an indexer_source, which is linked
 * with an indexer. The system maitains
 * a dynamic collection of indexer that will
 * work on different kind of files or URIs.
 */

#include "catalog.h"

/**
 * An indexer maintains a collection of catalog entries
 * that it's then able to execute.
 *
 * Indexers have more than one indexer_source, usually linked
 * to one directory or hierarchy.
 */
struct indexer
{
   /**
    * Unique name for this indexer.
    * This name is used to find the indexer_sources that
    * are linked to the indexer. It is never displayed.
    */
   const char *name;

   /**
    * Load a indexer_source from the catalog.
    * @param indexer
    * @param catalog catalog to load the indexer_source from.
    * don't keep references to this catalog, because it'll
    * be closed just after this function returns
    * @param id unique id of the indexer_source
    * @return a indexer_source structure, even if the catalog
    * did not contain everything that was needed for
    * the indexer_source to be complete (it might be completed
    * later.
    */
   struct indexer_source *(*load_indexer_source)(struct indexer *self, struct catalog *catalog, int id);


   /**
    * Execute an entry added by a indexer_source of this indexer.
    *
    * @param indexer
    * @param name entry name
    * @param long_name long entry name
    * @param path entry path or uri
    * @param err if non-NULL, any indexing errors
    * will be added into this object iff this function
    * returns FALSE. The errors types and domain are
    * the same as those returned by the function
    * execute in the struct result
    * @return TRUE for success, FALSE for failure
    * @see result
    */
   gboolean (*execute)(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err);

   /**
    * Execute an entry added by a indexer_source of this indexer.
    *
    * @param name entry name
    * @param long_name long entry name
    * @param path entry path or uri
    * @return true if the entry is valid and execute
    * has a chance of working, false otherwise
    * @see result
    */
   gboolean (*validate)(struct indexer *, const char *name, const char *long_name, const char *path);

};

/**
 * A indexer_source is an instance of an indexer
 * that manages a subset of catalog entries.
 */
struct indexer_source
{
   /** unique indexer_source id */
   int id;

   /**
    * (re)-index the entries in this indexer_source.
    * @param dest catalog to add the entries into
    * @param err if non-NULL, any indexing errors
    * will be added into this object iff this function
    * returns FALSE
    * @return TRUE for success, FALSE for failure
    */
   gboolean (*index)(struct indexer_source *self, struct catalog *dest, GError **err);

   /**
    * Release the source structure.
    *
    * After this call, the source must not be used
    * any more.
    * @param source
    */
   void (*release)(struct indexer_source *source);
};

#define INDEXER_ERROR_DOMAIN_NAME "INDEXER"

/** Error codes */
typedef enum
   {
      /** error that comes directly or indirectly from the catalog */
      INDEXER_CATALOG_ERROR,
      /** some source configuration is missing or makes no sense */
      INDEXER_INVALID_CONFIGURATION,
      /** file passed to the function cannot be cataloged */
      INDEXER_INVALID_INPUT,
      /** an error from some third-party module */
      INDEXER_EXTERNAL_ERROR
   } CatalogIndexErrorCode;

#endif /*INDEXER_H*/