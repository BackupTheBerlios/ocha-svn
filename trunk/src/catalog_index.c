#include "catalog_index.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <libgnomevfs/gnome-vfs.h>
#include "desktop_file.h"

/** \file implementation of the API defined in catalog_index.h */

/** default patterns, as strings */
static const char *DEFAULT_IGNORE_STRINGS[] = { "CVS", "*~", "*.bak", "#*#", NULL };
/** pattern spec created the 1st time catalog_index_directory() is called (and never freed) */
static GPatternSpec **DEFAULT_IGNORE;

static bool getmode(const char *path, mode_t *mode);
static bool is_accessible_directory(mode_t mode);
static bool is_accessible_file(mode_t mode);
static bool is_readable(mode_t mode);
static bool is_executable(mode_t mode);

static bool to_ignore(const char *filename, GPatternSpec **patterns);

static bool catalog_index_directory_recursive(struct catalog *catalog, const char *directory, int maxdepth, GPatternSpec **patterns, bool slow, int cmd);
static bool catalog_index_applications_recursive(struct catalog *catalog, const char *directory, int maxdepth, bool slow, int cmd);
static GPatternSpec **create_patterns(const char **patterns);
static void free_patterns(GPatternSpec **);

static bool has_gnome_mime_command(const char *path);
static bool bookmarks_read_line(FILE *, GString *);

static char *html_expand_common_entities(const char *orig);

/* ------------------------- public functions */

void catalog_index_init()
{
   DEFAULT_IGNORE=create_patterns(DEFAULT_IGNORE_STRINGS);
   gnome_vfs_init();
}

bool catalog_index_directory(struct catalog *catalog, const char *directory, int maxdepth, const char **ignore, bool slow)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(directory!=NULL, false);
   g_return_val_if_fail(maxdepth==-1 || maxdepth>0, false);
   g_return_val_if_fail(DEFAULT_IGNORE!=NULL, false); /* call catalog_index_init!() */

   int cmd = -1;
   if(!catalog_addcommand(catalog, "gnome-open", "gnome-open '%f'", &cmd))
      return false;




   GPatternSpec **ignore_patterns = create_patterns(ignore);
   bool retval = catalog_index_directory_recursive(catalog,
                                                   directory,
                                                   maxdepth,
                                                   ignore_patterns,
                                                   slow,
                                                   cmd);
   free_patterns(ignore_patterns);
   return retval;
}

bool catalog_index_applications(struct catalog *catalog, const char *directory, int maxdepth, bool slow)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(directory!=NULL, false);
   g_return_val_if_fail(maxdepth==-1 || maxdepth>0, false);

   int cmd = -1;
   if(!catalog_addcommand(catalog, "run-desktop-entry", "run-desktop-entry '%f'", &cmd))
      return false;


   return catalog_index_applications_recursive(catalog,
                                               directory,
                                               maxdepth,
                                               slow,
                                               cmd);
}

bool catalog_index_bookmarks(struct catalog *catalog, const char *bookmark_file)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(bookmark_file!=NULL, false);

   int cmd = -1;
   if(!catalog_addcommand(catalog, "gnome-moz-remote", "gnome-moz-remote '%f'", &cmd))
      return false;

   bool retval=true;
   FILE *fh = fopen(bookmark_file, "r");
   if(fh)
      {
         GString *line = g_string_new("");
         while( bookmarks_read_line(fh, line) )
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
                                             if(!catalog_addentry(catalog,
                                                                  quote_start,
                                                                  unescaped_label,
                                                                  cmd,
                                                                  NULL/*id_out*/))
                                                {
                                                   retval=false;
                                                }
                                             g_free(unescaped_label);
                                          }
                                    }
                              }
                        }
                  }
            }
         fclose(fh);
      }
   else
      {
         retval=false;
      }
   return retval;
}

/* ------------------------- private functions */

static bool catalog_index_directory_recursive(struct catalog *catalog, const char *directory, int maxdepth, GPatternSpec **ignore, bool slow, int cmd)
{
   if(maxdepth==0)
      return true;
   if(maxdepth>0)
      maxdepth--;

   bool retval=true;
   DIR *dir = opendir(directory);
   if(dir==NULL)
      return false;

   struct dirent *dirent;
   while( (dirent=readdir(dir)) != NULL )
      {
         const char *filename = dirent->d_name;
         if(*filename=='.'
            || to_ignore(filename, DEFAULT_IGNORE)
            || to_ignore(filename, ignore))
            continue;

         char buffer[strlen(directory)+1+strlen(filename)+1];
         strcpy(buffer, directory);
         strcat(buffer, "/");
         strcat(buffer, filename);

         mode_t mode;
         if(getmode(buffer, &mode))
            {
               if(is_accessible_directory(mode))
                  {
                     if(maxdepth!=0)
                        {
                           if(!catalog_index_directory_recursive(catalog,
                                                                 buffer,
                                                                 maxdepth,
                                                                 ignore,
                                                                 slow,
                                                                 cmd))
                              retval=false;
                        }
                  }
               else if(is_accessible_file(mode))
                  {
                     if(has_gnome_mime_command(buffer))
                        catalog_addentry(catalog,
                                         buffer,
                                         filename,
                                         cmd,
                                         NULL/*id_out*/);
                  }
            }
      }

   closedir(dir);

   return retval;
}

