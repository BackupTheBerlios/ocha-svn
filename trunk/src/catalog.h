#ifndef CATALOG_H
#define CATALOG_H

#include <stdbool.h>
#include "target.h"

/**
 * This function receives search results.
 *
 * Search results and sent to this callback whenever
 * they come or whenever the confidence changes.
 * 
 * Search results can come in any order. The same
 * result may (will) be sent more than once if the
 * confidence increases or decreases. If a catalog
 * notices that a target is totally wrong for the
 * search ats it is now, it should call this function
 * with a confidence of 0. 
 *
 * A confidence of 1.0 should be reserved for exact
 * matches.
 *
 * @param confidence search result confience 0.0 <= confidence <= 1.0
 * @param target a target structure. a reference is
 * kept on the target until this function returns
 */
typedef void (*catalog_search_callback_t)(struct catalog_search *, float confidence, struct target *target);


/**
 * Searchable target index.
 *
 * A catalog keeps an index of targets (usually files)
 * that can be searched on.
 *
 * Catalogs must support being used by more than
 * one thread at a time; there can be several
 * search runnig on paraller threads while
 * an update is running on another. Searches, on
 * the other hand, should only be used by 
 * one thread at a time.
 */
struct catalog 
{

   /**
	* Create a new catalog search.
	* 
	* This function creates a catalog search, fill in the structure
	* and returns true. If anything goes wrong, it returns false and
	* the catalog 
	*
	* Catalog search should be released by calling catalog_search->delete(catalog_search)
	*
	* @param cb callback to send the results to
	* @return new catalog search or NULL if something went wrong
	*/
   struct catalog_search (*new_search)(struct catalog *, catalog_search_callback_t cb);
   
   /**
	* Update the catalog.
	* 
	* This operation can take a very long time
	* and should not be interrupted. Let it run on its own 
	* low-priority thread. 
	*/
   void (*catalog_update_t)(struct catalog *);
};


/**
 * A search on a catalog.
 *
 * A search starts empty and the query is set
 * piece by piece. It's not possible to reset 
 * the query; create a new search instad.
 *
 * Only one thread should run a search at a
 * time.
 */
struct catalog_search
{
   /**
	* Get the query as it is so far.
	*
	* @param memory to write the query to. if it's null
	* nothing will be done
	* @param len size of dest if it's too small
	* for the query to fit, it is truncated. The
	* last bit is always set to 0
	* @return the real length of the query, always.
	*/
   const int (*get_query)(const struct catalog_search *, char *dest, int len);

   /**
	* Append the following string to the query.
	* @param search 
	* @param str string to append to the current query
	* @return true if appending was really done (always true
	* unless the catalog is frozen, in which case it's always false)
	*/
   bool (*append)(struct catalog_search *search, const char *str);

   /**
	* Freeze the search.
	*
	* The most expensive searches won't be executed until
	* the search is frozen. 
	*/
   void (*freeze)(struct catalog_search *search);

   /**
	* Stop the search and release the resource.
	* 
	* This destroys the search object.
	*/
   void (*delete)(struct catalog_search *);
};

#endif
