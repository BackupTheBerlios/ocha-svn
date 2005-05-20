#include "restart.h"
#include <libgnome/gnome-init.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/libgnomeui.h>

/** \file implement API defined in restart.h
 *
 */


/* ------------------------- prototypes: static functions */

/* ------------------------- definitions */

/* ------------------------- public functions */
gboolean restart_register(const char *startup_command)
{
        GnomeClient *client = gnome_master_client();
        GnomeClientFlags flags;

        if(client==NULL) {
                /* I can't do anything */
                return FALSE;
        }
        flags = gnome_client_get_flags(client);

        if( (flags&(GNOME_CLIENT_RESTARTED|GNOME_CLIENT_RESTORED)) == 0 ) {
                gchar *restart;
                if(g_path_is_absolute(startup_command)) {
                        restart = g_strdup(startup_command);
                } else {
                        gchar *curdir = g_get_current_dir();
                        restart = g_strdup_printf("%s/%s",
                                                  curdir,
                                                  startup_command);
                        g_free(curdir);
                }

                gnome_client_set_restart_style(client,
                                               GNOME_RESTART_IMMEDIATELY);
                gnome_client_set_restart_command(client, 1, &restart);
                gnome_client_request_save (client,
                                           GNOME_SAVE_LOCAL,
                                           FALSE/*not shutdown*/,
                                           GNOME_INTERACT_NONE,
                                           TRUE/*fast*/,
                                           FALSE/*not global*/);

                g_free(restart);
        }
        return TRUE;
}

gboolean restart_unregister_and_quit_when_done(void)
{
        GnomeClient *client = gnome_master_client();

        printf("quitting...\n");
        if(client==NULL) {
                /* I can't do anything */
                gtk_main_quit();
                return FALSE;
        }
        gnome_client_set_restart_style(client,
                                       GNOME_RESTART_NEVER);
        gnome_client_request_save (client,
                                   GNOME_SAVE_LOCAL,
                                   FALSE/*not shutdown*/,
                                   GNOME_INTERACT_NONE,
                                   TRUE/*fast*/,
                                   FALSE/*not global*/);
        g_signal_connect(client,
                         "save-complete",
                         G_CALLBACK(gtk_main_quit),
                         NULL/*userdata*/);
        return TRUE;
}

/* ------------------------- static functions */

