#ifndef RESTART_H
#define RESTART_H

/** \file Register ocha to gnome-session for automatic restart
 *
 */
#include <glib.h>

/**
 * Register ocha to gnome-session.
 *
 * @param startup_command command that was used to start
 * ocha (argv[0])
 * @return true if ocha could be registered or was already
 * registered. false usually means there's no gnome-session
 */
gboolean restart_register(const char *startup_command);

/**
 * Remove ocha from gnome-session.
 *
 * Make sure gnome-session won't try and restart ocha
 * next time.
 *
 * This function will make sure gtk_main_quit() will be
 * called at the appropriate time. That might not be immediately.
 *
 * @return true if ocha could be unregistered. false usually
 * means there's no gnome-session. Even if it returned
 * false, gtk_main_quit() has been or will be called.
 */
gboolean restart_unregister_and_quit_when_done(void);

#endif /* RESTART_H */
