
/** \files
 * Implementation of an indexer_view for indexer_files,
 * usually created from indexer_files->new_view()
 */

#include "indexer_files_view.h"
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

/** For the UI, treat this number as infinity (-1) */
#define DEPTH_INFINITY 10
#define INDEXER_NAME "files"

/**
 * Structure for the views created in this module
 */
struct indexer_files_view
{
        struct indexer_source_view base;
        /** Disable most callbacks when this is true */
        gboolean disable_cb;

        /** The label that displays the current path */
        GtkEntry *path;
        /** The button that opens the chooser dialog */
        GtkButton *choose_button;
        /** The file chooser dialog */
        GtkFileChooser *choose;
        /** Include content checkbox */
        GtkToggleButton *include_contents;
        /** The label displaying the current depth value */
        GtkLabel *depth_value;
        /** The scale that lets one choose the depth */
        GtkRange *depth;
        /** The entry box for 'Includes' */
        GtkEntry *includes;
        /** The entry boy for 'Excludes' */
        GtkEntry *excludes;
        /** The root of the widgets that should be disabled if include_contents==false */
        GtkContainer *contentWidgetsRoot;
};

/* ------------------------- prototypes: indexer_files_view */
static void indexer_files_view_attach(struct indexer_source_view *view, struct indexer_source *source);
static void indexer_files_view_detach(struct indexer_source_view *view);
static void indexer_files_view_release(struct indexer_source_view *view);

/* ------------------------- prototypes: other */
static GtkWidget *create_widget(struct indexer_files_view *view);
static void update_widgets(struct indexer_files_view *view);
static void update_from_text_fields(struct indexer_files_view *view);
static void connect_widgets(struct indexer_files_view *view);
static void depth_changed_cb(GtkRange *range, gpointer userdata);
static void update_depth_label_cb(GtkRange *range, gpointer userdata);
static void exclude_changed_cb(GtkEditable *widget, gpointer userdata);
static void include_content_set_attribute_cb(GtkToggleButton *toggle, gpointer userdata);
static void recursive_set_sensitive(GtkContainer *parent, gboolean active, GtkWidget *exclude);
static void include_content_disable_cb(GtkToggleButton *toggle, gpointer userdata);
static void include_content_reset_depth_cb(GtkToggleButton *toggle, gpointer userdata);
static void choose_file_cb(GtkButton *toggle, gpointer userdata);
static gboolean update_path_on_focus_out_cb(GtkWidget *widget, GdkEvent *ev, gpointer userdata);
static void update_path(struct indexer_files_view *view);
/* ------------------------- public functions */

struct indexer_source_view *indexer_files_view_new(struct indexer *indexer)
{
        struct indexer_files_view *retval;
        g_return_val_if_fail(indexer!=NULL, NULL);

        retval=g_new(struct indexer_files_view, 1);
        memset(retval, 0, sizeof(struct indexer_files_view));

        retval->base.indexer = indexer;
        retval->base.source_id=-1;
        retval->disable_cb=TRUE;
        retval->base.widget = create_widget(retval);
        g_object_ref(retval->base.widget);
        gtk_widget_show(retval->base.widget);

        retval->base.attach=indexer_files_view_attach;
        retval->base.detach=indexer_files_view_detach;
        retval->base.release=indexer_files_view_release;

        update_widgets(retval);
        connect_widgets(retval);

        return &retval->base;
}
/* ------------------------- member function (indexer_files_view) */
static void indexer_files_view_attach(struct indexer_source_view *_view, struct indexer_source *source)
{
        struct indexer_files_view *view;
        view=(struct indexer_files_view *)_view;

        g_return_if_fail(_view!=NULL);
        g_return_if_fail(source!=NULL);

        if(view->base.source_id==source->id) {
                return;
        }

        if(view->base.source_id>0) {
                update_from_text_fields(view);
        }

        view->disable_cb=TRUE;
        view->base.source_id=source->id;
        update_widgets(view);
        view->disable_cb=FALSE;
}

