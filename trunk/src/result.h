#ifndef RESULT_H
#define RESULT_H

#include <glib.h>

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
        /** User-friently name for this result (UTF-8). */
        const char *name;
        /** Long user-friendly name for this result (UTF-8). */
        const char *long_name;

        /** An URI that uniquely identifies the result (UTF-8). */
        const char *path;

        /**
         * Execute this result.
         *
         * This function should execute the result, whatever
         * that means for the current result, and return
         * TRUE if it succeeded. Any error message
         * should have been presented to the user before
         * this function returns, depending on the
         * type of the result.
         *
         * If execution can take a long time, it must
         * be run in another thread or another process
         * so as not to block the caller's thread.
         *
         * @param self the result
         * @param errors a GError to set when the function
         * returns FALSE, may be NULL if you don't care.
         * The domain will be RESULT_ERROR and the error
         * code one of the codes defined by ResultErrorCode.
         * (to be freed with g_error_free())
         * @return TRUE if execution succeeded
         */
        gboolean (*execute)(struct result *self, GError **errors);

        /**
         * Make sure the result is still valid.
         *
         * If a result is kept for more than the duration
         * of a query, it could have become invalid. This
         * function checks whether it's the case.
         *
         * Executing an invalid result will result in an
         * error. Executing a valid result might result in
         * an error.
         *
         * @return FALSE if the result is invalid
         */
        gboolean (*validate)(struct result *self);

        /**
         * Free all memory and resources held by this result
         * @param self the result
         */
        void (*release)(struct result *self);
};

/** Result error quark */
#define RESULT_ERROR g_quark_from_static_string("RESULT_ERROR")

/** Result error codes */
typedef enum
{
        /** Such an error meant that the result is invalid and should be discarded */
        RESULT_ERROR_INVALID_RESULT,
        /** Such an error meant that some resource was missing, but the result is generally valid */
        RESULT_ERROR_MISSING_RESOURCE,
        /** Generic error code, use it only when nothing else matches */
        RESULT_ERROR_FAILED
} ResultErrorCode;

#endif /* RESULT_H */