static bool catalog_index_applications_recursive(struct catalog *catalog, const char *directory, int maxdepth, bool slow, int cmd)
{
   if(maxdepth==0)
      return true;
   if(maxdepth>0)
      maxdepth--;

   bool retval=true;
   DIR *dir = opendir(directory);
   if(dir==NULL)
      return false;

   struct dirent *dirent;
   while( (dirent=readdir(dir)) != NULL )
      {
         const char *filename = dirent->d_name;
         if(*filename=='.')
            continue;

         char buffer[strlen(directory)+1+strlen(filename)+1];
         strcpy(buffer, directory);
         strcat(buffer, "/");
         strcat(buffer, filename);

         mode_t mode;
         if(getmode(buffer, &mode))
            {
               if(is_accessible_directory(mode))
                  {
                     if(maxdepth!=0)
                        {
                           if(!catalog_index_applications_recursive(catalog,
                                                                    buffer,
                                                                    maxdepth,
                                                                    slow,
                                                                    cmd))
                              retval=false;
                        }
                  }
               else if(is_accessible_file(mode))
                  {
                     if(g_str_has_suffix(filename, ".desktop"))
                        {
                           GError *err = NULL;
                           GnomeDesktopFile *desktopfile = gnome_desktop_file_load(buffer,
                                                                                   NULL/*error*/);
                           if(desktopfile!=NULL)
                              {
                                 char *name = NULL;
                                 gnome_desktop_file_get_string(desktopfile,
                                                               "Desktop Entry",
                                                               "Name",
                                                               &name);
                                 gboolean terminal = false;
                                 gnome_desktop_file_get_boolean(desktopfile,
                                                                "Desktop Entry",
                                                                "Terminal",
                                                                &terminal);

                                 char *exec = NULL;
                                 gnome_desktop_file_get_string(desktopfile,
                                                               "Desktop Entry",
                                                               "Exec",
                                                               &exec);
                                 if(!terminal && exec!=NULL && strstr(exec, "%")==NULL)
                                    {
                                       catalog_addentry(catalog,
                                                        buffer,
                                                        name==NULL ? filename:name,
                                                        cmd,
                                                        NULL/*id_out*/);
                                    }
                                 if(name)
                                    g_free(name);
                                 if(exec)
                                    g_free(exec);

                                 gnome_desktop_file_free(desktopfile);
                              }
                        }
                  }
            }
      }

   closedir(dir);

   return retval;
}


static bool getmode(const char *path, mode_t* mode)
{
   struct stat buf;
   if(stat(path, &buf)==0)
      {
         *mode=buf.st_mode;
         return true;
      }
   return false;
}

static bool is_accessible_directory(mode_t mode)
{
   if(!S_ISDIR(mode))
      return false;
   return is_readable(mode) && is_executable(mode);
}

static bool is_accessible_file(mode_t mode)
{
   if(!S_ISREG(mode))
      return false;
   return is_readable(mode);
}

static bool is_readable(mode_t mode)
{
   return true;
}

static bool is_executable(mode_t mode)
{
   return true;
}

static bool to_ignore(const char *filename, GPatternSpec **patterns)
{
   if(patterns==NULL)
      return false;
   for(int i=0; patterns[i]!=NULL; i++)
      {
         if(g_pattern_match_string(patterns[i], filename))
            return true;
      }
   return false;
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

static bool has_gnome_mime_command(const char *path)
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

   bool retval=false;
   char *mimetype = gnome_vfs_get_mime_type(uri->str);
   if(mimetype)
      {
         char *app = gnome_vfs_mime_get_default_desktop_entry(mimetype);
         if(app)
            {
               g_free(app);
               retval=true;
            }
         g_free(mimetype);
      }

   return retval;
}

static bool bookmarks_read_line(FILE *fh, GString *line)
{
   int buffer_len=256;
   char buffer[buffer_len];
   g_string_truncate(line, 0);
   while(fgets(buffer, buffer_len, fh)!=NULL)
      {
         char *nl = strchr(buffer, '\n');
         if(nl)
            *nl='\0';
         g_string_append(line, buffer);
         if(nl)
            return true;
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
               bool writec=false;
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
                              writec=true;
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
                        writec=true;
                     if(!writec)
                        c=end;
                  }
               else
                  writec=true;
               if(writec)
                  g_string_append_c(retval, *c);
            }
         else
            g_string_append_c(retval, *c);
      }
   char *retval_str = retval->str;
   g_string_free(retval, false/*don't free retval_str*/);
   return retval_str;
}
