#include "catalog_index.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <libgnomevfs/gnome-vfs.h>
#include "desktop_file.h"

/** \file implementation of the API defined in catalog_index.h */

/** default patterns, as strings */
static const char *DEFAULT_IGNORE_STRINGS[] = { "CVS", "*~", "*.bak", "#*#", NULL };

/** pattern spec created the 1st time catalog_index_directory() is called (and never freed) */
static GPatternSpec **DEFAULT_IGNORE;

/** callback set by catalog_index_set_callback() */
static catalog_index_trace_callback_f trace_callback;
/** callback userdata passed to catalog_index_set_callback() */
static gpointer trace_callback_userdata;

static gboolean getmode(const char *path, mode_t *mode);
static gboolean is_accessible_directory(mode_t mode);
static gboolean is_accessible_file(mode_t mode);
static gboolean is_readable(mode_t mode);
static gboolean is_executable(mode_t mode);

static gboolean to_ignore(const char *filename, GPatternSpec **patterns);

static gboolean catalog_index_applications_recursive(struct catalog *catalog, const char *directory, int maxdepth, gboolean slow, int cmd);
static GPatternSpec **create_patterns(const char **patterns);
static void free_patterns(GPatternSpec **);

static gboolean has_gnome_mime_command(const char *path);
static gboolean bookmarks_read_line(FILE *, GString *, GError **err);

static char *html_expand_common_entities(const char *orig);

static void doze_off(gboolean really);

static gboolean catalog_addcommand_witherrors(struct catalog *catalog, const char *name, const char *execute, int *cmd, GError **err);
static gboolean catalog_addentry_witherrors(struct catalog *catalog, const char *path, const char *name, int cmd_id, GError **err);
static DIR *opendir_witherrors(const char *path, GError **err);

typedef gboolean (*handle_file_f)(struct catalog *catalog, int cmd, const char *path, const char *filename, GError **err, gpointer userdata);
static gboolean recurse(struct catalog *catalog,
                    const char *directory,
                    GPatternSpec **ignore_patterns,
                    int maxdepth,
                    gboolean slow,
                    int cmd,
                    handle_file_f callback,
                    gpointer userdata,
                    GError **err);
static gboolean index_directory_entry(struct catalog *catalog, int cmd, const char *path, const char *filename, GError **err, gpointer userdata);
static gboolean index_application_entry(struct catalog *catalog, int cmd, const char *path, const char *filename, GError **err, gpointer userdata);

/* ------------------------- public functions */
GQuark catalog_index_error_quark()
{
   static GQuark quark;
   static gboolean initialized;
   if(!initialized)
      {
         quark=g_quark_from_static_string("CATALOG_INDEX");
         initialized=TRUE;
      }
   return quark;
}
void catalog_index_init()
{
   DEFAULT_IGNORE=create_patterns(DEFAULT_IGNORE_STRINGS);
   gnome_vfs_init();
}

void catalog_index_set_trace_callback(catalog_index_trace_callback_f callback, gpointer userdata)
{
   trace_callback=callback;
   trace_callback_userdata=userdata;
}

gboolean catalog_index_directory(struct catalog *catalog, const char *directory, int maxdepth, const char **ignore, gboolean slow, GError **err)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(directory!=NULL, FALSE);
   g_return_val_if_fail(maxdepth==-1 || maxdepth>0, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);
   g_return_val_if_fail(DEFAULT_IGNORE!=NULL, FALSE); /* call catalog_index_init!() */

   int cmd = -1;
   if(!catalog_addcommand_witherrors(catalog, "gnome-open", "gnome-open \"%f\"", &cmd, err))
      return FALSE;

   GPatternSpec **ignore_patterns = create_patterns(ignore);
   gboolean retval = recurse(catalog,
                         directory,
                         ignore_patterns,
                         maxdepth,
                         slow,
                         cmd,
                         index_directory_entry,
                         NULL/*userdata*/,
                         err);
   free_patterns(ignore_patterns);
   return retval;
}

gboolean catalog_index_applications(struct catalog *catalog, const char *directory, int maxdepth, gboolean slow, GError **err)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(directory!=NULL, FALSE);
   g_return_val_if_fail(maxdepth==-1 || maxdepth>0, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   int cmd = -1;
   if(!catalog_addcommand_witherrors(catalog, "run-desktop-entry", "run-desktop-entry \"%f\"", &cmd, err))
      return FALSE;

   return recurse(catalog,
                  directory,
                  NULL/*no ignore_patterns*/,
                  maxdepth,
                  slow,
                  cmd,
                  index_application_entry,
                  NULL/*userdata*/,
                  err);
}

