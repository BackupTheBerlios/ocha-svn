#ifndef LOCATE_H
#define LOCATE_H
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

/** \file
 * The API though which the rest of the application
 * interacts with updatedb.
 */

/**
 * Locate process information.
 *
 * This represents the information publicly available
 * about the locate process. They can be used to
 * do a fcntl() or control the child process.
 * Change them at your own risks.
 */
struct locate
{
   /** fd of the pipe used to communicate wit the child process */
   int fd;
   /** PID of the child process */
   pid_t child_pid;
};

/**
 * Run a new locate child process.
 *
 * @param query the string to search for
 * @return a new locate structure or NULL
 */
struct locate *locate_new(const char *query);

/**
 * Get rid of a locate child process and everything associated with it.
 * @param locate data returned by locate_new()
 */
void locate_delete(struct locate *locate);

/**
 * Check whether more data is available on the pipe.
 * @param locate
 * @return true if there's more data to be read without blocking
 */
bool locate_has_more(struct locate *locate);

/**
 * Get the next file path from the pipe.
 *
 * This call will block if nothing is available
 * on the pipe.
 *
 * @param locate
 * @param dest gstring to append the file path to
 * @return true if there was something false otherwise (=> stream closed)
 */
bool locate_next(struct locate *locate, GString* dest);

#endif
