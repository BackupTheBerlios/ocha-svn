#include "indexer.h"
#include "indexer_utils.h"
#include "result.h"
#include "ocha_gconf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>

/** index files that can be executed by gnome, using gnome_vfs.
 *
 * This is an implementation of the API defined in indexer.h.
 *
 * Get the indexer using the extern defined in indexer_files.h.
 *
 * @see indexer.h
 */
static struct indexer_source *load(struct indexer *self, struct catalog *catalog, int id);
static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *path, GError **err);
static gboolean validate(struct indexer *self, const char *name, const char *long_name, const char *path);
static gboolean index(struct indexer_source *self, struct catalog *catalog, GError **err);
static gboolean discover(struct indexer *, struct catalog *catalog);
static gboolean has_gnome_mime_command(const char *path);
static char *display_name(struct catalog *catalog, int id);
static void release_source(struct indexer_source *source);
static GtkWidget *editor_widget(struct indexer_source *source, struct catalog *);
#define INDEXER_NAME "files"

/** Definition of the indexer */
struct indexer indexer_files =
{
   .name = INDEXER_NAME,
   .display_name = "Files and Directories",
   .execute = execute,
   .validate = validate,
   .load_source = load,
   .discover = discover,

   .description =
    "This indexer looks recursively into directories for files "
    "that GNOME knows how to open.\n"
    "By default, only the desktop and your home directory are "
    "indexed. You'll probably want to add new sources inside "
    "this indexer for the folders where you save the files you "
    "most often work with."
};

struct source_with_catalog
{
   int source_id;
   struct catalog *catalog;
};
#define DEPTH_INFINITY 10
/* ------------------------- private functions */

/**
 * Load a source from the catalog.
 *
 * This function will look for the attribute 'path', which
 * must be set to the base directory and the attribute 'depth',
 * which might be set (defaults to -1, infinite depth)
 */
static struct indexer_source *load(struct indexer *self, struct catalog *catalog, int id)
{
   g_return_val_if_fail(catalog!=NULL, NULL);
   g_return_val_if_fail(self==&indexer_files, NULL);

