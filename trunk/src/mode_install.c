#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mode_install.h"
#include "indexer.h"
#include "indexers.h"
#include "ocha_init.h"
#include "ocha_gconf.h"
#include <libgnomeui/libgnomeui.h>
#include <libgnome/libgnome.h>

/** \file Implement the API defined in first_time.h
 *
 */

#define MAIN_TITLE "Ocha - First Time Configuration"
#define START_TEXT "You have started Ocha for the first time. Before you can " \
                    "use it to quickly search program, files and web links, " \
                    "ocha needs to go through your files and create an index.\n\n" \
                    "Indexing should be pretty quick, depending on how many files " \
                    "you have on your hard disk. This will be done only once. Afterwards, " \
                    "the index will be updated automatically in the background."
#define FINISH_TEXT "You can now start using Ocha.\n\n" \
                   "Try it: press Alt-Space, type 'ocha' and then press Enter. This will start " \
                   "Ocha's preference screen.\n\nYou can launch any program, visit any web link in " \
                   "the same way. Press Alt-Space followed by some part of the program name or " \
                   "link title. Use the arrow key to select the program or web link you meant."
#define FAILED_TEXT "Indexing was successfull, but the deamon could not be started.\n\n" \
                    "Please have the system administrator of this machine check whether ocha was " \
                    "installed properly on your system. The daemon should be available at the following " \
                    "location:\n " BINDIR "/ochad\nbut it is missing or not executable.\n\n" \
                    "Once this is resolved, try and start Ocha again."

struct first_time
{
        GnomeDruid *druid;
        struct catalog *catalog;
        gboolean indexed;
        guint timeout_id;
        gint pid;
        GSList *steps;
        GSList *current_step;
        GtkProgressBar *progress;
        /** One step is worth that much progress */
        gdouble progress_step;
        /** Current progress value */
        gdouble progress_current;
        GnomeDruidPage *last_page;
};

struct step
{
        struct indexer *indexer;
        int source_id;
};

/* ------------------------- prototypes: static functions */
static int do_install(struct configuration *config);
static void setup_druid(struct first_time *data);
static GnomeDruidPage *first_page(struct first_time *data);
static GnomeDruidPage *indexing_page(struct first_time *data);
static GnomeDruidPage *last_page(struct first_time *data);
static gboolean indexing(gpointer userdata);
static void indexing_page_prepare(GnomeDruidPage *page, GnomeDruid *druid, gpointer userdata);
static void last_page_prepare(GnomeDruidPage *page, GnomeDruid *druid, gpointer userdata);
static gboolean last_page_setup(gpointer userdata);
static void discover(struct catalog *catalog);
static GSList *create_steps(struct catalog *catalog, guint *count);
static void handle_errors(struct first_time *data, GError *err);
static void set_default_configuration(void);

/* ------------------------- definitions */

/* ------------------------- public functions */

void mode_install_if_necessary(struct configuration *config)
{
        if(ocha_gconf_exists()) {
                return;
        }
        exit(do_install(config));
}

int mode_install(int argc, char *argv[])
{
        struct configuration config;

        ocha_init(PACKAGE, argc, argv, TRUE/*gui*/, &config);

        return do_install(&config);
}

/* ------------------------- static functions */
static int do_install(struct configuration  *config) {
        GError *err = NULL;
        struct catalog  *catalog;
        struct first_time data;

        catalog =  catalog_connect(config->catalog_path, &err);
        if(catalog==NULL) {
                fprintf(stderr, "error: could not open or create catalog at '%s': %s\n",
                        config->catalog_path,
                        err->message);
                exit(114);
        }

        memset(&data, 0, sizeof(struct first_time));
        data.catalog=catalog;

        set_default_configuration();
        setup_druid(&data);

        gtk_main();

        if(data.timeout_id) {
                g_source_remove(data.timeout_id);
        }

        catalog_timestamp_update(catalog);
        catalog_disconnect(catalog);

        if(!data.indexed) {
                unlink(config->catalog_path);
                return 82;
        }
        return 0;
}

static void setup_druid(struct first_time *data)
{
        GnomeDruid *druid;
        GtkWidget *window=NULL;

        druid = GNOME_DRUID(gnome_druid_new());
        data->druid=druid;

        gnome_druid_append_page(druid, first_page(data));
        gnome_druid_append_page(druid, indexing_page(data));
        gnome_druid_append_page(druid, last_page(data));

        gtk_widget_show(GTK_WIDGET(druid));

        gnome_druid_construct_with_window(druid,
                                          MAIN_TITLE,
                                          NULL/*parent*/,
                                          TRUE/*close_on_cancel*/,
                                          &window);

        g_signal_connect(window,
                         "delete-event",
                         G_CALLBACK(gtk_main_quit),
                         NULL/*userdata*/);

        gtk_widget_show(GTK_WIDGET(window));

}

