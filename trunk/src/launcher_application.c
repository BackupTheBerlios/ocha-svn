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
static GnomeDesktopFile *load_file(const char *uri, GError **err);
static void remove_exec_percents(char *str);
static GnomeVFSResult read_into_string(GnomeVFSHandle *handle, char *buffer, GnomeVFSFileSize size);

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
        gboolean terminal;

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
        desktopfile =  load_file(uri, err);
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
        return retval;
}

/**
 * Load the .desktop file using GNOME VFS.
 * @param uri
 * @param err if non-null, this will contain an error in the domain LAUNCHER_ERROR
 * @return desktop file or null
 */
static GnomeDesktopFile *load_file(const char *uri, GError **err)
{
        GError *gnome_err = NULL;
        GnomeVFSResult result;
        GnomeVFSFileSize size;
        GnomeVFSHandle *handle;
        GnomeVFSFileInfo info;
        GnomeDesktopFile *retval = NULL;

        g_return_val_if_fail(uri!=NULL, NULL);
        g_return_val_if_fail(err==NULL || *err==NULL, NULL);

        result = gnome_vfs_get_file_info(uri, &info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
        if(result==GNOME_VFS_OK) {
                size=info.size;
                if(size>(1024*1024)) {
                        g_set_error(err,
                                    LAUNCHER_ERROR,
                                    LAUNCHER_INVALID_URI,
                                    "Failed to parse application description in %s: it's too large (%lu bytes > 1Mb)",
                                    uri,
                                    (gulong)size);

                } else {
                        result = gnome_vfs_open(&handle, uri, GNOME_VFS_OPEN_READ);
                        if(result==GNOME_VFS_OK) {
                                char *buffer = malloc(size+1);
                                if(buffer!=NULL) {
                                        result=read_into_string(handle, buffer, size);
                                        if(result==GNOME_VFS_OK) {
                                                retval = gnome_desktop_file_new_from_string(buffer, &gnome_err);
                                        }
                                        free(buffer);
                                } else {
                                        g_set_error(err,
                                                    LAUNCHER_ERROR,
                                                    LAUNCHER_EXTERNAL_ERROR,
                                                    "Failed to allocate buffer for parsing %s: not enough memory (for %lu bytes)",
                                                    uri,
                                                    (gulong)size);
                                }
                                gnome_vfs_close(handle);
                        }
                }
        }

        if(gnome_err!=NULL) {
                g_set_error(err,
                            LAUNCHER_ERROR,
                            LAUNCHER_INVALID_URI,
                            "Failed to parse application description in %s: %s",
                            uri,
                            gnome_err->message);
        }
        if(result!=GNOME_VFS_OK) {
                g_set_error(err,
                            LAUNCHER_ERROR,
                            LAUNCHER_INVALID_URI,
                            "I/O error when parsing application description in %s: %s",
                            uri,
                            gnome_vfs_result_to_string(result));
        }
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

/**
 * Read size characters from the file into the buffer and
 * add a '\0'
 * @param handle
 * @param buffer a buffer with size+1 bytes free
 * @param size number of bytes to read
 * @return gnome vfs result
 */
static GnomeVFSResult read_into_string(GnomeVFSHandle *handle, char *buffer, GnomeVFSFileSize size)
{
        GnomeVFSResult result;
        GnomeVFSFileSize sofar=0;
        GnomeVFSFileSize read;

        while( (result=gnome_vfs_read(handle, &buffer[sofar], size-sofar, &read)) == GNOME_VFS_OK ) {
                if(read==0) {
                        break;
                }
                sofar+=read;
        }
        buffer[sofar]='\0';
        return result==GNOME_VFS_ERROR_EOF ? GNOME_VFS_OK:result;
}
