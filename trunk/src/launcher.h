#ifndef LAUNCHER_H
#define LAUNCHER_H

/** \file define the launcher interface
 */
#include <glib.h>

/**
 * A launcher will execute some action given an
 * URI.
 *
 * What the URI might be and how it's going
 * to be executed depends on the launcher itself.
 *
 * Launchers are usually linked to sources; For
 * each source there's a launcher that will interpret
 * the URIs on the catalog.
 */
struct launcher
{
        /** launcher ID */
        const char *id;

        /**
         * Execute.
         *
         * @param name entry name
         * @param long_name long entry name
         * @param uri entry uri to launch
         * @param err a pointer to a variable that
         * will receive a launcher error or NULL if you
         * don't care
         * @return true if the entry is valid and execute
         * has been started as it should and it looks like
         * it's working so far. false otherwise (and see the
         * content of err)
         */
        gboolean (*execute)(struct launcher *self, const char *name, const char *long_name, const char *uri, GError **err);

        /**
         * Validate.
         *
         * @param uri
         * @param path entry path or uri
         * @return true if the entry is valid and execute
         * has a chance of working, false otherwise
         * @see result
         */
        gboolean (*validate)(struct launcher *self, const char *uri);
};

/**
 * Error domain name for launcher functions
 */
#define LAUNCHER_ERROR_DOMAIN_NAME "LAUNCHER"

/**
 * Error domain (quark) for launcher functions
 */
#define LAUNCHER_ERROR g_quark_from_static_string(LAUNCHER_ERROR_DOMAIN_NAME)

/**
 * Error codes in the launcher domain
 */
typedef enum
{
        /** Some internal launcher configuration is missing or makes no sense */
        LAUNCHER_INVALID_CONFIGURATION,
        /** URI is invalid (it has probably been deleted) */
        LAUNCHER_INVALID_URI,
        /** an error from some third-party module */
        LAUNCHER_EXTERNAL_ERROR
} LauncherIndexErrorCode;


/**
 * Execute an URI.
 *
 * @param name entry name
 * @param long_name long entry name
 * @param uri entry uri to launch
 * @param err a pointer to a variable that
 * will receive a launcher error or NULL if you
 * don't care
 * @return true if the entry is valid and execute
 * has been started as it should and it looks like
 * it's working so far. false otherwise (and see the
 * content of err)
 */
static inline gboolean launcher_execute(struct launcher *self,
                                        const char *name,
                                        const char *long_name,
                                        const char *uri,
                                        GError **err)
{
        g_return_val_if_fail(self, FALSE);
        g_return_val_if_fail(name, FALSE);
        g_return_val_if_fail(long_name, FALSE);
        g_return_val_if_fail(uri, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);
        g_return_val_if_fail(self->execute, FALSE);
        return self->execute(self, name, long_name, uri, err);
}

/**
 * Validate an URI.
 *
 * @param uri
 * @param path entry path or uri
 * @return true if the entry is valid and execute
 * has a chance of working, false otherwise
 * @see result
 */
static inline gboolean launcher_validate(struct launcher *self, const char *uri)
{
        g_return_val_if_fail(self, FALSE);
        g_return_val_if_fail(uri, FALSE);
        g_return_val_if_fail(self->validate, FALSE);
        return self->validate(self, uri);
}

#endif /* LAUNCHER_H */
