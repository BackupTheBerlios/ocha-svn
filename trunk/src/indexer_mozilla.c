#include "indexer_mozilla.h"
#include "indexer_utils.h"
#include "ocha_gconf.h"
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-util.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <glib.h>

/**
 * \file index mozilla/firefox bookmarks
 */
static struct indexer_source *load(struct indexer *self, struct catalog *catalog, int id);
static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err);
static gboolean validate(struct indexer *, const char *name, const char *long_name, const char *path);
static gboolean index(struct indexer_source *, struct catalog *, GError **);
static void release(struct indexer_source *);
static gboolean bookmarks_read_line(FILE *, GString *, GError **err);
static char *html_expand_common_entities(const char *orig);
static gboolean catalog_index_bookmarks(struct catalog *catalog, int source_id, const char *bookmark_file, GError **err);
static gboolean discover(struct indexer *, struct catalog *catalog);
static char *display_name(struct catalog *catalog, int id);
static GtkWidget *editor_widget(struct indexer_source *source);


#define INDEXER_NAME "mozilla"
struct indexer indexer_mozilla =
{
   .name=INDEXER_NAME,
   .display_name="Mozilla/Firefox",
   .load_source=load,
   .execute=execute,
   .validate=validate,
   .discover=discover,

   .description=
    "Each source in this indexer corresponds to a profile "
    "of Mozilla or Mozilla/Firefox. This indexer keeps track of "
    "the bookmarks in these profiles.\n"
};

/**
 * A bookmark file that's just been
 * discovered, possibly with a label
 * if it came from a profile.ini or
 * if it has been found in a profile.ini
 * later.
 */
struct discovered
{
    /** path this was found it, to free with g_free() */
    char *path;
    /** name of the profile, to free with g_free(). may be null */
    char *profile_name;
};

static struct indexer_source *load(struct indexer *self, struct catalog *catalog, int id)
{
   struct indexer_source *retval = g_new(struct indexer_source, 1);
   retval->id=id;
   retval->indexer=self;
   retval->index=index;
   retval->release=release;
   retval->display_name=display_name(catalog, id);
   retval->editor_widget=editor_widget;
   return retval;
}
static void release(struct indexer_source *source)
{
   g_return_if_fail(source);
   g_free((gpointer)source->display_name);
   g_free(source);
}

