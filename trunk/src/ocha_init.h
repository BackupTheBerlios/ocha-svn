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
 * @param argc argc passed to main()
 * @param argv argv passed to main()
 * @param gui if true, the current main will open a window and expects GTK to be ready
 * @param config a pointer to a configuration structure to fill
 */
void ocha_init(int argc, char **argv, gboolean ui, struct configuration *config);

/**
 * Stop the program is no catalog file is available.
 * @param catalog_path catalog location
 */
void ocha_init_requires_catalog(const char *catalog_path);
#endif /*OCHA_INIT_H*/
