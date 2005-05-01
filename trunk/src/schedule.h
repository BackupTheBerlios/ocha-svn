#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "ocha_init.h"

/** \file Schedule index updates
 *
 * schedule_init() registers timeouts and event handler
 * into the gtk look so that automatic updates will be
 * run at the appropriate times.
 *
 * Everything in this module runs in the main thread, except
 * the re-indexing itself that runs in another process
 * entirely (using the command ocha_indexer)
 */

/**
 * This function, called at startup, will check the
 * configuration and schedule automatic updates.
 */
void schedule_init(struct configuration *config);

#endif /* SCHEDULE_H */