static void indexer_files_view_detach(struct indexer_source_view *_view)
{
        struct indexer_files_view *view;
        view=(struct indexer_files_view *)_view;

        g_return_if_fail(view!=NULL);
        view->disable_cb=TRUE;
        if(view->base.source_id>0) {
                update_from_text_fields(view);
                view->base.source_id=-1;
                update_widgets(view);
        }
}

static void indexer_files_view_release(struct indexer_source_view *_view)
{
        struct indexer_files_view *view;
        view=(struct indexer_files_view *)_view;

        g_return_if_fail(view!=NULL);
        indexer_files_view_detach(_view);
        if(view->choose) {
                gtk_widget_destroy(GTK_WIDGET(view->choose));
        }
        g_object_unref(view->base.widget);
        g_free(view);
}



/* ------------------------- static functions */
static GtkWidget *create_widget(struct indexer_files_view *view)
{
        GtkWidget *retval;
        GtkWidget *frame_top;
        GtkWidget *frame_top_label;
        GtkWidget *align;
        GtkWidget *top_table;
        GtkWidget *path;
        GtkWidget *choose_button;
        GtkWidget *frame_bottom;
        GtkWidget *frame_bottom_label;
        GtkWidget *bottom_table;
        GtkWidget *include_label;
        GtkWidget *exclude_label;
        GtkWidget *include_contents;
        GtkWidget *depth_label;
        GtkWidget *depth_value;
        PangoAttrList *attrs;
        GtkWidget *include;
        GtkWidget *exclude;
        GtkWidget *depth;
        GtkWidget *filler;
        GtkWidget *explanation;

        retval =  gtk_vbox_new(FALSE, 0);
        gtk_widget_show(retval);

        /* --- Top frame: folder */

        frame_top =  gtk_frame_new(NULL);
        gtk_widget_show(frame_top);
        gtk_box_pack_start(GTK_BOX(retval),
                           frame_top,
                           FALSE/*!expand*/,
                           TRUE/*fill*/,
                           0);
        gtk_frame_set_shadow_type(GTK_FRAME(frame_top), GTK_SHADOW_NONE);
        frame_top_label =  gtk_label_new("<b>Path</b>");
        gtk_widget_show(frame_top_label);
        gtk_frame_set_label_widget(GTK_FRAME(frame_top), frame_top_label);
        gtk_label_set_use_markup(GTK_LABEL(frame_top_label), TRUE);

        align =  gtk_alignment_new(0.5, 0.5, 1, 1);
        gtk_widget_show(align);
        gtk_container_add(GTK_CONTAINER(frame_top), align);
        gtk_alignment_set_padding(GTK_ALIGNMENT(align),
                                  0, 0, 12, 0);

        top_table =  gtk_table_new(1/*rows*/,
                                   2/*columns*/,
                                   FALSE);
        gtk_widget_show(top_table);
        gtk_container_add(GTK_CONTAINER(align), top_table);

        path =  gtk_entry_new();
        gtk_widget_show(path);
        gtk_table_attach(GTK_TABLE(top_table), path, 0, 1, 0, 1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        choose_button = gtk_button_new_with_label("Choose");
        gtk_widget_show(choose_button);
        gtk_table_attach(GTK_TABLE(top_table), choose_button, 1, 2, 0, 1,
                         0, 0, 0, 0);

        /* --- Bottom frame: Indexing */

        frame_bottom =  gtk_frame_new(NULL);
        gtk_widget_show(frame_bottom);
        gtk_box_pack_start(GTK_BOX(retval),
                           frame_bottom,
                           TRUE/*expand*/,
                           TRUE/*fill*/,
                           0);
        gtk_frame_set_shadow_type(GTK_FRAME(frame_bottom), GTK_SHADOW_NONE);
        frame_bottom_label =  gtk_label_new("<b>Indexing</b>");
        gtk_widget_show(frame_bottom_label);
        gtk_frame_set_label_widget(GTK_FRAME(frame_bottom), frame_bottom_label);
        gtk_label_set_use_markup(GTK_LABEL(frame_bottom_label), TRUE);

        align = gtk_alignment_new(0.5, 0.5, 1, 1);
        gtk_widget_show(align);
        gtk_container_add(GTK_CONTAINER(frame_bottom), align);
        gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 12, 0);

        bottom_table =  gtk_table_new(5, 3, FALSE);
        gtk_widget_show(bottom_table);
        gtk_container_add(GTK_CONTAINER(align), bottom_table);
        gtk_container_set_border_width(GTK_CONTAINER(bottom_table), 4);

        include_label =  gtk_label_new("Include: ");
        gtk_widget_show(include_label);
        gtk_table_attach(GTK_TABLE(bottom_table), include_label, 0, 1, 1, 2,
                         (GtkAttachOptions)(GTK_FILL),
                         (GtkAttachOptions)(0), 0, 0);
        gtk_misc_set_alignment(GTK_MISC(include_label), 0, 0.5);

        exclude_label =  gtk_label_new("Exclude:");
        gtk_widget_show(exclude_label);
        gtk_table_attach(GTK_TABLE(bottom_table), exclude_label, 0, 1, 2, 3,
                         (GtkAttachOptions)(GTK_FILL),
                         (GtkAttachOptions)(0), 0, 0);
        gtk_misc_set_alignment(GTK_MISC(exclude_label), 0, 0.5);

        include_contents =  gtk_check_button_new_with_mnemonic("Include Contents");
        gtk_widget_show(include_contents);
        gtk_table_attach(GTK_TABLE(bottom_table), include_contents, 0, 2, 0, 1,
                         (GtkAttachOptions)(GTK_FILL),
                         (GtkAttachOptions)(0), 0, 0);
        depth_label =  gtk_label_new("Depth");
        gtk_widget_show(depth_label);
        gtk_table_attach(GTK_TABLE(bottom_table), depth_label, 2, 3, 0, 1,
                         (GtkAttachOptions)(GTK_FILL),
                         (GtkAttachOptions)(0), 0, 0);
        gtk_label_set_justify(GTK_LABEL(depth_label), GTK_JUSTIFY_CENTER);

        depth_value =  gtk_label_new("");
        gtk_widget_show(depth_value);
        gtk_table_attach(GTK_TABLE(bottom_table), depth_value, 2, 3, 1, 2,
                         (GtkAttachOptions)(0),
                         (GtkAttachOptions)(0), 0, 0);
        gtk_label_set_justify(GTK_LABEL(depth_value), GTK_JUSTIFY_CENTER);

        attrs =  pango_attr_list_new();
        pango_attr_list_insert(attrs,
                               pango_attr_scale_new(PANGO_SCALE_SMALL));
        gtk_label_set_attributes(GTK_LABEL(depth_value), attrs);
        pango_attr_list_unref(attrs);

        include =  gtk_entry_new();
        gtk_widget_show(include);
        gtk_table_attach(GTK_TABLE(bottom_table), include, 1, 2, 1, 2,
                         (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                         (GtkAttachOptions)(0), 4, 0);

        exclude =  gtk_entry_new();
        gtk_widget_show(exclude);
        gtk_table_attach(GTK_TABLE(bottom_table), exclude, 1, 2, 2, 3,
                         (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                         (GtkAttachOptions)(0), 4, 0);
        depth =  gtk_vscale_new_with_range(1.0/*min*/,
                                           DEPTH_INFINITY/*max*/,
                                           1.0/*step*/);
        gtk_widget_show(depth);
        gtk_table_attach(GTK_TABLE(bottom_table), depth, 2, 3, 2, 5,
                         (GtkAttachOptions)(GTK_FILL),
                         (GtkAttachOptions)(GTK_FILL), 0, 0);
        gtk_scale_set_digits(GTK_SCALE(depth), 0);
        gtk_scale_set_draw_value(GTK_SCALE(depth), FALSE);

        filler =  gtk_label_new("");
        gtk_widget_show(filler);
        gtk_table_attach(GTK_TABLE(bottom_table), filler, 1, 2, 4, 5,
                         (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                         (GtkAttachOptions)(GTK_EXPAND), 0, 0);
        gtk_misc_set_alignment(GTK_MISC(filler), 0, 0.5);

        explanation =  gtk_label_new("Fill include/exclude with comma-separated "
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

        view->path=GTK_ENTRY(path);
        view->include_contents=GTK_TOGGLE_BUTTON(include_contents);
        view->choose_button=GTK_BUTTON(choose_button);
        view->depth_value=GTK_LABEL(depth_value);
        view->depth=GTK_RANGE(depth);
        view->includes=GTK_ENTRY(include);
        view->excludes=GTK_ENTRY(exclude);
        view->contentWidgetsRoot=GTK_CONTAINER(bottom_table);
        return retval;
}

static void connect_widgets(struct indexer_files_view  *view)
{
        g_signal_connect(view->include_contents,
                         "toggled",
                         G_CALLBACK(include_content_set_attribute_cb),
                         view);
        g_signal_connect(view->include_contents,
                         "toggled",
                         G_CALLBACK(include_content_disable_cb),
                         view);
        g_signal_connect(view->excludes,
                         "changed",
                         G_CALLBACK(exclude_changed_cb),
                         view);
        g_signal_connect(view->depth,
                         "value-changed",
                         G_CALLBACK(depth_changed_cb),
                         view);
        g_signal_connect(view->depth,
                         "value-changed",
                         G_CALLBACK(update_depth_label_cb),
                         view);
        g_signal_connect(view->include_contents,
                         "toggled",
                         G_CALLBACK(include_content_reset_depth_cb),
                         view);

        g_signal_connect(view->choose_button,
                         "clicked",
                         G_CALLBACK(choose_file_cb),
                         view);

        g_signal_connect(view->path,
                         "focus-out-event",
                         G_CALLBACK(update_path_on_focus_out_cb),
                         view);
}


static void update_widgets(struct indexer_files_view *view)
{
        const char *indexer_name;
        char *current_path;
        char *current_depth;
        char *current_ignore;
        int current_depth_i;
        int source_id;
        gboolean system;
        gboolean readonly;

        indexer_name=view->base.indexer->name;
        source_id=view->base.source_id;

        if(source_id<=0) {
                current_path = NULL;
                current_ignore = NULL;
                current_depth = NULL;
                system = FALSE;
                readonly = TRUE;
        } else {
                current_path = ocha_gconf_get_source_attribute(indexer_name,
                                                               source_id,
                                                               "path");
                current_ignore = ocha_gconf_get_source_attribute(indexer_name,
                                                                 source_id,
                                                                 "ignore");

                current_depth = ocha_gconf_get_source_attribute(indexer_name,
                                                                source_id,
                                                                "depth");
                system = ocha_gconf_is_system(indexer_name, source_id);
                readonly = system;
        }

        current_depth_i =  current_depth ? atoi(current_depth):1;
        if(current_depth_i==-1) {
                current_depth_i=DEPTH_INFINITY;
        }

        gtk_entry_set_text(view->path, current_path ? current_path:"");
        gtk_toggle_button_set_active(view->include_contents,
                                     current_depth_i>0);
        gtk_entry_set_text(view->excludes, current_ignore ? current_ignore:"");
        gtk_range_set_value(view->depth, current_depth_i==0 ? 1:current_depth_i);

        if(current_path) {
                g_free(current_path);
        }
        if(current_depth) {
                g_free(current_depth);
        }
        if(current_ignore) {
                g_free(current_ignore);
        }

        if(readonly) {
                gtk_widget_set_sensitive(GTK_WIDGET(view->base.widget), FALSE);
        } else {
                gtk_widget_set_sensitive(GTK_WIDGET(view->base.widget), TRUE);
        }
}

static void update_from_text_fields(struct indexer_files_view *view)
{
        update_path(view);
}

static void depth_changed_cb(GtkRange *range, gpointer userdata)
{
        int source_id;
        int value;
        char *value_as_string;
        struct indexer_files_view *view;

        g_return_if_fail(range);
        g_return_if_fail(userdata);
        view = (struct indexer_files_view *)userdata;
        if(view->disable_cb) {
                return;
        }

        source_id =  view->base.source_id;
        if(source_id<=0) {
                return;
        }

        value =  (int)gtk_range_get_value(range);
        if(value>=DEPTH_INFINITY) {
                value=-1;
        }
        value_as_string =  g_strdup_printf("%d", (int)value);
        ocha_gconf_set_source_attribute(INDEXER_NAME,
                                        source_id,
                                        "depth",
                                        value_as_string);
        g_free(value_as_string);
}
static void update_depth_label_cb(GtkRange *range, gpointer userdata)
{
        GtkLabel *label;
        int value;
        struct indexer_files_view *view;

        g_return_if_fail(range);
        g_return_if_fail(userdata);
        view = (struct indexer_files_view *)userdata;

        label =  view->depth_value;
        value =  (int)gtk_range_get_value(range);
        if(value>=DEPTH_INFINITY) {
                gtk_label_set_text(label, "\xe2\x88\x9e"/*U+221E INFINITY*/);
        } else {
                char *value_as_string = g_strdup_printf("%d", value);
                gtk_label_set_text(label, value_as_string);
                g_free(value_as_string);
        }
}
static void exclude_changed_cb(GtkEditable *widget, gpointer userdata)
{
        int source_id;
        const char *text;
        struct indexer_files_view *view;

        g_return_if_fail(widget);
        g_return_if_fail(userdata);
        view = (struct indexer_files_view *)userdata;
        if(view->disable_cb) {
                return;
        }

        source_id = view->base.source_id;
        if(source_id<=0) {
                return;
        }
        text =  gtk_entry_get_text(GTK_ENTRY(widget));
        ocha_gconf_set_source_attribute(INDEXER_NAME,
                                        source_id,
                                        "ignore",
                                        text);
}
static void include_content_set_attribute_cb(GtkToggleButton *toggle, gpointer userdata)
{
        int source_id;
        struct indexer_files_view *view;

        g_return_if_fail(toggle);
        g_return_if_fail(userdata);
        view = (struct indexer_files_view *)userdata;
        if(view->disable_cb) {
                return;
        }


        source_id = view->base.source_id;
        if(source_id<=0) {
                return;
        }

        if(gtk_toggle_button_get_active(toggle)) {
                ocha_gconf_set_source_attribute(INDEXER_NAME,
                                                source_id,
                                                "depth",
                                                "1");
        } else {
                ocha_gconf_set_source_attribute(INDEXER_NAME,
                                                source_id,
                                                "depth",
                                                "0");
        }
}

static void recursive_set_sensitive(GtkContainer *parent, gboolean active, GtkWidget *exclude)
{
        GList *children, *child;

        g_return_if_fail(parent);

        children =  gtk_container_get_children(parent);
        for(child=children; child; child=child->next) {
                GtkWidget *childw = child->data;
                if(childw==exclude) {
                        continue;
                }
                if(GTK_IS_CONTAINER(childw)) {
                        recursive_set_sensitive(GTK_CONTAINER(childw), active, exclude);
                }
                gtk_widget_set_sensitive(childw, active);
        }
        g_list_free(children);
}

static void include_content_disable_cb(GtkToggleButton *toggle, gpointer userdata)
{
        GtkContainer *parent;
        gboolean active;
        struct indexer_files_view *view;

        g_return_if_fail(toggle);
        g_return_if_fail(userdata);
        view = (struct indexer_files_view *)userdata;

        parent =  view->contentWidgetsRoot;
        active =  gtk_toggle_button_get_active(toggle);

        recursive_set_sensitive(parent, active, GTK_WIDGET(toggle)/*exclude*/);
}

static void include_content_reset_depth_cb(GtkToggleButton *toggle, gpointer userdata)
{
        GtkRange *depth;
        gboolean active;
        struct indexer_files_view *view;

        g_return_if_fail(toggle);
        g_return_if_fail(userdata);
        view = (struct indexer_files_view *)userdata;

        depth =  GTK_RANGE(view->depth);
        active =  gtk_toggle_button_get_active(toggle);
        if(active) {
                /* was inactive, is now active */
                gtk_range_set_value(depth, 1);
        }
}

static void choose_file_cb(GtkButton *button, gpointer userdata)
{
        struct indexer_files_view *view;
        gboolean changed;
        char *old;

        g_return_if_fail(userdata);
        view = (struct indexer_files_view *)userdata;
        if(view->disable_cb) {
                return;
        }
        if(view->base.source_id<=0) {
                return;
        }

        if(view->choose==NULL) {
                GtkWidget *dialog;
                dialog=gtk_file_chooser_dialog_new("",
                                                   GTK_WINDOW(gtk_widget_get_ancestor(GTK_WIDGET(button),
                                                                                      GTK_TYPE_WINDOW)),
                                                   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                   GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                   NULL);
                view->choose=GTK_FILE_CHOOSER(dialog);
        }
        old = ocha_gconf_get_source_attribute(INDEXER_NAME,
                                              view->base.source_id,
                                              "path");
        if(old) {
                gtk_file_chooser_set_filename(view->choose, old);
                g_free(old);
        }
        changed = gtk_dialog_run (GTK_DIALOG(view->choose))== GTK_RESPONSE_ACCEPT;
        gtk_widget_hide(GTK_WIDGET(view->choose));

        if (changed) {
                char *filename;
                int source_id = view->base.source_id;

                filename = gtk_file_chooser_get_filename (view->choose);
                ocha_gconf_set_source_attribute(INDEXER_NAME,
                                                source_id,
                                                "path",
                                                filename);
                gtk_entry_set_text(view->path, filename ? filename:"");

                printf("%s:%d: filename: %s\n", /*@nocommit@*/
                       __FILE__,
                       __LINE__,
                       filename
                       );

                if(filename && g_file_test(filename, G_FILE_TEST_IS_DIR)) {
                        char *old_depth;

                        printf("%s:%d: is dir\n", /*@nocommit@*/
                               __FILE__,
                               __LINE__
                               );

                        old_depth=ocha_gconf_get_source_attribute(INDEXER_NAME,
                                                                  source_id,
                                                                  "depth");
                        printf("%s:%d: depth: '%s'\n", /*@nocommit@*/
                               __FILE__,
                               __LINE__,
                               old_depth
                               );

                        if(old_depth || strcmp("0", old_depth)==0) {
                                printf("%s:%d: set source to 3\n", /*@nocommit@*/
                                       __FILE__,
                                       __LINE__
                                       );

                                ocha_gconf_set_source_attribute(INDEXER_NAME,
                                                                source_id,
                                                                "depth",
                                                                "3");
                                update_widgets(view);
                        }
                        if(old_depth) {
                                g_free(old_depth);
                        }
                }

                g_free (filename);
        }
}
static gboolean update_path_on_focus_out_cb(GtkWidget *widget, GdkEvent *ev, gpointer userdata)
{
        struct indexer_files_view *view;

        g_return_val_if_fail(userdata, FALSE);
        view = (struct indexer_files_view *)userdata;
        if(!view->disable_cb) {
                update_path(view);
        }
        return FALSE;
}
static void update_path(struct indexer_files_view *view)
{
        const char *path;
        char *oldpath;
        g_return_if_fail(view);
        if(view->base.source_id<=0) {
                return;
        }
        path = gtk_entry_get_text(view->path);
        oldpath = ocha_gconf_get_source_attribute(INDEXER_NAME,
                                                  view->base.source_id,
                                                  "path");
        if(!oldpath || strcmp(oldpath, path)!=0) {
                ocha_gconf_set_source_attribute(INDEXER_NAME,
                                                view->base.source_id,
                                                "path",
                                                path);
        }
        if(oldpath) {
                g_free(oldpath);
        }
}
