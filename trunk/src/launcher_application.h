#ifndef LAUNCHER_APPLICATION_H
#define LAUNCHER_APPLICATION_H

/** \file Launch an application given a .desktop file
 * of the correct type.
 */
#include "launcher.h"

/**
 * The launcher itself
 */
struct launcher launcher_application;

/**
 * The launcher ID
 */
#define LAUNCHER_APPLICATION_ID "application"

/**
 * Do a thorough evaluation of a .desktop file to see whether
 * the launcher could use it.
 */
gboolean launcher_application_is_usable(const char *uri);

#endif /* LAUNCHER_APPLICATION_H */