static GnomeDruidPage *first_page(struct first_time *data)
{
        GtkWidget *page;

        page = gnome_druid_page_edge_new_with_vals(GNOME_EDGE_START,
                                                   TRUE/*antialiased*/,
                                                   MAIN_TITLE,
                                                   START_TEXT,
                                                   NULL/*logo*/,
                                                   NULL/*watermark*/,
                                                   NULL/*top_watermark*/);
        gtk_widget_show(page);

        g_signal_connect(data->druid,
                         "cancel",
                         G_CALLBACK(gtk_main_quit),
                         NULL);

        return GNOME_DRUID_PAGE(page);
}

static GnomeDruidPage *indexing_page(struct first_time *data)
{
        GnomeDruidPageStandard *page;
        GtkWidget *vbox;
        GtkWidget *label;
        GtkWidget *progress;

        page = GNOME_DRUID_PAGE_STANDARD(gnome_druid_page_standard_new());
        gnome_druid_page_standard_set_title(page, MAIN_TITLE " - Indexing");

        vbox=page->vbox;

        g_signal_connect(page,
                         "prepare",
                         G_CALLBACK(indexing_page_prepare),
                         data/*userdata*/);


        label = gtk_label_new("Ocha is indexing your files and links for the first time. Please wait...");
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_show(GTK_WIDGET(label));
        gtk_box_pack_start(GTK_BOX(vbox),
                           label,
                           TRUE/*expand*/,
                           TRUE/*fill*/,
                           12/*padding*/);


        progress = gtk_progress_bar_new();
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0.0);
        data->progress = GTK_PROGRESS_BAR(progress);
        gtk_widget_show(progress);
        gtk_box_pack_end(GTK_BOX(vbox),
                           progress,
                           FALSE/*expand*/,
                           FALSE/*fill*/,
                           12/*padding*/);

        gtk_widget_show(GTK_WIDGET(page));

        g_signal_connect(data->druid,
                         "cancel",
                         G_CALLBACK(gtk_main_quit),
                         NULL);

        return GNOME_DRUID_PAGE(page);
}

static GnomeDruidPage *last_page(struct first_time *data)
{
        GtkWidget *page;

        page = gnome_druid_page_edge_new_with_vals(GNOME_EDGE_FINISH,
                                                   TRUE/*antialiased*/,
                                                   "Finish " MAIN_TITLE,
                                                   FINISH_TEXT,
                                                   NULL/*logo*/,
                                                   NULL/*watermark*/,
                                                   NULL/*top_watermark*/);

        g_signal_connect(page,
                         "finish",
                         G_CALLBACK(gtk_main_quit),
                         NULL/*userdata*/);
        gtk_widget_show(page);

        g_signal_connect(data->druid,
                         "cancel",
                         G_CALLBACK(gtk_main_quit),
                         NULL);

        g_signal_connect(page,
                         "prepare",
                         G_CALLBACK(last_page_prepare),
                         data);

        data->last_page=GNOME_DRUID_PAGE(page);
        return GNOME_DRUID_PAGE(page);
}

static void discover(struct catalog *catalog)
{
        struct indexer  **indexer_ptr;
        for(indexer_ptr = indexers_list(); *indexer_ptr; indexer_ptr++) {
                struct indexer *indexer = *indexer_ptr;
                indexer_discover(indexer, catalog);
        }
}


static gboolean indexing(gpointer userdata)
{
        struct first_time *data;
        GnomeDruid *druid;

        g_return_val_if_fail(userdata, FALSE);

        data = (struct first_time *)userdata;
        g_return_val_if_fail(!data->indexed, FALSE);

        druid = GNOME_DRUID(data->druid);

        if(data->steps==NULL) {
                GSList *steps = NULL;
                guint steps_len = 0;

                gnome_druid_set_buttons_sensitive(druid,
                                                  FALSE/*no back*/,
                                                  FALSE/*no next*/,
                                                  TRUE/*cancel*/,
                                                  FALSE/*no help*/);

                gtk_progress_bar_set_fraction(data->progress, 0.0);
                gtk_progress_bar_set_text(data->progress, "File discovery");

                discover(data->catalog);

                steps=create_steps(data->catalog, &steps_len);
                data->steps=steps;
                data->current_step=steps;
                data->progress_step=1.0/((gdouble)steps_len+1);
                data->progress_current=0.0;

        } else if(data->current_step) {
                struct step *step = (struct step *)data->current_step->data;
                struct indexer_source *source;
                GError *err = NULL;
                data->current_step=data->current_step->next;
                data->progress_current+=data->progress_step;
                gtk_progress_bar_set_fraction(data->progress, data->progress_current);


                source=indexer_load_source(step->indexer, data->catalog, step->source_id);
                gtk_progress_bar_set_text(data->progress, source->display_name);
                indexer_source_index(source,  data->catalog, &err);



                handle_errors(data, err);

                g_free(step);
        }

        if(!data->current_step) {
                g_slist_free(data->steps);
                data->steps=NULL;
                data->current_step=NULL;
                data->indexed=TRUE;
                gtk_progress_bar_set_fraction(data->progress, (gdouble)1.0);
                gtk_progress_bar_set_text(data->progress, "Finished");
                gnome_druid_set_buttons_sensitive(druid,
                                                  FALSE/*no back*/,
                                                  TRUE/*next*/,
                                                  FALSE/*cancel*/,
                                                  FALSE/*no help*/);

                return FALSE;
        } else {
                return TRUE;
        }
}

