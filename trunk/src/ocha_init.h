#ifndef OCHA_INIT_H
#define OCHA_INIT_H

/** \file Generic initialization, usually called from main ()
 */
#include <glib.h>

/**
 * Structure initialized by ocha_init.
 */
struct configuration
{
        /** absolute path to the catalog file, which may not exist (call g_free() when you're done) */
        const char *catalog_path;
};


/**
 * Generic initialization.
 *
 * @param program name of the current program (ocha, ocha_indexer, ocha_preferences, ...)
 * @param argc argc passed to main()
 * @param argv argv passed to main()
 * @param gui if true, the current main will open a window and expects GTK to be ready
 * @param config a pointer to a configuration structure to fill
 */
void ocha_init(const char *program, int argc, char **argv, gboolean ui, struct configuration *config);

/**
 * Stop the program is no catalog file is available.
 * @param catalog_path catalog location
 */
void ocha_init_requires_catalog(const char *catalog_path);

/**
 * Kill another instance of ocha and remove it from
 * gnome-session.
 * @return true if another instance was found and
 * killed
 */
gboolean ocha_init_kill(void);

/**
 * Check whether another instance of ocha is running.
 * @return true if another instance is runninng
 */
gboolean ocha_init_ocha_is_running(void);

/**
 * Write the current PID so that this instance of
 * ocha is registered as being 'the' ocha.
 *
 * This function also registers the correct handlers
 * and gtk sources for answering kill and ping requests
 * from other processes. This supposes that ocha will
 * eventually run a gtk loop.
 *
 * @return true if it could be done, false
 * if another instance was found or if some other
 * error prevented the communication channels from
 * being setup (error has been displayed to stderr)
 */
gboolean ocha_init_create_socket(void);

/**
 * Argument list for running 'ocha index' (null-terminated).
 *
 * The size of this array is to be found
 * in ocha_init_indexer_argc
 * @see ocha_init_indexer_argc
 */
extern gchar *ocha_init_indexer_argv[];

/**
 * Size of ocha_init_indexer_argv
 * @see ocha_init_indexer_argv
 */
extern gint ocha_init_indexer_argc;

/**
 * Argument list for running 'ocha install' (null-terminated).
 *
 * The size of this array is to be found
 * in ocha_init_install_argc
 * @see ocha_init_install_argc
 */
extern gchar *ocha_init_install_argv[];

/**
 * Size of ocha_init_install_argv
 * @see ocha_init_install_argv
 */
extern gint ocha_init_install_argc;

/**
 * Argument list for running 'ocha preferences' (null-terminated).
 *
 * The size of this array is to be found
 * in ocha_init_preferences_argc
 * @see ocha_init_preferences_argc
 */
extern gchar *ocha_init_preferences_argv[];

/**
 * Size of ocha_init_preferences_argv
 * @see ocha_init_preferences_argv
 */
extern gint ocha_init_preferences_argc;

/**
 * Argument list for running 'ochad' (null-terminated).
 *
 * The size of this array is to be found
 * in ocha_init_daemon_argc
 * @see ocha_init_daemon_argc
 */
extern gchar *ocha_init_daemon_argv[];

/**
 * Size of ocha_init_daemon_argv
 * @see ocha_init_daemon_argv
 */
extern gint ocha_init_daemon_argc;


#endif /*OCHA_INIT_H*/
