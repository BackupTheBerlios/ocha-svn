#ifndef QUERYWIN_H
#define QUERYWIN_H

#include "result_queue.h"
#include "queryrunner.h"

void querywin_init();
struct result_queue *querywin_get_result_queue(void);
void querywin_set_queryrunner(struct queryrunner *);
void querywin_start(void);
void querywin_stop(void);

#endif /*QUERYWIN_H*/
