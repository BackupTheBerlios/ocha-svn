#ifndef NETWM_QUERYRUNNER_H
#define NETWM_QUERYRUNNER_H

/** \file a query runner for windows.
 *
 * This query runner will lookup the window list for a NetWM-compliant
 * window manager and apply the query string on the window titles.
 *
 * Executing a result of this query runner means activating the
 * window. The result path will be of the form "x-win:0x<window address>",
 * which makes it fine to use as a key to compare results, but not to
 * display to the user.
 */

#include "queryrunner.h"
#include "result_queue.h"
#include <X11/Xlib.h>

/**
 * Create a query runner for the given display
 */
struct queryrunner *netwm_queryrunner_create(Display *display, struct result_queue *queue);

#endif /*NETWM_QUERYRUNNER_H*/
