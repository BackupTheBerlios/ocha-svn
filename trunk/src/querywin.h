#ifndef QUERYWIN_H
#define QUERYWIN_H

#include "result_queue.h"
#include "queryrunner.h"

/** \file
 * This module open/hides a window that listens for
 * user keypresses and runs queries. Queries are
 * run using the query runner passed to the
 * module using querywin_set_queryrunner()
 */

/**
 * Initialize the structure necessary for querywin to work.
 * Call this before any other functions in this module.
 */
void querywin_init(void);

/**
 * Get the result queue created by querywin_init()
 */
struct result_queue *querywin_get_result_queue(void);

/**
 * Set the query runner to be used by the window.
 *
 * There can be only one query runner. It must be
 * set before calling querywin_start()
 */
void querywin_set_queryrunner(struct queryrunner *);

/**
 * (Re-)open the window, listen for keypresses.
 *
 * Make sure you've called querywin_init() and querywin_set_queryrunner()
 * before making this call.
 */
void querywin_start(void);

/**
 * Stop the query and hide the window
 */
void querywin_stop(void);

#endif /*QUERYWIN_H*/
