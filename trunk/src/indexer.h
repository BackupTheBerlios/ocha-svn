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
#include <gtk/gtk.h>

struct indexer;

/**
 * Function called by new_source just after the new source
 * has been created.
 *
 * The source must be valid and ready to be indexed.
 *
 * @param indexer
 * @param source id
 * @param userdata
 */
typedef void (*indexer_new_source_cb)(struct indexer *, int source_id, gpointer userdata);

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
         * Name of this indexer to be displayed to the users
         */
        const char *display_name;

        /**
         * A long multiline description for the
         * indexer, in UTF8 with pango markup.
         * This description will be displayed on
         * the preference screen when the user selects
         * the indexer (as opposed to selecting a
         * source inside the indexer)
         */
        const char *description;

        /**
         * Examine the current environment and try to figure out
         * what to index.
         *
         * This function is called on all indexers the first time
         * ocha is started, with an empty catalog. The goal is to
         * look at directories, user configuration files and
         * the environment and create the sources that would be
         * the most appropriate in this environment and add
         * them into the catalog.
         *
         * The new sources should be ready to be indexed, but
         * they should still be empty when this call ends.
         *
         * @param catalog catalog to add new sources into
         * @return FALSE if something went wrong accessing
         * the catalog. The error will be found in the catalog
         * itself. Everything else should not be considered
         * an error; the sources whose creation failed for
         * some other reason should simply be skipped. They
         * probably were not appropriate.
         */
        gboolean (*discover)(struct indexer *, struct catalog *catalog);

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
        struct indexer_source *(*load_source)(struct indexer *self, struct catalog *catalog, int id);

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

        /**
         * Create a new source.
         *
         * Some indexers will not allow the creation of
         * new sources, in which case this function may
         * be null.
         *
         * @param indexer
         * @param catalog catalog to register the source in
         * @param err error structure (may be null)
         * @return a new, uninitialized source, to be freed the usual way or
         * NULL if there was an error
         */
        struct indexer_source *(*new_source)(struct indexer *, struct catalog *catalog, GError **err);

        /**
         * Create an indexer view.
         *
         * The view can display any source. By default, it
         * displays no source and is disabled.
         *
         * @param indexer
         * @see indexer_source_view
         */
        struct indexer_source_view *(*new_view)(struct indexer *);
};

/**
 * View of an indexer source.
 *
 * The view can display more than one source.
 * The source is set using attach and unset using
 * detach. This structure is not freed automatically
 * but if the widget is destroyed it becomes unusable.
 */
struct indexer_source_view
{
        struct indexer *indexer;

        /**
         * The view's widget. One per view
         * Never change this directly
         */
        GtkWidget *widget;

        /**
         * Source displayed by the view;
         * updated by attach() and detach.
         * Never change this directly
         */
        int source_id;

        /**
         * Set the source displayed by the view
         */
        void (*attach)(struct indexer_source_view *view, struct indexer_source *source);

        /**
         * Unset the source displayed by the view.
         * After this call, the source displays no view
         * and is disabled, just like after it's just
         * been created.
         */
        void (*detach)(struct indexer_source_view *view);

        /**
         * Free all memory used by the view and unreference
         * the widget
         */
        void (*release)(struct indexer_source_view *view);
};

/**
 * Set/Change the source displayed by the view.
 * @param view
 * @param source source to attach to. The source can be released after this call has been made.
 */
static inline void indexer_source_view_attach(struct indexer_source_view *view, struct indexer_source *source)
{
        g_return_if_fail(view!=NULL);
        view->attach(view, source);
}

/**
 * Unset the source displayed by the view.
 * After this call, the source displays no view
 * and is disabled, just like after it's just
 * been created.
 */
static inline void indexer_source_view_detach(struct indexer_source_view *view)
{
        g_return_if_fail(view!=NULL);
        view->detach(view);
}

/**
 * Free all memory used by the view and unreference
 * the widget
 */
static inline void indexer_source_view_release(struct indexer_source_view *view)
{
        g_return_if_fail(view!=NULL);
        view->release(view);
}

/**
 * Examine the current environment and try to figure out
 * what to index.
 *
 * This function is called on all indexers the first time
 * ocha is started, with an empty catalog. The goal is to
 * look at directories, user configuration files and
 * the environment and create the sources that would be
 * the most appropriate in this environment and add
 * them into the catalog.
 *
 * The new sources should be ready to be indexed, but
 * they should still be empty when this call ends.
 *
 * This is a shortcut for indexer->discover()
 *
 * @param catalog catalog to add new sources into
 * @return FALSE if something went wrong accessing
 * the catalog. The error will be found in the catalog
 * itself. Everything else should not be considered
 * an error; the sources whose creation failed for
 * some other reason should simply be skipped. They
 * probably were not appropriate.
 */
static inline gboolean indexer_discover(struct indexer *indexer, struct catalog *catalog)
{
        g_return_val_if_fail(indexer, FALSE);
        return indexer->discover(indexer, catalog);
}