static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *url, GError **err)
{
   g_return_val_if_fail(self!=NULL, FALSE);
   g_return_val_if_fail(name!=NULL, FALSE);
   g_return_val_if_fail(long_name!=NULL, FALSE);
   g_return_val_if_fail(url!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   printf("opening %s...\n", url);
   GError *gnome_err = NULL;
   gboolean shown;
    shown = gnome_url_show(url, &gnome_err);
   if(!shown)
   {
       /* use the good old  gnome-moz-remote
        * if url_show fails.
        */
       const char *argv[] = { "gnome-moz-remote", url };
       errno=0;

       gboolean executed = gnome_execute_async(NULL/*dir*/,
                                               2,
                                               (char * const *)argv);
       if(!executed)
           g_set_error(err,
                       RESULT_ERROR,
                       RESULT_ERROR_MISSING_RESOURCE,
                       "error opening %s: %s",
                       url,
                       gnome_err->message);
       g_error_free(gnome_err);
       return FALSE;
   }
   return TRUE;
}

static gboolean validate(struct indexer *self, const char *name, const char *long_name, const char *text_uri)
{
   return TRUE;
}

static gboolean index(struct indexer_source *self, struct catalog *catalog, GError **err)
{
   g_return_val_if_fail(self!=NULL, FALSE);
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   char *path = NULL;
   if(!catalog_get_source_attribute_witherrors(INDEXER_NAME,
                                               self->id,
                                               "path",
                                               &path,
                                               TRUE/*required*/,
                                               err))
      {
         return FALSE;
      }
   gboolean retval = catalog_index_bookmarks(catalog,
                                             self->id,
                                             path,
                                             err);
   g_free(path);
   return retval;
}

static gboolean catalog_index_bookmarks(struct catalog *catalog, int source_id, const char *bookmark_file, GError **err)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(bookmark_file!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   FILE *fh = fopen(bookmark_file, "r");
   if(!fh)
      {
         g_set_error(err,
                     INDEXER_ERROR,
                     INDEXER_INVALID_INPUT,
                     "error opening %s for reading: %s",
                     bookmark_file,
                     strerror(errno));
         return FALSE;
      }

   gboolean error=FALSE;
   GString *line = g_string_new("");
   while( !error && bookmarks_read_line(fh, line, err) )
      {
         char *a_open = strstr(line->str, "<A");
         if(a_open)
            {
               char *href = strstr(a_open, "HREF=\"");
               if(href)
                  {
                     char *href_start = href+strlen("HREF=\"");
                     char *href_end = strstr(href_start, "\"");
                     if(href_end)
                        {
                           char *a_end = strstr(a_open, ">");
                           if(a_end)
                              {
                                 char *a_close = strstr(a_end, "</A>");
                                 if(a_close)
                                    {
                                       *href_end='\0';
                                       *a_close='\0';
                                       char *expanded_label = html_expand_common_entities(a_end+1);
                                       if(!catalog_addentry_witherrors(catalog,
                                                                       href_start,
                                                                       expanded_label,
                                                                       href_start,
                                                                       source_id,
                                                                       err))
                                          {
                                             error=TRUE;
                                          }
                                       g_free(expanded_label);
                                    }
                              }
                        }
                  }
            }
      }
   fclose(fh);
   return !error;
}

static gboolean bookmarks_read_line(FILE *fh, GString *line, GError **err)
{
   int buffer_len=256;
   char buffer[buffer_len];
   g_string_truncate(line, 0);
   errno=0;
   while(fgets(buffer, buffer_len, fh)!=NULL)
      {
         char *nl = strchr(buffer, '\n');
         if(nl)
            *nl='\0';
         g_string_append(line, buffer);
         if(nl)
            return TRUE;
      }
   if(errno!=0)
      {
         g_set_error(err,
                     INDEXER_ERROR,
                     INDEXER_EXTERNAL_ERROR,
                     "read error: %s",
                     strerror(errno));
         return FALSE;
      }
   return line->len>0;
}

static char *html_expand_common_entities(const char *str)
{
   GString *retval = g_string_new("");
   for(const char *c=str; *c!='\0'; c++)
      {
         if(*c=='&')
            {
               gboolean writec=FALSE;
               const char *start=c+1;
               const char *end=strchr(start, ';');
               if(end)
                  {
                     if(*start=='#')
                        {
                           /* character entity */
                           gunichar unichar = (gunichar)g_strtod(start+1, NULL/*endptr*/);
                           if(g_unichar_validate(unichar))
                              {
                                 char buffer[6];
                                 int len = g_unichar_to_utf8(unichar, buffer);
                                 g_string_append_len(retval, buffer, len);
                              }
                           else
                              writec=TRUE;
                        }
                     else if(strncmp("amp", start, end-start)==0)
                        {
                           g_string_append_c(retval, '&');
                        }
                     else if(strncmp("gt", start, end-start)==0)
                        {
                           g_string_append_c(retval, '>');
                        }
                     else if(strncmp("lt", start, end-start)==0)
                        {
                           g_string_append_c(retval, '<');
                        }
                     else if(strncmp("nbsp", start, end-start)==0)
                        {
                           g_string_append_c(retval, ' ');
                        }
                     else
                        writec=TRUE;
                     if(!writec)
                        c=end;
                  }
               else
                  writec=TRUE;
               if(writec)
                  g_string_append_c(retval, *c);
            }
         else
            g_string_append_c(retval, *c);
      }
   char *retval_str = retval->str;
   g_string_free(retval, FALSE/*don't free retval_str*/);
   return retval_str;
}

static struct discovered *discover_add_boorkmark_file(GArray *discovereds, const char *path)
{
   struct discovered *start = (struct discovered *)discovereds->data;
   struct discovered *end = start+discovereds->len;
   for(struct discovered *current=start; current<end; current++)
      {
         if(strcmp(path, current->path)==0)
             return current;
      }
   g_array_set_size(discovereds, discovereds->len+1);
   struct discovered *last = ((struct discovered *)discovereds->data)+(discovereds->len-1);
   last->path=g_strdup(path);
   last->profile_name=NULL;
   return last;
}

static void discover_add_profiles_file(GArray *discovereds, const char *path)
{
    GKeyFile *keyfile = g_key_file_new();
    if(g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, NULL/*err*/))
        {
            int groups_len = 0;
            gchar **groups = g_key_file_get_groups(keyfile, &groups_len);
            if(groups)
                {
                    for(int i=0; i<groups_len; i++)
                        {
                            gchar *group = groups[i];
                            gchar *profile_name = g_key_file_get_string(keyfile,
                                                                        group,
                                                                        "Name",
                                                                        NULL/*err*/);
                            if(profile_name)
                                {
                                    gchar *profile_path = g_key_file_get_string(keyfile,
                                                                                group,
                                                                                "Path",
                                                                                NULL/*err*/);
                                    if(profile_path)
                                        {
                                            struct discovered *discovered;
                                            char full_profile_path[strlen(path)+1+strlen(profile_path)+1+strlen("bookmarks.html")+1];
                                            if(*profile_path=='/')
                                                strcpy(full_profile_path, profile_path);
                                            else
                                                {
                                                    strcpy(full_profile_path, path);
                                                    char *lastslash = strrchr(full_profile_path, '/');
                                                    if(lastslash)
                                                        *lastslash='\0';
                                                    strcat(full_profile_path, "/");
                                                    strcat(full_profile_path, profile_path);
                                                }
                                            strcat(full_profile_path, "/");
                                            strcat(full_profile_path, "bookmarks.html");

                                            discovered = discover_add_boorkmark_file(discovereds,
                                                                                     full_profile_path);
                                            discovered->profile_name=g_strdup(profile_name);
                                            g_free(profile_path);
                                        }
                                    g_free(profile_name);
                                }
                        }
                    g_strfreev(groups);
                }
        }
    g_key_file_free(keyfile);
}

