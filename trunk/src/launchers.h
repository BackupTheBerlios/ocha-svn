#ifndef LAUNCHERS_H
#define LAUNCHERS_H

/** \file keep track of launchers available on the system.
 */
#include "launcher.h"

/**
 * Return a NULL-terminated list of launchers available on
 * the system.
 *
 * This will always return the same list, once the launchers
 * have been looked up. The array will be available as long
 * as the process is running. Don't ever modify it.
 *
 * @return a null-terminated array of pointers to launchers,
 * valid until the process dies
 */
struct launcher **launchers_list(void);

/**
 * Get a launcher, given its ID.
 *
 * @param launcher ID
 * @return a launcher, if there is on with the given ID, or null
 */
struct launcher *launchers_get(const char *id);

#endif /*LAUNCHERS_H*/
