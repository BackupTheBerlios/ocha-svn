
#include "indexer_applications.h"
#include "indexer_utils.h"
#include "ocha_gconf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-util.h>
#include "desktop_file.h"


/**
 * \file index applications with a .desktop file
 */

#define DESKTOP_SECTION "Desktop Entry"

#define INDEXER_NAME "applications"

/* ------------------------- prototypes: indexer_application */

static struct indexer_source *indexer_application_load_source(struct indexer *self, struct catalog *catalog, int id);
static gboolean indexer_application_execute(struct indexer *self, const char *name, const char *long_name, const char *uri, GError **err);
static gboolean indexer_application_validate(struct indexer *self, const char *name, const char *long_name, const char *text_uri);
static gboolean indexer_application_discover(struct indexer *indexer, struct catalog *catalog);

/* ------------------------- prototypes: indexer_application_source */
static void indexer_application_source_release(struct indexer_source *source);
static gboolean index_application_cb(struct catalog *catalog, int source_id, const char *path, const char *filename, GError **err, gpointer userdata);
static gboolean indexer_application_source_index(struct indexer_source *self, struct catalog *catalog, GError **err);
static GtkWidget *indexer_application_source_editor_widget(struct indexer_source *source);
static guint indexer_application_source_notify_add(struct indexer_source *source, struct catalog *catalog, indexer_source_notify_f notify, gpointer userdata);
static void indexer_application_source_notify_remove(struct indexer_source *source, guint id);

/* ------------------------- prototypes: other */
static gboolean add_source(struct catalog *catalog, const char *path, int depth, char *ignore);
static gboolean add_source_for_directory(struct catalog *catalog, const char *possibility);
static char *display_name(struct catalog *catalog, int id);
static void remove_exec_percents(char *str);

/* ------------------------- definitions */

struct indexer indexer_applications = {
        INDEXER_NAME,
        "Applications"/*display_name*/,

        /* description */
        "This indexer looks into standard locations for "
        "application definitions (.desktop files).\n"
        "The application of this indexer should correspond"
        "to what you have in your application menu.\n",

        indexer_application_discover,
        indexer_application_load_source,
        indexer_application_execute,
        indexer_application_validate,
        NULL/*new_source*/
};

/* ------------------------- public functions */