static void indexing_page_prepare(GnomeDruidPage *page, GnomeDruid *druid, gpointer userdata)
{
        struct first_time *data;

        g_return_if_fail(userdata);
        data = (struct first_time *)userdata;


        gnome_druid_set_buttons_sensitive(druid,
                                          FALSE/*no back*/,
                                          FALSE/*no next*/,
                                          TRUE/*cancel*/,
                                          FALSE/*no help*/);
        g_idle_add(indexing,
                   userdata/*userdata*/);
}

static void last_page_prepare(GnomeDruidPage *page, GnomeDruid *druid, gpointer userdata)
{
        struct first_time *data;

        g_return_if_fail(userdata);
        data = (struct first_time *)userdata;

        data->pid = gnome_execute_async(g_get_home_dir(),
                                        ocha_init_daemon_argc,
                                        ocha_init_daemon_argv);
        if(data->pid==-1) {
                gnome_druid_page_edge_set_text(GNOME_DRUID_PAGE_EDGE(data->last_page),
                                               FAILED_TEXT);
        }

        g_idle_add(last_page_setup,
                   userdata/*userdata*/);
}

static gboolean last_page_setup(gpointer userdata)
{
        struct first_time *data;
        gboolean ochad_running;

        g_return_val_if_fail(userdata, FALSE);
        data = (struct first_time *)userdata;

        ochad_running=data->pid>0;
        gnome_druid_set_buttons_sensitive(data->druid,
                                          FALSE/*no back*/,
                                          ochad_running/*next*/,
                                          !ochad_running/*cancel*/,
                                          FALSE/*no help*/);

        return FALSE;
}

static GSList *create_steps(struct catalog *catalog, guint *count_ptr)
{
        GSList *retval = NULL;
        struct indexer  **indexer_ptr;
        guint count = 0;

        g_return_val_if_fail(catalog, NULL);
        g_return_val_if_fail(count_ptr, NULL);

        for(indexer_ptr = indexers_list();
            *indexer_ptr;
            indexer_ptr++) {
                struct indexer *indexer = *indexer_ptr;
                int *source_ids = NULL;
                int source_ids_len = 0;
                int i;

                ocha_gconf_get_sources(indexer->name,
                                       &source_ids,
                                       &source_ids_len);
                for(i=0; i<source_ids_len; i++) {
                        struct step *step;

                        step = g_new(struct step, 1);
                        step->indexer=indexer;
                        step->source_id=source_ids[i];
                        retval=g_slist_append(retval, step);
                        count++;
                }
                if(source_ids) {
                        g_free(source_ids);
                }
        }

        *count_ptr=count;
        return retval;
}

static void handle_errors(struct first_time *data, GError *err)
{
        g_return_if_fail(data);
        if(err==NULL) {
                return;
        }
        fprintf(stderr, "error: %s", err->message);
        g_error_free(err);
}

/**
 * Set gconf defaults for the current user.
 *
 * This totally violates the idea behind gconf that
 * administrators can set defaults, choose values
 * that users cannot change, etc... Yet, ocha
 * does not use gconf correctly and for the
 * time being, this will do...
 *
 * I ignore errors here just in case an administrator
 * has really set this up so that there *is* a default...
 */
static void set_default_configuration(void)
{
        GError *err = NULL;

        gconf_client_set_int(ocha_gconf_get_client(),
                             OCHA_GCONF_UPDATE_CATALOG_KEY,
                             OCHA_GCONF_UPDATE_CATALOG_DEFAULT,
                             &err);
        if(err) {
                g_error_free(err);
        }
}
