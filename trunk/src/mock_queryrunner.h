#ifndef MOCK_QUERYRUNNER_H
#define MOCK_QUERYRUNNER_H

#include "queryrunner.h"
#include "result_queue.h"

/** \file Mock implementation of a queryrunner.
 *
 * This queryrunner will always find 5 results of everything
 * and then 5 more when consolidating.
 */

/**
 * Create a new mock queryrunner.
 * @param queue result queue to add the results to
 * @return queryrunner
 */
struct queryrunner *mock_queryrunner_new(struct result_queue *queue);

#endif /*MOCK_QUERYRUNNER_H*/