/* ------------------------- member functions (indexer_application) */
static struct indexer_source *indexer_application_load_source(struct indexer *self, struct catalog *catalog, int id)
{
        struct indexer_source *retval = g_new(struct indexer_source, 1);
        retval->id=id;
        retval->indexer=self;
        retval->index=indexer_application_source_index;
        retval->release=indexer_application_source_release;
        retval->display_name=display_name(catalog, id);
        retval->editor_widget=indexer_application_source_editor_widget;
        retval->notify_display_name_change=indexer_application_source_notify_add;
        retval->remove_notification=indexer_application_source_notify_remove;
        return retval;
}
static gboolean indexer_application_execute(struct indexer *self, const char *name, const char *long_name, const char *uri, GError **err)
{
        GError *gnome_err;
        gboolean retval;
        GnomeDesktopFile *desktopfile;

        g_return_val_if_fail(self!=NULL, FALSE);
        g_return_val_if_fail(name!=NULL, FALSE);
        g_return_val_if_fail(long_name!=NULL, FALSE);
        g_return_val_if_fail(uri!=NULL, FALSE);
        g_return_val_if_fail(g_str_has_prefix(uri, "file://"), FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

        gnome_err =  NULL;

        retval =  FALSE;
        desktopfile =  gnome_desktop_file_load(&uri[strlen("file://")],
                                        &gnome_err);
        if(desktopfile==NULL)
        {
                g_set_error(err,
                            RESULT_ERROR,
                            RESULT_ERROR_INVALID_RESULT,
                            "entry for application %s not found in %s: %s",
                            name,
                            uri,
                            gnome_err->message);
                g_error_free(gnome_err);
        } else
        {
                char *exec = NULL;
                gboolean terminal = FALSE;

                gnome_desktop_file_get_string(desktopfile,
                                              DESKTOP_SECTION,
                                              "Exec",
                                              &exec);
                gnome_desktop_file_get_boolean(desktopfile,
                                               DESKTOP_SECTION,
                                               "Terminal",
                                               &terminal);


                if(!exec) {
                        g_set_error(err,
                                    RESULT_ERROR,
                                    RESULT_ERROR_INVALID_RESULT,
                                    "entry for application %s found in %s has no 'Exec' defined",
                                    name,
                                    uri);
                } else {
                        int pid;
                        remove_exec_percents(exec);
                        printf("execute: '%s' terminal=%d\n",
                               exec,
                               (int)terminal);
                        errno=0;
                        if(terminal)
                                pid=gnome_execute_terminal_shell(NULL, exec);
                        else
                                pid=gnome_execute_shell(NULL, exec);
                        if(pid==-1) {
                                g_set_error(err,
                                            RESULT_ERROR,
                                            RESULT_ERROR_MISSING_RESOURCE,
                                            "launching '%s' failed: %s",
                                            exec,
                                            errno!=0 ? strerror(errno):"unknown error");
                        } else {
                                printf("started application %s with PID %d\n",
                                       exec,
                                       pid);
                                retval=TRUE;
                        }
                }
                if(exec)
                        g_free(exec);
                gnome_desktop_file_free(desktopfile);
        }

        return retval;
}


static gboolean indexer_application_validate(struct indexer *self, const char *name, const char *long_name, const char *text_uri)
{
        g_return_val_if_fail(text_uri!=NULL, FALSE);
        g_return_val_if_fail(self==&indexer_applications, FALSE);
        return uri_exists(text_uri);
}

static gboolean indexer_application_discover(struct indexer *indexer, struct catalog *catalog)
{
        gboolean retval = TRUE;
        char *home = gnome_util_prepend_user_home(".local/share");
        char *possibilities[] =
                {
                        NULL,
                        "/usr/share",
                        "/usr/local/share",
                        "/opt/gnome/share",
                        "/opt/kde/share",
                        "/opt/kde2/share",
                        "/opt/kde3/share",
                        NULL
                };
        char **ptr;
        possibilities[0]=home;
        for(ptr=possibilities; *ptr; ptr++) {
                char *possibility = *ptr;
                if(!add_source_for_directory(catalog, possibility)) {
                        retval=FALSE;
                        break;
                }
        }
        g_free(home);
        return retval;
}


/* ------------------------- member functions (indexer_application_source) */
static guint indexer_application_source_notify_add(struct indexer_source *source,
                                        struct catalog *catalog,
                                        indexer_source_notify_f notify,
                                        gpointer userdata)
{
        g_return_val_if_fail(source, 0);
        g_return_val_if_fail(notify, 0);
        return source_attribute_change_notify_add(&indexer_applications,
                        source->id,
                        "path",
                        catalog,
                        notify,
                        userdata);
}
static void indexer_application_source_notify_remove(struct indexer_source *source,
                                guint id)
{
        source_attribute_change_notify_remove(id);
}
static void indexer_application_source_release(struct indexer_source *source)
{
        g_return_if_fail(source);
        g_free((gpointer)source->display_name);
        g_free(source);
}

static gboolean indexer_application_source_index(struct indexer_source *self, struct catalog *catalog, GError **err)
{
        g_return_val_if_fail(self!=NULL, FALSE);
        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

        remove_invalid_entries(&indexer_applications, self->id, catalog);
        return index_recursively(INDEXER_NAME,
                                 catalog,
                                 self->id,
                                 index_application_cb,
                                 self/*userdata*/,
                                 err);
}

static GtkWidget *indexer_application_source_editor_widget(struct indexer_source *source)
{
        return gtk_label_new(source->display_name);
}


/* ------------------------- static functions */

static gboolean index_application_cb(struct catalog *catalog,
                                     int source_id,
                                     const char *path,
                                     const char *filename,
                                     GError **err,
                                     gpointer userdata)
{
        char *uri;
        GnomeDesktopFile *desktopfile;
        char *type = NULL;
        char *name = NULL;
        char *comment = NULL;
        char *generic_name = NULL;
        gboolean nodisplay = FALSE;
        gboolean hidden = FALSE;
        char *exec = NULL;
        char *description=NULL;
        gboolean retval;

        if(!g_str_has_suffix(filename, ".desktop"))
                return TRUE;

        uri = g_strdup_printf("file://%s", path);

        desktopfile =  gnome_desktop_file_load(path,
                                               NULL/*ignore errors*/);
        if(desktopfile==NULL)
                return TRUE;


        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "Type",
                                      &type);

        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "Name",
                                      &name);

        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "Comment",
                                      &comment);

        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "GenericName",
                                      &generic_name);

        gnome_desktop_file_get_boolean(desktopfile,
                                       DESKTOP_SECTION,
                                       "NoDisplay",
                                       &nodisplay);


        gnome_desktop_file_get_boolean(desktopfile,
                                       DESKTOP_SECTION,
                                       "Hidden",
                                       &hidden);

        gnome_desktop_file_get_string(desktopfile,
                                      DESKTOP_SECTION,
                                      "Exec",
                                      &exec);


        if(comment && generic_name)
                description=g_strdup_printf("%s (%s)", comment, generic_name);

        retval = TRUE;
        if((type==NULL || g_strcasecmp("Application", type)==0)
           && !hidden
           && !nodisplay
           && exec!=NULL)
        {
                const char *long_name=description;
                if(!long_name)
                        long_name=comment;
                if(!long_name)
                        long_name=generic_name;
                if(!long_name)
                        long_name=path;
                retval=catalog_addentry_witherrors(catalog,
                                                   uri,
                                                   name==NULL ? filename:name,
                                                   long_name,
                                                   source_id,
                                                   err);
        }

        if(description)
                g_free(description);
        if(comment)
                g_free(comment);
        if(generic_name)
                g_free(generic_name);
        if(name)
                g_free(name);
        if(exec)
                g_free(exec);
        g_free(uri);

        gnome_desktop_file_free(desktopfile);
        return retval;
}

