#ifndef RESULT_H
#define RESULT_H

#include <stdbool.h>
/** \file
 * A result is an executable object that
 * will be presented to the user as a result
 * of some query.
 *
 * struct result is an interface that can
 * be implemented by different query runners.
 */

/**
 * Result structure
 */
struct result
{
   /** User-friently name for this result. */
   const char *name;
   /** Full path to this result, which might be a file, an URL or NULL. */
   const char *path;

   /**
    * Function to be called to execute this result.
    *
    * This function should execute the result, whatever
    * that means for the current result, and return
    * true if it succeeded. Any error message
    * should have been presented to the user before
    * this function returns, depending on the
    * type of the result.
    *
    * If execution can take a long time, it must
    * be run in another thread or another process
    * so as not to block the caller's thread.
    *
    * @param self the result
    * @return true if execution succeeded
    */
   bool (*execute)(struct result *self);
};

#endif /* RESULT_H */
