#include "launcher_openurl.h"

#include <libgnome/gnome-url.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-util.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <glib.h>

/** \file a launcher based on gnome_url_show
 *
 */

/* ------------------------- prototypes: member functions (launcher_openurl) */
static gboolean launcher_openurl_execute(struct launcher *launcher, const char *name, const char *long_name, const char *uri, GError **err);
static gboolean launcher_openurl_validate(struct launcher *launcher, const char *uri);

/* ------------------------- prototypes: private functions */

/* ------------------------- definitions */
struct launcher launcher_openurl = {
        LAUNCHER_OPENURL_ID,
        launcher_openurl_execute,
        launcher_openurl_validate
};

/* ------------------------- member functions (launcher_openurl) */
static gboolean launcher_openurl_execute(struct launcher *launcher,
                                         const char *name,
                                         const char *long_name,
                                         const char *url,
                                         GError **err)
{
        GError *gnome_err = NULL;
        gboolean shown;

        g_return_val_if_fail(launcher!=NULL, FALSE);
        g_return_val_if_fail(launcher==&launcher_openurl, FALSE);
        g_return_val_if_fail(name!=NULL, FALSE);
        g_return_val_if_fail(long_name!=NULL, FALSE);
        g_return_val_if_fail(url!=NULL, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);


        printf("%s:%d: opening %s...\n", __FILE__, __LINE__, url);
        shown = gnome_url_show(url, &gnome_err);
        if(!shown)
        {
                /* use the good old  gnome-moz-remote
                 * if url_show fails.
                 */
                gboolean executed;
                const char *argv[2];
                argv[0]= "gnome-moz-remote";
                argv[1]= url;
                errno=0;

                executed = gnome_execute_async(NULL/*dir*/,
                                               2,
                                               (char * const *)argv);
                if(!executed) {
                        g_set_error(err,
                                    LAUNCHER_ERROR,
                                    LAUNCHER_EXTERNAL_ERROR,
                                    "error opening URL %s: %s",
                                    url,
                                    gnome_err && gnome_err->message ? gnome_err->message:"Unknown error");
                        return FALSE;
                }
                if(gnome_err) {
                        g_error_free(gnome_err);
                }
        }
        return TRUE;
}

static gboolean launcher_openurl_validate(struct launcher *launcher,
                                          const char *uri)
{
        return TRUE;
}


/* ------------------------- static functions */