gboolean discover_callback(struct catalog *catalog,
                           int ignored,
                           const char *path,
                           const char *filename,
                           GError **err,
                           gpointer userdata)
{
   GArray *discovereds = (GArray *)userdata;
   g_return_val_if_fail(filename!=NULL, FALSE);
   g_return_val_if_fail(discovereds!=NULL, FALSE);
   g_return_val_if_fail(path!=NULL, FALSE);

   if(strcmp("bookmarks.html", filename)==0)
       discover_add_boorkmark_file(discovereds, path);
   else if(strcmp("profiles.ini", filename)==0)
       discover_add_profiles_file(discovereds, path);
   return TRUE;
}

static gboolean discover(struct indexer *indexer, struct catalog *catalog)
{
   GArray *discovereds = g_array_new(FALSE/*not zero terminated*/, TRUE/*clear*/, sizeof(struct discovered));
   gboolean retval = TRUE;
   char *directories[] = { ".mozilla", ".firefox", ".phoenix", NULL };
   for(int i=0; retval && directories[i]; i++)
      {
         char *path = gnome_util_prepend_user_home(directories[i]);
         if(g_file_test(path, G_FILE_TEST_EXISTS)
            && g_file_test(path, G_FILE_TEST_IS_DIR))
            {
               retval=recurse(catalog,
                              path,
                              NULL/*ignore*/,
                              4/*depth*/,
                              FALSE/*not slow*/,
                              0/*source_id, ignored*/,
                              discover_callback,
                              discovereds/*userdata*/,
                              NULL/*err*/);
            }
         g_free(path);
      }
   if(discovereds->len>0)
      {
         struct discovered *start = (struct discovered *)discovereds->data;
         struct discovered *end = start+discovereds->len;
         for(struct discovered *current=start; current<end; current++)
            {
               int id;
               if(catalog_add_source(catalog, INDEXER_NAME, &id))
                  {
                      if(ocha_gconf_set_source_attribute(INDEXER_NAME,
                                                         id,
                                                         "path",
                                                         current->path))
                        {
                           if(current->profile_name)
                               ocha_gconf_set_source_attribute(INDEXER_NAME,
                                                               id,
                                                               "profile",
                                                               current->profile_name);
                        }
                     else
                        retval=FALSE;
                  }
               else
                  retval=FALSE;

               g_free(current->path);
               if(current->profile_name)
                  g_free(current->profile_name);
            }
      }
   g_array_free(discovereds, TRUE/*free content*/);
   return retval;
}

static char *display_name(struct catalog *catalog, int id)
{
    char *uri=ocha_gconf_get_source_attribute(INDEXER_NAME, id, "path");
    char *retval=NULL;
    if(uri==NULL)
        retval=g_strdup("Invalid");
    else
    {
        char *profile_name=ocha_gconf_get_source_attribute(INDEXER_NAME, id, "profile");

        char *path=NULL;
        if(g_str_has_prefix(uri, "file://"))
            path=&uri[strlen("file://")];
        else if(*uri=='/')
            path=uri;

        if(path)
        {
            const char *home = g_get_home_dir();
            if(g_str_has_prefix(path, home))
            {
                GString *gstr = g_string_new("");
                if(strstr(path, "firefox")
                   || strstr(path, "Firefox")
                   || strstr(path, "firebird")
                   || strstr(path, "phoenix"))
                {
                    g_string_append(gstr, "Firefox ");
                }
                else
                   g_string_append(gstr, "Mozilla ");
                if(profile_name)
                    {
                        g_string_append(gstr, "Profile \"");
                        g_string_append(gstr, profile_name);
                        g_string_append(gstr, "\"");
                    }
                else
                    {
                        if(!strstr(path, "default"))
                            {
                                /* not the default, print the path instead of the
                                 * profile name...
                                 */
                                g_string_append(gstr, &path[strlen(home)+1]);
                            }
                        else
                            g_string_append(gstr, "Default Profile");
                    }
                retval=g_strdup(gstr->str);
                g_string_free(gstr, TRUE);
            }
            else
            {
                retval=path;
                uri=NULL;
            }
        }
        if(!retval)
        {
            retval=uri;
            uri=NULL;
        }
        if(profile_name)
            g_free(profile_name);
    }
    if(uri)
        g_free(uri);
    return retval;
}

static GtkWidget *editor_widget(struct indexer_source *source)
{
    return gtk_label_new(source->display_name);
}