gboolean catalog_index_bookmarks(struct catalog *catalog, const char *bookmark_file, GError **err)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(bookmark_file!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   int cmd = -1;
   if(!catalog_addcommand(catalog, "gnome-moz-remote", "gnome-moz-remote \"%f\"", &cmd))
      return FALSE;


   FILE *fh = fopen(bookmark_file, "r");
   if(!fh)
      {
         g_set_error(err,
                     CATALOG_INDEX_ERROR,
                     CATALOG_INDEX_INVALID_INPUT,
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
                     char *quote_start = href+strlen("HREF=\"");
                     char *quote_end = strstr(quote_start, "\"");
                     if(quote_end)
                        {
                           char *a_end = strstr(a_open, ">");
                           if(a_end)
                              {
                                 char *a_close = strstr(a_end, "</A>");
                                 if(a_close)
                                    {
                                       *quote_end='\0';
                                       *a_close='\0';
                                       char *unescaped_label = html_expand_common_entities(a_end+1);
                                       if(!catalog_addentry_witherrors(catalog,
                                                                       quote_start,
                                                                       unescaped_label,
                                                                       cmd,
                                                                       err))
                                          {
                                             error=TRUE;
                                          }
                                       g_free(unescaped_label);
                                    }
                              }
                        }
                  }
            }
      }
   fclose(fh);
   return !error;
}

/* ------------------------- private functions */


static gboolean catalog_addcommand_witherrors(struct catalog *catalog, const char *name, const char *execute, int *cmd, GError **err)
{
   if(!catalog_addcommand(catalog, name, execute, cmd))
      {
         g_set_error(err,
                     CATALOG_INDEX_ERROR,
                     CATALOG_INDEX_CATALOG_ERROR,
                     "could not add/refresh %s command in catalog: %s",
                     name,
                     catalog_error(catalog));
         return FALSE;
      }
   return TRUE;
}
static gboolean catalog_addentry_witherrors(struct catalog *catalog, const char *path, const char *name, int cmd_id, GError **err)
{
   if(!catalog_addentry(catalog, path, name, cmd_id, NULL/*id_out*/))
      {
         g_set_error(err,
                     CATALOG_INDEX_ERROR,
                     CATALOG_INDEX_CATALOG_ERROR,
                     "could not add/refresh entry %s in catalog: %s",
                     path,
                     catalog_error(catalog));
         return FALSE;
      }
   if(trace_callback)
      trace_callback(path, name, trace_callback_userdata);
   return TRUE;
}

static DIR *opendir_witherrors(const char *path, GError **err)
{
   DIR *retval = opendir(path);
   if(retval==NULL)
      {
         g_set_error(err,
                     CATALOG_INDEX_ERROR,
                     CATALOG_INDEX_INVALID_INPUT,
                     "could not open directory: %s",
                     strerror(errno));
      }
   return retval;
}


static gboolean _recurse(struct catalog *catalog,
                     const char *directory,
                     DIR *dirhandle,
                     GPatternSpec **ignore_patterns,
                     int maxdepth,
                     gboolean slow,
                     int cmd,
                     handle_file_f callback,
                     gpointer userdata,
                     GError **err)
{
   if(maxdepth==0)
      return TRUE;
   if(maxdepth>0)
      maxdepth--;

   gboolean error=FALSE;
   struct dirent *dirent;
   while( !error && (dirent=readdir(dirhandle)) != NULL )
      {
         const char *filename = dirent->d_name;
         if(*filename=='.' || to_ignore(filename, DEFAULT_IGNORE) || to_ignore(filename, ignore_patterns))
            continue;

         char current_path[strlen(directory)+1+strlen(filename)+1];
         strcpy(current_path, directory);
         if(current_path[strlen(current_path)-1]!='/')
            strcat(current_path, "/");
         strcat(current_path, filename);

         mode_t mode;
         if(getmode(current_path, &mode))
            {
               if(is_accessible_directory(mode))
                  {
                     if(maxdepth!=0)
                        {
                           DIR *subdir = opendir(current_path);
                           if(subdir!=NULL)
                              {
                                 if(!_recurse(catalog,
                                              current_path,
                                              subdir,
                                              ignore_patterns,
                                              maxdepth,
                                              slow,
                                              cmd,
                                              callback,
                                              userdata,
                                              err))
                                    {
                                       error=TRUE;
                                    }
                                 else
                                    {
                                       doze_off(slow);
                                    }
                                 closedir(subdir);
                              }
                        }
                  }
               else if(is_accessible_file(mode))
                  {
                     if(!callback(catalog, cmd, current_path, filename, userdata, err))
                        error=TRUE;
                  }
            }
      }
   return !error;
}
static gboolean recurse(struct catalog *catalog,
                    const char *directory,
                    GPatternSpec **ignore_patterns,
                    int maxdepth,
                    gboolean slow,
                    int cmd,
                    handle_file_f callback,
                    gpointer userdata,
                    GError **err)
{
   DIR *dir=opendir_witherrors(directory, err);
   if(dir)
      {
         gboolean retval = _recurse(catalog,
                                directory,
                                dir,
                                ignore_patterns,
                                maxdepth,
                                slow,
                                cmd,
                                callback,
                                userdata,
                                err);
         closedir(dir);
         return retval;
      }
   else
      {
         return FALSE;
      }
}