static gboolean add_source(struct catalog *catalog, const char *path, int depth, char *ignore)
{
        int id;
        if(!catalog_add_source(catalog, INDEXER_NAME, &id))
                return FALSE;
        if(!ocha_gconf_set_source_attribute(INDEXER_NAME, id, "path", path))
                return FALSE;
        if(depth!=-1)
        {
                char *depth_str = g_strdup_printf("%d", depth);
                gboolean ret = ocha_gconf_set_source_attribute(INDEXER_NAME, id, "depth", depth_str);
                g_free(depth_str);
                return ret;
        }
        if(ignore)
        {
                if(ocha_gconf_set_source_attribute(INDEXER_NAME, id, "ignore", ignore))
                        return FALSE;
        }
        return TRUE;
}

static gboolean add_source_for_directory(struct catalog *catalog, const char *possibility)
{
        if(g_file_test(possibility, G_FILE_TEST_EXISTS)
                        && g_file_test(possibility, G_FILE_TEST_IS_DIR))
        {
                return add_source(catalog,
                                  possibility,
                                  10/*depth*/,
                                  "locale,man,themes,doc,fonts,perl,pixmaps");
        }
        return TRUE;
}
static char *display_name(struct catalog *catalog, int id)
{
        char *uri= ocha_gconf_get_source_attribute(INDEXER_NAME, id, "path");
        char *retval=NULL;

        if(uri==NULL)
                retval=g_strdup("Invalid");
        else
        {
                char *path=NULL;
                if(g_str_has_prefix(uri, "file://"))
                        path=&uri[strlen("file://")];
                else if(*uri=='/')
                        path=uri;
                if(path) {
                        const char *home = g_get_home_dir();
                        if(g_str_has_prefix(path, home) && strcmp(&path[strlen(home)], "/.local/share")==0)
                                retval=g_strdup("User Applications");
                        else if(strcmp("/usr/share", path)==0)
                                retval=g_strdup("System Applications");
                        else if(strcmp("/usr/local/share", path)==0)
                                retval=g_strdup("System Applications (/usr/local)");
                        else if(g_str_has_prefix(path, "/opt") && strstr(path, "gnome"))
                                retval=g_strdup("GNOME Applications");
                        else if(g_str_has_prefix(path, "/opt") && strstr(path, "kde"))
                                retval=g_strdup("KDE Applications");
                        else {
                                retval=path;
                                uri=NULL;
                        }
                }
                if(!retval) {
                        retval=uri;
                        uri=NULL;
                }
        }
        if(uri)
                g_free(uri);
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
