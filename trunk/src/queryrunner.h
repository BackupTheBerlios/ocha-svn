#ifndef QUERYRUNNER_H
#define QUERYRUNNER_H

/** \file
 * A query runner runs a query (a string typed by the user)
 * and sends results into a result queue.
 *
 * This header defines a common interface for queryrunners.
 * There might be several implementation that can be
 * created in different ways.
 */

/**
 * This is the public part of a query runner.
 *
 * A query runner is always used as follow:
 * <ol>
 * <li>call start() to tell the runner to get ready (make connections, open files...)</li>
 * <li>call run_quey() several times, with different queries</li>
 * <li>sometimes call consolidate() to get more results, if the user is waiting</li>
 * <li>call stop() once the user has chosen or abandoned the query (close connections, close files...)</li>
 * <li>go back to the first point or call release()</li>
 * </ol>
 */
struct queryrunner
{
        /**
         * Start running query.
         */
        void (*start)(struct queryrunner *self);
        /**
         * Set or reset the query to be run.
         * @param query the query
         */
        void (*run_query)(struct queryrunner *self, const char *query);

        /**
         * Tell the query runner that it's free to
         * run the more expensive search to consolidate
         * results; the user is waiting.
         */
        void (*consolidate)(struct queryrunner *self);

        /**
         * Stop the current query, if there is one
         */
        void (*stop)(struct queryrunner *self);

        /**
         * Release all data held by the query runner, including
         * the query runner structure itself.
         */
        void (*release)(struct queryrunner *self);
};


#endif /*QUERYRUNNER_H*/