   struct indexer_source *retval = g_new(struct indexer_source, 1);
   retval->id=id;
   retval->index=index;
   retval->release=release_source;
   retval->display_name=display_name(catalog, id);
   retval->editor_widget=editor_widget;
   return retval;
}
static void release_source(struct indexer_source *source)
{
    g_return_if_fail(source!=NULL);
    g_free((gpointer)source->display_name);
    g_free(source);
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
static gboolean discover(struct indexer *indexer, struct catalog *catalog)
{
   char *home_Desktop = gnome_util_prepend_user_home("Desktop");
   char *home_dot_gnome_desktop = gnome_util_prepend_user_home(".gnome-desktop");
   const char *home = g_get_home_dir();
   gboolean has_desktop = FALSE;
   gboolean retval = FALSE;

   if(g_file_test(home_Desktop, G_FILE_TEST_EXISTS)
      && g_file_test(home_Desktop, G_FILE_TEST_IS_DIR))
      {
         if(!add_source(catalog, home_Desktop, 2/*depth*/, NULL/*ignore*/))
            goto error;
         has_desktop=TRUE;
      }

   if(g_file_test(home_dot_gnome_desktop, G_FILE_TEST_EXISTS)
      && g_file_test(home_dot_gnome_desktop, G_FILE_TEST_IS_DIR))
      {
         if(!add_source(catalog, home_dot_gnome_desktop, 2/*depth*/, NULL/*ignore*/))
            goto error;
         has_desktop=TRUE;
      }

   if(!has_desktop)
      {
         /* yes, I'm paranoid... */
         if(g_file_test(home, G_FILE_TEST_EXISTS)
            && g_file_test(home, G_FILE_TEST_IS_DIR))
            {
               if(!add_source(catalog, home, 2/*depth*/, "Desktop"))
                  goto error;
            }
      }
   retval=TRUE;
 error:
   g_free(home_Desktop);
   g_free(home_dot_gnome_desktop);
   /* not: g_free(home); */
   return retval;
}
static gboolean index_file_cb(struct catalog *catalog,
                              int source_id,
                              const char *path,
                              const char *filename,
                              GError **err,
                              gpointer userdata)
{

   if(!has_gnome_mime_command(path))
      return TRUE;

   char uri[strlen("file://")+strlen(path)+1];
   strcpy(uri, "file://");
   strcat(uri, path);

   return catalog_addentry_witherrors(catalog,
                                      uri,
                                      filename,
                                      path/*long_name*/,
                                      source_id,
                                      err);
}

/**
 * (re)index the directory of the source
 */
static gboolean index(struct indexer_source *self, struct catalog *catalog, GError **err)
{
   g_return_val_if_fail(self!=NULL, FALSE);
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   return index_recursively(INDEXER_NAME,
                            catalog,
                            self->id,
                            index_file_cb,
                            self/*userdata*/,
                            err);
}

/**
 * Execute a file using gnome_vfs
 *
 * @param name file display name
 * @param long long file display name
 * @param text_uri file:// URL (file://<absolute path>) with three "/", then. (called 'path' elsewhere)
 * @param err error to set if execution failed
 * @return true if it worked
 */
static gboolean execute(struct indexer *self, const char *name, const char *long_name, const char *text_uri, GError **err)
{
   g_return_val_if_fail(text_uri!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);
   g_return_val_if_fail(self==&indexer_files, FALSE);

   if(!validate(self, name, long_name, text_uri))
      {
         g_set_error(err,
                     RESULT_ERROR,
                     RESULT_ERROR_INVALID_RESULT,
                     "file not found: %s",
                     text_uri);
         return FALSE;
      }

   printf("opening %s...\n", text_uri);
   GError *gnome_err = NULL;
   if(!gnome_url_show(text_uri, &gnome_err))
      {
         g_set_error(err,
                     RESULT_ERROR,
                     RESULT_ERROR_MISSING_RESOURCE,
                     "error opening %s: %s",
                     text_uri,
                     gnome_err->message);
         g_error_free(gnome_err);
         return FALSE;
      }
   return TRUE;
}

/**
 * Make sure the file still exists.
 *
 * @param name file display name
 * @param path long file display name, the path
 * @param text_uri file:// URL (file://<absolute path>) with three "/", then. (called 'path' elsewhere)
 */
static gboolean validate(struct indexer *self, const char *name, const char *long_name, const char *text_uri)
{
   g_return_val_if_fail(text_uri!=NULL, FALSE);
   g_return_val_if_fail(self==&indexer_files, FALSE);

   return uri_exists(text_uri);
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
   g_string_free(uri, TRUE/*free content*/);
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
        char *path=NULL;
        if(g_str_has_prefix(uri, "file://"))
            path=&uri[strlen("file://")];
        else if(*uri=='/')
            path=uri;
        if(path)
        {
            const char *home = g_get_home_dir();
            if(strcmp(home, path)==0)
                retval=g_strdup("Home");
            else if(g_str_has_prefix(path, home) && strcmp(&path[strlen(home)], "/Desktop")==0)
                retval=g_strdup("Desktop");
            else if(g_str_has_prefix(path, home) && strcmp(&path[strlen(home)], "/.gnome-desktop")==0)
                retval=g_strdup("GNOME Desktop");
            else if(g_str_has_prefix(path, home))
                retval=g_strdup(&path[strlen(home)+1]);
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
    }
    if(uri)
        g_free(uri);
    return retval;
}

static void free_source_with_catalog_cb(GtkWidget *widget, gpointer userdata)
{
    g_return_if_fail(userdata);
    struct source_with_catalog *source_with_catalog =
        (struct source_with_catalog *)userdata;
    g_free(userdata);
}
static void depth_changed_cb(GtkRange *range, gpointer userdata)
{
    g_return_if_fail(range);
    g_return_if_fail(userdata);
    struct source_with_catalog *source_with_catalog =
        (struct source_with_catalog *)userdata;
    int value = (int)gtk_range_get_value(range);
    if(value>=DEPTH_INFINITY)
       value=-1;
    char *value_as_string = g_strdup_printf("%d", (int)value);
    ocha_gconf_set_source_attribute(INDEXER_NAME,
                                    source_with_catalog->source_id,
                                    "depth",
                                    value_as_string);
    g_free(value_as_string);
}
static void update_depth_label_cb(GtkRange *range, gpointer userdata)
{
    g_return_if_fail(range);
    g_return_if_fail(userdata);
    GtkLabel *label = GTK_LABEL(userdata);

    int value = (int)gtk_range_get_value(range);
    if(value>=DEPTH_INFINITY)
       gtk_label_set_text(label, "\xe2\x88\x9e"/*U+221E INFINITY*/);
    else
       {
          char *value_as_string = g_strdup_printf("%d", value);
          gtk_label_set_text(label, value_as_string);
          g_free(value_as_string);
       }
}
static void exclude_changed_cb(GtkEditable *widget, gpointer userdata)
{
    g_return_if_fail(widget);
    g_return_if_fail(userdata);
    struct source_with_catalog *source_with_catalog =
        (struct source_with_catalog *)userdata;
    const char *text = gtk_entry_get_text(GTK_ENTRY(widget));
    ocha_gconf_set_source_attribute(INDEXER_NAME,
                                    source_with_catalog->source_id,
                                    "ignore",
                                    text);
}
static void include_content_set_attribute_cb(GtkToggleButton *toggle, gpointer userdata)
{
    g_return_if_fail(toggle);
    g_return_if_fail(userdata);
    struct source_with_catalog *source_with_catalog =
        (struct source_with_catalog *)userdata;
    if(gtk_toggle_button_get_active(toggle))
        {
            ocha_gconf_set_source_attribute(INDEXER_NAME,
                                            source_with_catalog->source_id,
                                            "depth",
                                            "1");
        }
    else
        {
            ocha_gconf_set_source_attribute(INDEXER_NAME,
                                            source_with_catalog->source_id,
                                            "depth",
                                            "0");
        }
}
static void include_content_disable_cb(GtkToggleButton *toggle, gpointer userdata)
{
    g_return_if_fail(toggle);
    g_return_if_fail(userdata);
    GtkContainer *parent = GTK_CONTAINER(userdata);
    gboolean active = gtk_toggle_button_get_active(toggle);

    GList *children = gtk_container_get_children(parent);
    for(GList *child=children; child; child=child->next)
        {
            GtkWidget *childw = child->data;
            if(childw==GTK_WIDGET(toggle))
                continue;
            /*                     if(GTK_IS_CONTAINER(childw)) */
            /*                         include_content_disable_cb(toggle, childw); */
            gtk_widget_set_sensitive(childw, active);
        }
    g_list_free(children);
}

static void include_content_reset_depth_cb(GtkToggleButton *toggle, gpointer userdata)
{
    g_return_if_fail(toggle);
    GtkRange *depth = GTK_RANGE(userdata);
    gboolean active = gtk_toggle_button_get_active(toggle);
    if(active)
        {
            /* was inactive, is now active */
            gtk_range_set_value(depth, 1);
        }
}

static GtkWidget *editor_widget(struct indexer_source *source, struct catalog *catalog)
{
    GtkWidget *retval = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(retval);

    char *current_path = NULL;
    char *current_depth = NULL;
    char *current_ignore = NULL;

    current_path=ocha_gconf_get_source_attribute(INDEXER_NAME, source->id, "path");
    current_depth=ocha_gconf_get_source_attribute(INDEXER_NAME, source->id, "depth");
    current_ignore=ocha_gconf_get_source_attribute(INDEXER_NAME, source->id, "ignore");

    int current_depth_i = current_depth ? atoi(current_depth):1;
    if(current_depth_i==-1)
       current_depth_i=DEPTH_INFINITY;

    struct source_with_catalog *source_with_catalog = g_new(struct source_with_catalog, 1);
    source_with_catalog->catalog=catalog;
    source_with_catalog->source_id=source->id;
    g_signal_connect(retval,
                     "destroy",
                     G_CALLBACK(free_source_with_catalog_cb),
                     source_with_catalog/*userdata*/);

    /* --- Top frame: folder */

    GtkWidget *frame_top = gtk_frame_new(NULL);
    gtk_widget_show(frame_top);
    gtk_box_pack_start(GTK_BOX(retval), frame_top, FALSE/*!expand*/, TRUE/*fill*/, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame_top), GTK_SHADOW_NONE);
    GtkWidget *frame_top_label = gtk_label_new("<b>Folder</b>");
    gtk_widget_show(frame_top_label);
    gtk_frame_set_label_widget(GTK_FRAME(frame_top), frame_top_label);
    gtk_label_set_use_markup(GTK_LABEL(frame_top_label), TRUE);

    GtkWidget *align = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_widget_show(align);
    gtk_container_add(GTK_CONTAINER(frame_top), align);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 12, 0);

    GtkWidget *top_table = gtk_table_new(1/*rows*/,
                                         2/*columns*/,
                                         FALSE);
    gtk_widget_show(top_table);
    gtk_container_add(GTK_CONTAINER(align), top_table);

    GtkWidget *pathLabel = gtk_label_new("Path: ");
    gtk_widget_show(pathLabel);
    gtk_table_attach(GTK_TABLE(top_table),
                     pathLabel, 0, 1, 0, 1,
                     GTK_FILL, 0, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(pathLabel), 0, 0.5);
    gtk_misc_set_padding(GTK_MISC(pathLabel), 4, 0);

    GtkWidget *path = gtk_label_new(current_path ? current_path:"");
    gtk_widget_show(path);
    gtk_table_attach(GTK_TABLE(top_table), path, 1, 2, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(path), 0, 0.5);
    gtk_misc_set_padding(GTK_MISC(path), 4, 0);

    /* --- Bottom frame: Indexing */

    GtkWidget *frame_bottom = gtk_frame_new(NULL);
    gtk_widget_show(frame_bottom);
    gtk_box_pack_start(GTK_BOX(retval), frame_bottom, TRUE/*expand*/, TRUE/*fill*/, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame_bottom), GTK_SHADOW_NONE);
    GtkWidget *frame_bottom_label = gtk_label_new("<b>Indexing</b>");
    gtk_widget_show(frame_bottom_label);
    gtk_frame_set_label_widget(GTK_FRAME(frame_bottom), frame_bottom_label);
    gtk_label_set_use_markup(GTK_LABEL(frame_bottom_label), TRUE);

    align = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_widget_show(align);
    gtk_container_add(GTK_CONTAINER(frame_bottom), align);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 12, 0);

    GtkWidget *bottom_table = gtk_table_new(5, 3, FALSE);
    gtk_widget_show(bottom_table);
    gtk_container_add(GTK_CONTAINER(align), bottom_table);
    gtk_container_set_border_width(GTK_CONTAINER(bottom_table), 4);

    GtkWidget *include_label = gtk_label_new("Include: ");
    gtk_widget_show(include_label);
    gtk_table_attach(GTK_TABLE(bottom_table), include_label, 0, 1, 1, 2,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    gtk_misc_set_alignment(GTK_MISC(include_label), 0, 0.5);

    GtkWidget *exclude_label = gtk_label_new("Exclude:");
    gtk_widget_show(exclude_label);
    gtk_table_attach(GTK_TABLE(bottom_table), exclude_label, 0, 1, 2, 3,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    gtk_misc_set_alignment(GTK_MISC(exclude_label), 0, 0.5);

    GtkWidget *include_contents = gtk_check_button_new_with_mnemonic("Include Contents");
    gtk_widget_show(include_contents);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(include_contents),
                                 current_depth_i>0);
    gtk_table_attach(GTK_TABLE(bottom_table), include_contents, 0, 2, 0, 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    g_signal_connect(include_contents,
                     "toggled",
                     G_CALLBACK(include_content_set_attribute_cb),
                     source_with_catalog/*userdata*/);
    g_signal_connect(include_contents,
                     "toggled",
                     G_CALLBACK(include_content_disable_cb),
                     bottom_table/*userdata*/);

    GtkWidget *depth_label = gtk_label_new("Depth");
    gtk_widget_show(depth_label);
    gtk_table_attach(GTK_TABLE(bottom_table), depth_label, 2, 3, 0, 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(depth_label), GTK_JUSTIFY_CENTER);

    GtkWidget *depth_value = gtk_label_new("");
    gtk_widget_show(depth_value);
    gtk_table_attach(GTK_TABLE(bottom_table), depth_value, 2, 3, 1, 2,
                     (GtkAttachOptions)(0),
                     (GtkAttachOptions)(0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(depth_value), GTK_JUSTIFY_CENTER);

    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs,
                           pango_attr_scale_new(PANGO_SCALE_SMALL));
    gtk_label_set_attributes(GTK_LABEL(depth_value), attrs);
    pango_attr_list_unref(attrs);

    GtkWidget *include = gtk_entry_new();
    gtk_widget_show(include);
    gtk_table_attach(GTK_TABLE(bottom_table), include, 1, 2, 1, 2,
                     (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions)(0), 4, 0);

    GtkWidget *exclude = gtk_entry_new();
    gtk_widget_show(exclude);
    gtk_table_attach(GTK_TABLE(bottom_table), exclude, 1, 2, 2, 3,
                     (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions)(0), 4, 0);
    if(current_ignore)
        gtk_entry_set_text(GTK_ENTRY(exclude), current_ignore);
    g_signal_connect(exclude,
                     "changed",
                     G_CALLBACK(exclude_changed_cb),
                     source_with_catalog/*userdata*/);

    GtkWidget *depth = gtk_vscale_new_with_range(1.0/*min*/,
                                                 DEPTH_INFINITY/*max*/,
                                                 1.0/*step*/);
    gtk_widget_show(depth);
    gtk_table_attach(GTK_TABLE(bottom_table), depth, 2, 3, 2, 5,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);
    gtk_scale_set_digits(GTK_SCALE(depth), 0);
    gtk_scale_set_draw_value(GTK_SCALE(depth), FALSE);
    gtk_range_set_value(GTK_RANGE(depth), current_depth_i==0 ? 1:current_depth_i);
    update_depth_label_cb(GTK_RANGE(depth), depth_value);

    g_signal_connect(depth,
                     "value-changed",
                     G_CALLBACK(depth_changed_cb),
                     source_with_catalog/*userdata*/);
    g_signal_connect(depth,
                     "value-changed",
                     G_CALLBACK(update_depth_label_cb),
                     depth_value/*userdata*/);
    g_signal_connect(include_contents,
                     "toggled",
                     G_CALLBACK(include_content_reset_depth_cb),
                     depth/*userdata*/);

    GtkWidget *filler= gtk_label_new("");
    gtk_widget_show(filler);
    gtk_table_attach(GTK_TABLE(bottom_table), filler, 1, 2, 4, 5,
                     (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions)(GTK_EXPAND), 0, 0);
    gtk_misc_set_alignment(GTK_MISC(filler), 0, 0.5);

    GtkWidget *explanation = gtk_label_new("Fill include/exclude with comma-separated "
                                            "patterns to be applied to files or directories "
                                            "inside the current folder.\n"
                                            "Example: <i>*.txt,*.gif,*.jpg</i>");
    gtk_widget_show(explanation);
    gtk_table_attach(GTK_TABLE(bottom_table), explanation, 1, 2, 3, 4,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    gtk_label_set_use_markup(GTK_LABEL(explanation), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(explanation), TRUE);
    gtk_misc_set_alignment(GTK_MISC(explanation), 0, 0);
    gtk_misc_set_padding(GTK_MISC(explanation), 4, 0);

    if(current_path)
        g_free(current_path);
    if(current_depth)
        g_free(current_depth);
    if(current_ignore)
        g_free(current_ignore);

    include_content_disable_cb(GTK_TOGGLE_BUTTON(include_contents), bottom_table);

    return retval;
}