/**
 * Load a indexer_source from the catalog.
 *
 * This is a shortcut for indexer->load_source()
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
static inline struct indexer_source *indexer_load_source(struct indexer *self, struct catalog *catalog, int id)
{
        g_return_val_if_fail(self, NULL);
        return self->load_source(self, catalog, id);
}

/**
 * Execute an entry added by a indexer_source of this indexer.
 *
 * This is a shortcut for indexer->execute()
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
static inline gboolean indexer_execute(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err)
{
        g_return_val_if_fail(self, FALSE);
        return self->execute(self, name, long_name, path, err);
}

/**
 * Execute an entry added by a indexer_source of this indexer.
 *
 * This is a shortcut for indexer->validate()
 * @param name entry name
 * @param long_name long entry name
 * @param path entry path or uri
 * @return true if the entry is valid and execute
 * has a chance of working, false otherwise
 * @see result
 */
static inline gboolean indexer_validate(struct indexer *self , const char *name, const char *long_name, const char *path)
{
        g_return_val_if_fail(self, FALSE);
        return self->validate(self, name, long_name, path);
}

/**
 * Create a new source.
 *
 * Some indexers will not allow the creation of
 * new sources, in which case this function may
 * be null.
 *
 * This is a shortcut for indexer->new_source()
 * @param indexer
 * @param catalog catalog to register the source in
 * @param err error structure (may be null)
 * @return a new, uninitialized source, to be freed the usual way or
 * NULL if there was an error
 */
static inline struct indexer_source *indexer_new_source(struct indexer *self, struct catalog *catalog, GError **err)
{
        g_return_val_if_fail(self, NULL);
        return self->new_source(self, catalog, err);
}

/**
 * Create an indexer view.
 *
 * The view can display any source. By default, it
 * displays no source and is disabled.
 *
 * @param indexer
 * @see indexer_source_view
 */
static inline struct indexer_source_view *indexer_new_view(struct indexer *self)
{
        g_return_val_if_fail(self, NULL);
        return self->new_view(self);
}

/**
 * Notify function for whatching indexer sources
 * @param source source that has changed
 * @param userdata
 */
typedef void (*indexer_source_notify_f)(struct indexer_source * source, gpointer userdata);

/**
 * A indexer_source is an instance of an indexer
 * that manages a subset of catalog entries.
 */
struct indexer_source
{
        /** unique indexer_source id */
        int id;

        /** Name to be displayed to the user */
        const char *display_name;

        /** the indexer that created the source */
        struct indexer *indexer;

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
         * Get told when the display name has changed.
         *
         * Notifications outlive the struct indexer_source *. They'll
         * last as long as the object exists in the configuration.
         *
         * @param source
         * @param catalog a catalog that will exist for as long
         * as the notification or the source hasn't been removed
         * @param notify_func function to call when the name has changed
         * @param userdata userdata to pass to the function
         * @return an id to use to remove the notification request
         */
        guint (*notify_display_name_change)(struct indexer_source *, struct catalog *, indexer_source_notify_f, gpointer);

        /**
         * Remove a notification request added with notify_display_name_change
         * @param source
         * @param id number returned by notify_display_name_change
         */
        void (*remove_notification)(struct indexer_source *source, guint id);

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

/**
 * (Re)-index the entries in this indexer_source.
 *
 * This is a shortcut for source->index(source, dest, err)
 *
 * @param dest catalog to add the entries into
 * @param err if non-NULL, any indexing errors
 * will be added into this object iff this function
 * returns FALSE
 * @return TRUE for success, FALSE for failure
 */
static inline gboolean indexer_source_index(struct indexer_source *self, struct catalog *dest, GError **err)
{
        g_return_val_if_fail(self, FALSE);
        return self->index(self, dest, err);
}

/**
 * Release the source structure.
 *
 * After this call, the source must not be used
 * any more.
 *
 * This is a shortcut for source->editor_widget(source)
 *
 * @param source
 */
static inline void indexer_source_release(struct indexer_source *source)
{
        g_return_if_fail(source);
        source->release(source);
}

/**
 * Get told when the display name has changed.
 *
 * Notifications outlive the struct indexer_source *. They'll
 * last as long as the object exists in the configuration.
 *
 * This is a shortcut for source->notify_display_name_change(source)
 * @param source
 * @param catalog a catalog that will exist for as long
 * as the notification or the source hasn't been removed
 * @param notify_func function to call when the name has changed
 * @param userdata userdata to pass to the function
 * @return an id to use to remove the notification request
 */
static inline guint indexer_source_notify_display_name_change(struct indexer_source *source,
                struct catalog *catalog,
                indexer_source_notify_f notify,
                gpointer userdata)
{
        g_return_val_if_fail(source, 0);
        return source->notify_display_name_change(source,
                        catalog,
                        notify,
                        userdata);
}

/**
 * Remove a notification request added with notify_display_name_change
 * @param source
 * @param id number returned by notify_display_name_change
 */
static inline void indexer_source_remove_notification(struct indexer_source *source, guint id)
{
        g_return_if_fail(source);
        source->remove_notification(source, id);
}

/**
 * Destroy everything that forms the source from
 * the database and the configuration and release it.
 *
 * @param source the source to destroy
 * @param catalog catalog to remove the source from
 * @return true if everything was destroyed, false
 * otherwise. whatever happens, indexer_source_release
 * will have been called on the source object passed
 * to this function
 */
gboolean indexer_source_destroy(struct indexer_source *source, struct catalog *);

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
