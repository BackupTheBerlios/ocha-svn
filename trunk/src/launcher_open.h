#ifndef LAUNCHER_OPEN_H
#define LAUNCHER_OPEN_H

/** \file Implementation of the launcher 'open'
 * that will open a file using GNOME - which
 * usually mean use the default application
 * defined for the file's mimetype.
 */
#include "launcher.h"

/**
 * The launcher itself
 */
struct launcher launcher_open;

/**
 * The launcher ID
 */
#define LAUNCHER_OPEN_ID "open"

#endif /* LAUNCHER_OPEN_H */
