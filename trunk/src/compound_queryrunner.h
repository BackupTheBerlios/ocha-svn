#ifndef COMPOUND_QUERYRUNNER_H
#define COMPOUND_QUERYRUNNER_H

/** \file put several queryrunner together and make them
 * act like one.
 *
 */

#include "queryrunner.h"

/**
 * Create a compound query runner.
 *
 * Queries will be run on all query runners, consolidate will call all query runners and
 * release will release all query runners.
 *
 * @param queryrunners array of queryrunners (to be run in the same order they appear). array will be copied
 * @param queryrunners_len length of the array
 * @return a new queryrunner
 */
struct queryrunner *compound_queryrunner_new(struct queryrunner **queryrunners, int queryrunners_len);

#endif /*COMPOUND_QUERYRUNNER_H*/
