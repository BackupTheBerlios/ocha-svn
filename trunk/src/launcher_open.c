#include "launcher_open.h"

#include <libgnome/gnome-url.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>

/** \file a launcher base on gnome_url_show that opens files accessible by GNOME VFS.
 *
 */

/* ------------------------- prototypes: member functions (launcher_open) */
static gboolean launcher_open_execute(struct launcher *launcher, const char *name, const char *long_name, const char *uri, GError **err);
static gboolean launcher_open_validate(struct launcher *launcher, const char *uri);

/* ------------------------- prototypes: static functions */

/* ------------------------- definitions */
struct launcher launcher_open = {
        LAUNCHER_OPEN_ID,
        launcher_open_execute,
        launcher_open_validate
};

/* ------------------------- public functions */

/* ------------------------- member functions (launcher_open) */
static gboolean launcher_open_execute(struct launcher *launcher,
                                      const char *name,
                                      const char *long_name,
                                      const char *text_uri,
                                      GError **err)
{
        GError *gnome_err;
        g_return_val_if_fail(launcher, FALSE);
        g_return_val_if_fail(launcher==&launcher_open, FALSE);
        g_return_val_if_fail(text_uri!=NULL, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);


        if(!launcher_open_validate(launcher, text_uri)) {
                g_set_error(err,
                            LAUNCHER_ERROR,
                            LAUNCHER_INVALID_URI,
                            "file not found: %s",
                            text_uri);
                return FALSE;
        }

        printf("%s:%d: opening %s...\n", __FILE__, __LINE__, text_uri);
        gnome_err =  NULL;
        if(!gnome_url_show(text_uri, &gnome_err)) {
                g_set_error(err,
                            LAUNCHER_ERROR,
                            LAUNCHER_EXTERNAL_ERROR,
                            "error opening file %s: %s",
                            text_uri,
                            gnome_err->message);
                g_error_free(gnome_err);
                return FALSE;
        }
        return TRUE;

}

static gboolean launcher_open_validate(struct launcher *launcher,
                                       const char *text_uri)
{
        GnomeVFSURI *uri;

        g_return_val_if_fail(launcher, FALSE);
        g_return_val_if_fail(launcher==&launcher_open, FALSE);
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