static gboolean index_directory_entry(struct catalog *catalog, int cmd, const char *path, const char *filename, GError **err, gpointer userdata)
{
   if(!has_gnome_mime_command(path))
      return TRUE;

   return catalog_addentry_witherrors(catalog,
                                      path,
                                      filename,
                                      cmd,
                                      err);
}

static gboolean index_application_entry(struct catalog *catalog, int cmd, const char *path, const char *filename, GError **err, gpointer userdata)
{
   if(!g_str_has_suffix(filename, ".desktop"))
      return TRUE;

   GnomeDesktopFile *desktopfile = gnome_desktop_file_load(path,
                                                           NULL/*ignore errors*/);
   if(desktopfile==NULL)
      return TRUE;

   char *name = NULL;
   gnome_desktop_file_get_string(desktopfile,
                                 "Desktop Entry",
                                 "Name",
                                 &name);
   gboolean terminal = FALSE;
   gnome_desktop_file_get_boolean(desktopfile,
                                  "Desktop Entry",
                                  "Terminal",
                                  &terminal);

   char *exec = NULL;
   gnome_desktop_file_get_string(desktopfile,
                                 "Desktop Entry",
                                 "Exec",
                                 &exec);
   gboolean retval = TRUE;
   if(!terminal && exec!=NULL && strstr(exec, "%")==NULL)
      {
         retval=catalog_addentry_witherrors(catalog,
                                            path,
                                            name==NULL ? filename:name,
                                            cmd,
                                            err);
      }
   if(name)
      g_free(name);
   if(exec)
      g_free(exec);

   gnome_desktop_file_free(desktopfile);
   return retval;
}

static gboolean getmode(const char *path, mode_t* mode)
{
   struct stat buf;
   if(stat(path, &buf)==0)
      {
         *mode=buf.st_mode;
         return TRUE;
      }
   return FALSE;
}

static gboolean is_accessible_directory(mode_t mode)
{
   if(!S_ISDIR(mode))
      return FALSE;
   return is_readable(mode) && is_executable(mode);
}

static gboolean is_accessible_file(mode_t mode)
{
   if(!S_ISREG(mode))
      return FALSE;
   return is_readable(mode);
}

static gboolean is_readable(mode_t mode)
{
   return TRUE;
}

static gboolean is_executable(mode_t mode)
{
   return TRUE;
}

static gboolean to_ignore(const char *filename, GPatternSpec **patterns)
{
   if(patterns==NULL)
      return FALSE;
   for(int i=0; patterns[i]!=NULL; i++)
      {
         if(g_pattern_match_string(patterns[i], filename))
            return TRUE;
      }
   return FALSE;
}

static int null_terminated_array_length(const char **patterns)
{
   int count;
   for(count=0; *patterns!=NULL; count++, patterns++);
   return count;
}
static GPatternSpec **create_patterns(const char **patterns)
{
   if(patterns==NULL)
      return NULL;

   int len = null_terminated_array_length(patterns);
   GPatternSpec **retval = g_new(GPatternSpec *, len+1);
   for(int i=0; i<len; i++)
      retval[i]=g_pattern_spec_new(patterns[i]);
   retval[len]=NULL;
   return retval;
}
static void free_patterns(GPatternSpec **patterns)
{
   if(patterns==NULL)
      return;
   for(int i=0; patterns[i]!=NULL; i++)
      g_pattern_spec_free(patterns[i]);
   g_free(patterns);
}

static gboolean has_gnome_mime_command(const char *path)
{
   GString *uri = g_string_new("file://");
   if(*path!='/')
      {
         const char *cwd = g_get_current_dir();
         g_string_append(uri, cwd);
         g_free((void *)cwd);
         if(uri->str[uri->len-1]!='/')
            g_string_append_c(uri, '/');
      }
   g_string_append(uri, path);

   gboolean retval=FALSE;
   char *mimetype = gnome_vfs_get_mime_type(uri->str);
   if(mimetype)
      {
         char *app = gnome_vfs_mime_get_default_desktop_entry(mimetype);
         if(app)
            {
               g_free(app);
               retval=TRUE;
            }
         g_free(mimetype);
      }

   return retval;
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
                     CATALOG_INDEX_ERROR,
                     CATALOG_INDEX_EXTERNAL_ERROR,
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

static void doze_off(gboolean really)
{
   if(really)
      sleep(3);
}
