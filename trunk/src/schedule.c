#include "schedule.h"
#include "catalog.h"
#include "ocha_init.h"
#include <ocha_gconf.h>
#include <glib.h>
#include <libgnome/libgnome.h>

/** \file Schedule index updates, as defined in schedule.h
 *
 */

/**
 * Update interval, in minutes, for each possible setting value.
 * @see OchaGConfUpdateCatalog
 */
guint update_interval_min[OCHA_GCONF_UPDATE_CATALOG_COUNT] = {
        -1/*OCHA_GCONF_UPDATE_CATALOG_MANUALLY*/,
        10/*OCHA_GCONF_UPDATE_CATALOG_EVERY_10_MINUTES*/,
        30/*OCHA_GCONF_UPDATE_CATALOG_EVERY_30_MINUTES*/,
        60/*OCHA_GCONF_UPDATE_CATALOG_EVERY_HOUR*/,
        24*60/*OCHA_GCONF_UPDATE_CATALOG_EVERY_DAY*/
};

/**
 * Whether the current setting, stored
 * in the variable setting, is valid.
 *
 * The current setting is accessed using schedule_get_interval().
 * When the current setting changes, this variable
 * is set again to false by the callback.
 */
static gboolean setting_uptodate = FALSE;

/**
 * Time of the last update since the UNIX epoch.
 * 0 => never updated
 *
 * Invalid unless last_update_known == TRUE.
 *
 * Read it using schedule_get_last_update().
 * Updated by schedule_update_now().
 */
static gulong last_update;

/**
 * Set to true the first time last_update is
 * set. Ignore the value in last_update until
 * this is set to TRUE
 *
 * Access it using schedule_get_last_update()
 */
static gboolean last_update_known = FALSE;

static struct configuration config;

/* ------------------------- prototypes: static functions */
static gboolean schedule_first_time(void);
static gboolean schedule_check_cb(void);
static gboolean schedule_get_interval(guint *interval_min_out);
static gulong schedule_get_last_update(void);
static void schedule_update_now(void);
static void schedule_change_notify_cb(GConfClient *client, guint id, GConfEntry *entry, gpointer userdata);
/* ------------------------- definitions */

/* ------------------------- public functions */
void schedule_init(struct configuration *_config)
{
        g_return_if_fail(_config!=NULL);

        memcpy(&config, _config, sizeof(struct configuration));

        g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE+20/*low idle priority*/,
                           /*5*60**/1000/*ms*/,
                           (GSourceFunc)schedule_first_time,
                           NULL/*userdata*/,
                           NULL/*destroy notify*/);

        gconf_client_notify_add(ocha_gconf_get_client(),
                                OCHA_GCONF_UPDATE_CATALOG_KEY,
                                schedule_change_notify_cb,
                                NULL/*userdata*/,
                                NULL/*destroy cb*/,
                                NULL/*err*/);
 }

/* ------------------------- static functions */

/**
 * This function is called the 1st time automatic
 * reindexing is enabled.
 * It reads the last update entry from the database
 * and schedules new updates based on this information
 * and the current setting.
 * @return FALSE, always, so that it can be used as a timeout function
 * once.
 */
static gboolean schedule_first_time(void)
{
        schedule_check_cb();
        g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE+20/*low idle priority*/,
                           10*60*1000/*ms*/,
                           (GSourceFunc)schedule_check_cb,
                           NULL/*userdata*/,
                           NULL/*destroy notify*/);
        return FALSE;/*if used as a timeout function, don't repeat */
}

/**
 * Check whether update should run now (timeout callback).
 * @return TRUE always for the callback to be called again later
 */
static gboolean schedule_check_cb(void)
{
        guint interval;
        if(schedule_get_interval(&interval)) {
                guint interval_sec;
                gulong last_update;
                GTimeVal now;

                g_get_current_time(&now);

                interval_sec=interval*60;
                last_update=schedule_get_last_update();

                if((last_update+interval_sec)<now.tv_sec) {
                        schedule_update_now();
                }
        }
        return TRUE;
}

/**
 * Get the current update interval, if any.
 *
 * @param interval_out value to set to the current interval, in minutes, may be NULL
 * @return TRUE if automatic schedule is enabled, FALSE otherwise (in
 * which case interval_out is unmodified)
 */
static gboolean schedule_get_interval(guint *interval_out)
{
        /*
         * Current setting.
         *
         * It is invalid if setting_uptodate is FALSE.
         *
         * @see OchaGConfUpdateCatalog
         */
        static gint setting;
        /*
         * Interval corresponding to the current setting.
         *
         * It is invalid if setting==OCHA_GCONF_UPDATE_CATALOG_MANUALLY
         * or if setting_uptodate is FALSE
         */
        static guint interval;

        if(!setting_uptodate) {
                setting = gconf_client_get_int(ocha_gconf_get_client(),
                                               OCHA_GCONF_UPDATE_CATALOG_KEY,
                                               NULL/*default handler*/);

                if(setting<0 || setting>=OCHA_GCONF_UPDATE_CATALOG_COUNT) {
                        g_warning("invalid setting for "
                                  OCHA_GCONF_UPDATE_CATALOG_KEY
                                  ": %d range: 0<=value<%d. Not scheduling update.",
                                  setting,
                                  OCHA_GCONF_UPDATE_CATALOG_COUNT);
                        setting=OCHA_GCONF_UPDATE_CATALOG_MANUALLY;
                }
                interval = update_interval_min[setting];
                setting_uptodate=TRUE;
        }

        if(setting == OCHA_GCONF_UPDATE_CATALOG_MANUALLY) {
                return FALSE;
        } else {
                if(interval_out) {
                        *interval_out=interval;
                }
                return TRUE;
        }
}

/**
 * Get the time of the last update if known.
 *
 * @return time since the last update, in seconds since
 * the UNIX epoch or 0, meaning it was never updated.
 */
static gulong schedule_get_last_update(void)
{
        if(!last_update_known) {
                struct catalog *catalog;
                catalog = catalog_connect(config.catalog_path, NULL/*err*/);
                if(catalog!=NULL) {
                        last_update=catalog_timestamp_get(catalog);
                        printf("%s:%d: last_update=%lu\n", /*@nocommit@*/
                               __FILE__,
                               __LINE__,
                               last_update
                               );

                        catalog_disconnect(catalog);
                } else {
                        last_update=0;
                }
                last_update_known=TRUE;
        }

        return last_update;
}

/**
 * Launch ocha_indexer
 */
static void schedule_update_now(void)
{
        GTimeVal now;
        int pid;

        pid = gnome_execute_async(NULL/*current dir*/,
                                  ocha_init_indexer_argc,
                                  ocha_init_indexer_argv);
        if(pid==-1) {
                fprintf(stderr,
                        "ocha:warning: indexing failed: could not execute command %s\n",
                        ocha_init_indexer_argv[0]);
        }

        g_get_current_time(&now);
        last_update=now.tv_sec;
}

/**
 * Called when settings have been changed by the user.
 *
 * This writes down that the old setting is invalid for the
 * next time schedule_check_cb() is called.
 */
static void schedule_change_notify_cb(GConfClient *client, guint id, GConfEntry *entry, gpointer _userdata)
{
        setting_uptodate=FALSE;
}
