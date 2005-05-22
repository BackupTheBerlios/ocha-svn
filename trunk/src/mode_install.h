#ifndef MODE_INSTALL_H
#define MODE_INSTALL_H

/** \file Start ocha in 'install' mode.
 */

#include "ocha_init.h"

/**
 * Start ocha in install mode
 * @param arg arg count
 * @param argv arguments
 * @return main() return value
 */
int mode_install(int argc, char *argv[]);

/**
 * Check whether a first-time installation is
 * necessary and run it.
 *
 * If the function decides a first-time installation
 * is necessary, this function never returns.
 *
 * @param config configuration data
 */
void mode_install_if_necessary(struct configuration *config);

#endif /* MODE_INSTALL_H */
