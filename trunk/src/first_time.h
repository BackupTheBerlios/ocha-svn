#ifndef FIRST_TIME_H
#define FIRST_TIME_H

#include "catalog.h"

/** \file first time druid API
 * Setup the first time druid
 */
#include <glib.h>

/**
 * Run the first time druid
 * @return true if the druid has been run
 * successfully, false if the process was
 * cancelled by the user, or if there was
 * an error
 *
 * @param catalog open catalog (usually empty), will not be closed
 * @return true if the configuration was run successfully,
 * false otherwise
 */
gboolean first_time_run(struct catalog *catalog);


#endif /* FIRST_TIME_H */
