#include "launcher_application.h"

#include <libgnome/gnome-url.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <glib.h>
#include "desktop_file.h"

#define DESKTOP_SECTION "Desktop Entry"

/** \file a launcher based on gnome_url_show
 *
 */

/* ------------------------- prototypes: member functions (launcher_application) */
static gboolean launcher_application_execute(struct launcher *launcher, const char *name, const char *long_name, const char *uri, GError **err);
static gboolean launcher_application_validate(struct launcher *launcher, const char *uri);

/* ------------------------- prototypes: private functions */
static gboolean get_application(const char *uri, char **exec_out, gboolean *terminal_out, GError **err);
static GnomeDesktopFile *load_desktop_file(const char *uri, GError **err);
static void remove_exec_percents(char *str);

/* ------------------------- definitions */
struct launcher launcher_application = {
        LAUNCHER_APPLICATION_ID,
        launcher_application_execute,
        launcher_application_validate
};

/* ------------------------- member functions (launcher_application) */
gboolean launcher_application_execute(struct launcher *launcher,
                                      const char *name,
                                      const char *long_name,
                                      const char *uri,
                                      GError **err)
{
        char *exec = NULL;
        gboolean retval = FALSE;
        gboolean terminal = FALSE;

        g_return_val_if_fail(launcher!=NULL, FALSE);
        g_return_val_if_fail(launcher==&launcher_application, FALSE);
        g_return_val_if_fail(name!=NULL, FALSE);
        g_return_val_if_fail(long_name!=NULL, FALSE);
        g_return_val_if_fail(uri!=NULL, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);


        printf("%s:%d: launching application %s using %s...\n", __FILE__, __LINE__, name, uri);

        if(get_application(uri, &exec, &terminal, err)) {
                int pid;

                printf("%s:%d: execute: '%s' (in terminal=%c)\n",
                       __FILE__,
                       __LINE__,
                       exec,
                       terminal ? 'y':'n');
                errno=0;
                if(terminal)
                        pid=gnome_execute_terminal_shell(NULL, exec);
                else
                        pid=gnome_execute_shell(NULL, exec);
                if(pid==-1) {
                        g_set_error(err,
                                    LAUNCHER_ERROR,
                                    LAUNCHER_EXTERNAL_ERROR,
                                    "launching '%s' failed: %s",
                                    exec,
                                    errno!=0 ? strerror(errno):"unknown error");
                } else {
                        printf("%s:%d: executed '%s' with PID %d\n",
                               __FILE__,
                               __LINE__,
                               exec,
                               pid);
                        retval=TRUE;
                }
        }

        if(exec) {
                g_free(exec);
        }
        g_assert(retval || err==NULL || *err!=NULL);
        return retval;
}

gboolean launcher_application_validate(struct launcher *launcher,
                                       const char *text_uri)
{
        GnomeVFSURI *uri;

        g_return_val_if_fail(launcher, FALSE);
        g_return_val_if_fail(launcher==&launcher_application, FALSE);
        g_return_val_if_fail(text_uri!=NULL, FALSE);

        uri=gnome_vfs_uri_new(text_uri);
        if(uri) {
                gboolean retval=gnome_vfs_uri_exists(uri);
                gnome_vfs_uri_unref(uri);
                return retval;
        } else {
                return FALSE;
        }
}


/* ------------------------- static functions */

/**
 * Get the shell command to execute from a .desktop file.
 *
 * @param uri uri of the .desktop file
 * @param exec_out this variable will be filled
 * with the shell command to execute if get_application returns
 * TRUE.
 * @param terminal_out this variable will be set to TRUE
 * if the application must run in a terminal, to FALSE
 * otherwise
 * @param err if non-null, this will contain a description of
 * the error when this function returns false
 * @return true if it worked, and exec_out and terminal_out have been set,
 * false if it failed, and err has been set
 */
static gboolean get_application(const char *uri, char **exec_out, gboolean *terminal_out, GError **err)
{
        gboolean retval;
        GnomeDesktopFile *desktopfile;

        g_return_val_if_fail(uri!=NULL, FALSE);
        g_return_val_if_fail(exec_out!=NULL, FALSE);
        g_return_val_if_fail(terminal_out!=NULL, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

        retval =  FALSE;
        desktopfile =  load_desktop_file(uri, err);
        if(desktopfile!=NULL) {
                char *exec = NULL;
                gboolean terminal = FALSE;

                gnome_desktop_file_get_boolean(desktopfile,
                                               DESKTOP_SECTION,
                                               "Terminal",
                                               &terminal);

                gnome_desktop_file_get_string(desktopfile,
                                              DESKTOP_SECTION,
                                              "Exec",
                                              &exec);
                if(!exec) {
                        g_set_error(err,
                                    LAUNCHER_ERROR,
                                    LAUNCHER_INVALID_URI,
                                    "entry for application description in %s has no 'Exec' defined",
                                    uri);
                } else {
                        remove_exec_percents(exec);
                        *exec_out=exec;
                        *terminal_out=terminal;
                        retval=TRUE;
                }
                gnome_desktop_file_free(desktopfile);
        }
        g_assert(retval || err==NULL || *err!=NULL);
        return retval;
}

/**
 * Load the .desktop file using GNOME VFS.
 * @param uri
 * @param err if non-null, this will contain an error in the domain LAUNCHER_ERROR
 * @return desktop file or null
 */
static GnomeDesktopFile *load_desktop_file(const char *uri, GError **err)
{
        GnomeDesktopFile *retval;
        GError *gnome_err = NULL;
        char *path;

        g_return_val_if_fail(uri, NULL);
        g_return_val_if_fail(err==NULL || *err==NULL, NULL);

        path=gnome_vfs_get_local_path_from_uri(uri);
        if(path==NULL) {
                g_set_error(err,
                            LAUNCHER_ERROR,
                            LAUNCHER_INVALID_URI,
                            "Error parsing %s: not a local file",
                            uri);
                g_error_free(gnome_err);
        }

        retval = gnome_desktop_file_load(path, err==NULL ? NULL:&gnome_err);

        if(gnome_err) {
                g_set_error(err,
                            LAUNCHER_ERROR,
                            LAUNCHER_INVALID_URI,
                            "Error parsing %s: %s",
                            uri,
                            gnome_err->message);
                g_error_free(gnome_err);
        }
        g_assert(retval!=NULL || err==NULL || *err!=NULL);
        return retval;
}

/**
 * Remove everything that starts with '%', except
 * for '%%', which gets transformed into a single '%'
 * @param str string to modify
 */
static void remove_exec_percents(char *str)
{
        char *from;
        char *to = str;
        for(from=str; *from!='\0'; from++) {
                if(*from=='%') {
                        from++; /* skip this */
                        if(from[1]=='%') {
                                *to='%';
                                to++;
                        }
                        /* else skip */
                } else {
                        if(from>to) {
                                *to=*from;
                        }
                        to++;
                }
        }
        *to='\0';
}
