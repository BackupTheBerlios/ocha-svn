/** \file implementation of the API defined in catalog.h */

#include "catalog.h"
#include "catalog_result.h"
#include <sqlite.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#define SCHEMA_VERSION 1
#define SCHEMA_REVISION 0

/** Hidden catalog structure */
struct catalog
{
        sqlite *db;
        gboolean stop;
        GString* sql;
        GString* error;

        /** set in catalog_executequery to find the correct callback */
        catalog_callback_f callback;
        /** passed to callback */
        void *callback_userdata;
        GCond *busy_wait_cond;
        GMutex *busy_wait_mutex;

        char *path;

        /**
         * ID of the 'current source', used as a cache for source_version()
         */
        int current_source_id;
        /**
         * version of the 'current source', used as a cache for source_version()
         */
        int current_source_version;
};

#define return_unless_connected(catalog) if(!check_connected(catalog, __FILE__, __LINE__)) { return; }
#define return_val_unless_connected(catalog, val ) if(!check_connected(catalog, __FILE__, __LINE__)) { return (val); }

/* ------------------------- prototypes */
static int getinteger_callback(void *userdata, int column_count, char **result, char **names);
static gboolean exists(const char *path);
static gboolean handle_sqlite_retval(struct catalog *catalog, int retval, char *errmsg, const char *sql);
static int execute_update_nocatalog_printf(sqlite *db, const char *sql, char **errmsg, ...);
static int execute_update_nocatalog_vprintf(sqlite *db, const char *sql, char **errmsg, va_list ap);
static gboolean execute_update_printf(struct catalog *catalog, gboolean autocommit, const char *sql, ...);
static int progress_callback(void *userdata);
static gboolean execute_query_printf(struct catalog *catalog, sqlite_callback callback, void *userdata, const char *sql, ...);
static gboolean create_tables(sqlite *db, char **errmsg);
static int result_sqlite_callback(void *userdata, int col_count, char **col_data, char **col_names);
static void get_id(struct catalog  *catalog, int *id_out);
static int findid_callback(void *userdata, int column_count, char **result, char **names);
static int timestamp_callback(void *userdata, int column_count, char **result, char **names);
static gboolean findentry(struct catalog *catalog, const char *path, int source_id, int *id_out);
static gboolean source_version(struct catalog *catalog, int source_id, int *version_out);
static gboolean check_connected(struct catalog *catalog, const char *file, int line);
static void reset_error(struct catalog *catalog);

/* ------------------------- public functions */

gboolean catalog_add_entry(struct catalog *catalog,
                           const struct catalog_entry *entry,
                           int *id_out)
{
        int old_id=-1;
        int version;

        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(entry!=NULL, FALSE);
        g_return_val_if_fail(entry->launcher!=NULL, FALSE);
        g_return_val_if_fail(entry->path!=NULL, FALSE);
        g_return_val_if_fail(entry->name!=NULL, FALSE);
        g_return_val_if_fail(entry->long_name!=NULL, FALSE);

        return_val_unless_connected(catalog, FALSE);


        if(!source_version(catalog, entry->source_id, &version)) {
                return FALSE;
        }


        if(findentry(catalog, entry->path, entry->source_id, &old_id))
        {
                if(id_out) {
                        *id_out=old_id;
                }
                return execute_update_printf(catalog, TRUE/*autocommit*/,
                                             "UPDATE entries "
                                             "SET name='%q', long_name='%q', source_id=%d, launcher='%q', version=%d "
                                             "WHERE id=%d",
                                             entry->name,
                                             entry->long_name,
                                             entry->source_id,
                                             entry->launcher,
                                             version,
                                             old_id);
        } else
        {
                if(execute_update_printf(catalog, TRUE/*autocommit*/,
                                         "INSERT INTO entries "
                                         " (id, path, name, long_name, source_id, launcher, version, enabled) "
                                         " VALUES (NULL, '%q', '%q', '%q', %d, '%q', %d, 1)",
                                         entry->path,
                                         entry->name,
                                         entry->long_name,
                                         entry->source_id,
                                         entry->launcher,
                                         version)) {
                        get_id(catalog, id_out);
                        return TRUE;
                }
                return FALSE;
        }
}

gboolean catalog_add_source(struct catalog *catalog, const char *type, int *id_out)
{
        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(type!=NULL, FALSE);
        g_return_val_if_fail(id_out!=NULL, FALSE);

        return_val_unless_connected(catalog, FALSE);

        if(execute_update_printf(catalog, TRUE/*autocommit*/,
                                 "INSERT INTO sources (id, type, version, enabled) VALUES (NULL, '%q', 0, 1)",
                                 type))
        {
                get_id(catalog, id_out);
                return TRUE;
        }
        return FALSE;
}

gboolean catalog_check_source(struct catalog *catalog, const char *type, int source_id)
{
        int count;
        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(type!=NULL, FALSE);

        return_val_unless_connected(catalog, FALSE);

        if(!execute_query_printf(catalog,
                                 getinteger_callback,
                                 &count,
                                 "SELECT COUNT(*) FROM sources WHERE id=%d and type='%q'",
                                 source_id,
                                 type)) {
                return FALSE;
        }

        if(count==1) {
                return TRUE;
        }

        return execute_update_printf(catalog,
                                     TRUE/*autocommit*/,
                                     "DELETE FROM sources WHERE id=%d; "
                                     "DELETE FROM entries WHERE source_id=%d; "
                                     "INSERT INTO sources (id, type, version, enabled) VALUES (%d, '%q', 0, 1);",
                                     source_id,
                                     source_id,
                                     source_id,
                                     type);
}

struct catalog *catalog_new(const char *path)
{
        struct catalog *catalog;
        g_return_val_if_fail(path, NULL);

        catalog = g_new(struct catalog, 1);
        catalog->stop=FALSE;
        catalog->db=NULL;
        catalog->error=g_string_new("");
        catalog->busy_wait_cond=g_cond_new();
        catalog->busy_wait_mutex=g_mutex_new();
        catalog->path=g_strdup(path);

        return catalog;
}

struct catalog *catalog_new_and_connect(const char *path, GError **err)
{
        struct catalog *catalog;

        g_return_val_if_fail(path, NULL);
        g_return_val_if_fail(err==NULL || *err==NULL, NULL);

        catalog=catalog_new(path);
        if(!catalog_connect(catalog))
        {
                g_set_error(err,
                            catalog_error_quark(),
                            CATALOG_CONNECTION_ERROR,
                            catalog_error(catalog));
                catalog_free(catalog);
                return NULL;
        } else {
                return catalog;
        }
}

gboolean catalog_connect(struct catalog *catalog)
{
        gboolean newdb;
        char *errmsg;
        sqlite *db;

        g_return_val_if_fail(catalog!=NULL, FALSE);

        reset_error(catalog);

        if(catalog_is_connected(catalog)) {
                g_string_append(catalog->error, "already connected");
                return FALSE;
        }

        newdb = !exists(catalog->path);
        errmsg = NULL;
        db = sqlite_open(catalog->path, 0600, &errmsg);
        if(!db) {
                g_string_append_printf(catalog->error,
                                       "opening catalog %s failed: %s\n",
                                       catalog->path,
                                       errmsg==NULL ? "unknown error":errmsg);
                if(errmsg) {
                        sqlite_freemem(errmsg);
                }
                return FALSE;
        }
        if(newdb) {
                if(!create_tables(db, &errmsg)) {
                        g_string_append_printf(catalog->error,
                                               "initialization of catalog %s failed: %s\n",
                                               catalog->path,
                                               errmsg==NULL ? "unknown error":errmsg);
                        if(errmsg) {
                                sqlite_freemem(errmsg);
                        }

                        sqlite_close(db);
                        unlink(catalog->path);
                        return FALSE;
                }
        }

        catalog->db=db;
        return TRUE;
}

void catalog_disconnect(struct catalog *catalog)
{
        g_return_if_fail(catalog!=NULL);

        reset_error(catalog);

        return_unless_connected(catalog);

        sqlite_close(catalog->db);
        catalog->db=NULL;
}

gboolean catalog_is_connected(struct catalog *catalog)
{
        g_return_val_if_fail(catalog!=NULL, FALSE);
        return catalog->db!=NULL;
}

void catalog_free(struct catalog *catalog)
{
        g_return_if_fail(catalog!=NULL);
        if(catalog_is_connected(catalog)) {
                catalog_disconnect(catalog);
        }
        g_cond_free(catalog->busy_wait_cond);
        g_mutex_free(catalog->busy_wait_mutex);
        g_string_free(catalog->error, TRUE/*free_segment*/);
        g_free(catalog->path);
        g_free(catalog);
}

gboolean catalog_executequery(struct catalog *catalog,
                              const char *query,
                              catalog_callback_f callback,
                              void *userdata)
{
        GString *sql;
        gboolean has_space;
        const char *cptr;
        gboolean ret;

        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(query!=NULL, FALSE);
        g_return_val_if_fail(callback!=NULL, FALSE);

        return_val_unless_connected(catalog, FALSE);

        if(catalog->stop)
                return TRUE;

        /* the order of the columns is important, see result_sqlite_callback() */

        sql = g_string_new("SELECT e.id, e.path, e.name, e.long_name, "
                           "       s.id, e.launcher, e.enabled, e.lastuse "
                           "FROM entries e, sources s "
                           "WHERE e.enabled==1 AND e.source_id=s.id AND s.enabled==1 AND e.name LIKE '%%");

        has_space=FALSE;
        for(cptr=query; *cptr!='\0'; cptr++)
        {
                char c = *cptr;
                switch(c) {
                case ' ':
                        has_space=TRUE;
                        break;

                case '\'':
                case '"':
                        break;

                default:
                        if(has_space) {
                                g_string_append(sql,
                                                "%%' AND e.name LIKE '%%");
                                has_space=FALSE;
                        }
                        g_string_append_c(sql, c);
                }
        }
        g_string_append(sql,
                        "%%' "
                        "ORDER BY e.lastuse DESC");
        catalog->callback=callback;
        catalog->callback_userdata=userdata;
        ret = execute_query_printf(catalog,
                                   result_sqlite_callback,
                                   catalog/*userdata*/,
                                   sql->str);
        g_string_free(sql, TRUE/*free content*/);

        /* the catalog was probably called from two threads
         * at the same time: this is forbidden
         */
        g_return_val_if_fail(catalog->callback==callback,
                             FALSE);

        catalog->callback=NULL;
        catalog->callback_userdata=NULL;

        return ret;
}

const char *catalog_error(struct catalog *catalog)
{
        g_return_val_if_fail(catalog!=NULL, NULL);
        if(catalog->error->len>0) {
                return catalog->error->str;
        }
        return "unknown error";
}

GQuark catalog_error_quark()
{
        static GQuark catalog_quark;
        static gboolean initialized;
        if(!initialized) {
                catalog_quark=g_quark_from_static_string("CATALOG_ERROR");
                initialized=TRUE;
        }
        return catalog_quark;
}

gulong catalog_timestamp_get(struct catalog *catalog)
{
        gulong ts = 0;

        return_val_unless_connected(catalog, 0);

        execute_query_printf(catalog,
                             timestamp_callback,
                             &ts/*userdata*/,
                             "SELECT value FROM history WHERE  event = 'indexed'");
        return ts;
}

gboolean catalog_timestamp_update(struct catalog *catalog)
{
        GTimeVal now;
        gulong old;

        return_val_unless_connected(catalog, FALSE);

        old = catalog_timestamp_get(catalog);
        g_get_current_time(&now);

        if(old==0) {
                return execute_update_printf(catalog,
                                             TRUE/*autocommit*/,
                                             "INSERT INTO history (event, value) VALUES ('indexed', '%lu')",
                                             now.tv_sec);
        } else {
                return execute_update_printf(catalog,
                                             TRUE/*autocommit*/,
                                             "UPDATE history SET value='%lu' WHERE event='indexed'",
                                             now.tv_sec);
        }
}

gboolean catalog_get_source_content(struct catalog *catalog,
                                    int source_id,
                                    catalog_callback_f callback,
                                    void *userdata)
{
        gboolean ret;

        return_val_unless_connected(catalog, FALSE);

        catalog->callback=callback;
        catalog->callback_userdata=userdata;
        ret = execute_query_printf(catalog,
                                   result_sqlite_callback,
                                   catalog/*userdata*/,
                                   "SELECT e.id, e.path, e.name, e.long_name, "
                                   " s.id, e.launcher, e.enabled, e.lastuse "
                                   "FROM entries e, sources s "
                                   "WHERE e.source_id=%d and s.id=%d "
                                   "ORDER BY UPPER(e.name), UPPER(e.long_name)",
                                   source_id,
                                   source_id);
        catalog->callback=NULL;
        catalog->callback_userdata=userdata;
        return ret;
}

gboolean catalog_get_source_content_count(struct catalog *catalog,
                                          int source_id,
                                          unsigned int *count_out)
{
        guint number;
        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(count_out!=NULL, FALSE);

        return_val_unless_connected(catalog, FALSE);

        number=0;
        if(execute_query_printf(catalog,
                                getinteger_callback,
                                &number,
                                "SELECT COUNT(*) "
                                "FROM entries "
                                "WHERE source_id=%d",
                                source_id)) {
                *count_out=number;
                return TRUE;
        }
        return FALSE;
}

void catalog_interrupt(struct catalog *catalog)
{
        g_return_if_fail(catalog!=NULL);
        reset_error(catalog);

        g_mutex_lock(catalog->busy_wait_mutex);
        catalog->stop=TRUE;
        g_cond_broadcast(catalog->busy_wait_cond);
        g_mutex_unlock(catalog->busy_wait_mutex);
}

gboolean catalog_remove_entry(struct catalog *catalog,
                              int source_id,
                              const char *path)
{
        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(path!=NULL, FALSE);

        return_val_unless_connected(catalog, FALSE);

        return execute_update_printf(catalog, TRUE/*autocommit*/,
                                     "DELETE FROM entries "
                                     " WHERE source_id=%d AND path='%q'",
                                     source_id,
                                     path);
}

gboolean catalog_remove_source(struct catalog *catalog,
                               int source_id)
{
        g_return_val_if_fail(catalog!=NULL, FALSE);

        return_val_unless_connected(catalog, FALSE);
        return execute_update_printf(catalog, TRUE/*autocommit*/,
                                     "DELETE FROM sources "
                                     " WHERE id=%d; "
                                     "DELETE FROM entries "
                                     " WHERE source_id=%d",
                                     source_id,
                                     source_id);
}

void catalog_restart(struct catalog *catalog)
{
        g_return_if_fail(catalog!=NULL);
        reset_error(catalog);
        g_mutex_lock(catalog->busy_wait_mutex);
        catalog->stop=FALSE;
        g_mutex_unlock(catalog->busy_wait_mutex);
}

gboolean catalog_update_entry_timestamp(struct catalog *catalog, int entry_id)
{
        GTimeVal timeval;

        g_return_val_if_fail(catalog, FALSE);

        return_val_unless_connected(catalog, FALSE);

        g_get_current_time(&timeval);
        return execute_update_printf(catalog, TRUE/*autocommit*/,
                                     "UPDATE entries "
                                     "SET lastuse='%16.16lx.%6.6lu' "
                                     "WHERE id=%d",
                                     (unsigned long)timeval.tv_sec,
                                     (unsigned long)timeval.tv_usec,
                                     entry_id);
}


gboolean catalog_begin_source_update(struct catalog *catalog, int source_id)
{
        int old_version;
        int new_version;

        g_return_val_if_fail(catalog, FALSE);
        g_return_val_if_fail(source_id>0, FALSE);

        return_val_unless_connected(catalog, FALSE);

        if(!source_version(catalog, source_id, &old_version)) {
                return FALSE;
        }
        new_version=old_version+1;
        if(execute_update_printf(catalog,
                                 TRUE/*autocommit*/,
                                 "UPDATE sources SET version=%d WHERE id=%d",
                                 new_version,
                                 source_id))
        {
                catalog->current_source_id=source_id;
                catalog->current_source_version=new_version;
                return TRUE;
        }
        return FALSE;
}

gboolean catalog_end_source_update(struct catalog *catalog, int source_id)
{
        int version;

        g_return_val_if_fail(catalog, FALSE);
        g_return_val_if_fail(source_id>0, FALSE);

        return_val_unless_connected(catalog, FALSE);

        if(!source_version(catalog, source_id, &version)) {
                return FALSE;
        }

        return execute_update_printf(catalog,
                                     TRUE/*autocommit*/,
                                     "DELETE FROM entries WHERE source_id=%d and version<%d",
                                     source_id,
                                     version);
}

gboolean catalog_entry_set_enabled(struct catalog *catalog, int entry_id, gboolean enabled)
{
        g_return_val_if_fail(catalog, FALSE);

        return_val_unless_connected(catalog, FALSE);

        return execute_update_printf(catalog,
                                     TRUE/*autocommit*/,
                                     "UPDATE entries SET enabled=%d WHERE id=%d;",
                                     enabled ? 1:0,
                                     entry_id);
}

gboolean catalog_source_set_enabled(struct catalog *catalog, int source_id, gboolean enabled)
{
        g_return_val_if_fail(catalog, FALSE);

        return_val_unless_connected(catalog, FALSE);

        return execute_update_printf(catalog,
                                     TRUE/*autocommit*/,
                                     "UPDATE sources SET enabled=%d WHERE id=%d;",
                                     enabled ? 1:0,
                                     source_id);
}

gboolean catalog_source_get_enabled(struct catalog *catalog, int source_id, gboolean *enabled_out)
{
        int number;

        g_return_val_if_fail(catalog, FALSE);
        g_return_val_if_fail(enabled_out, FALSE);

        return_val_unless_connected(catalog, FALSE);

        number=0;
        if(execute_query_printf(catalog,
                                getinteger_callback,
                                &number,
                                "SELECT enabled "
                                "FROM sources "
                                "WHERE id=%d",
                                source_id)) {
                *enabled_out = number!=0;
                return TRUE;
        }
        return FALSE;
}

/* ------------------------- static functions */
/**
 * sqlite callback that expects an unsigned integer as the 1st (and only) result
 */
static int getinteger_callback(void *userdata,
                             int column_count,
                             char **result,
                             char **names)
{
        guint *number_out;
        long l;

        g_return_val_if_fail(userdata!=NULL, 1);
        g_return_val_if_fail(column_count>0, 1);
        number_out = (guint *)userdata;
        l = atol(result[0]);
        *number_out=(guint)l;
        return 1; /* no need for more results */
}

static gboolean exists(const char *path)
{
        struct stat buf;
        return stat(path, &buf)==0 && buf.st_size>0;
}
static gboolean handle_sqlite_retval(struct catalog *catalog, int retval, char *errmsg, const char *sql)
{
        g_return_val_if_fail(catalog!=NULL, FALSE);

        reset_error(catalog);
        if(retval==SQLITE_OK || retval==SQLITE_ABORT || retval==SQLITE_INTERRUPT) {
                return TRUE;
        }

        if(errmsg==NULL)
        {
                const char *staticerror=sqlite_error_string(retval);
                if(staticerror)
                        g_string_append(catalog->error, staticerror);
                else
                        g_string_printf(catalog->error, "unknown error (%d)", retval);
        } else
        {
                g_string_append(catalog->error, errmsg);
                sqlite_freemem(errmsg);
        }
        if(sql)
        {
                g_string_append(catalog->error, " (SQL: ");
                g_string_append(catalog->error, sql);
                g_string_append(catalog->error, ")");
        }
        return FALSE;
}

static int execute_update_nocatalog_printf(sqlite *db,
                                           const char *sql,
                                           char **errmsg, ...)
{
        va_list ap;
        va_start(ap, errmsg);
        return execute_update_nocatalog_vprintf(db, sql, errmsg, ap);
}
static int execute_update_nocatalog_vprintf(sqlite *db,
                                            const char *sql,
                                            char **errmsg,
                                            va_list ap)
{
        int ret;

        sqlite_busy_timeout(db, 30000/*30 seconds timout, for updates*/);

        ret=sqlite_exec_vprintf(db,
                            sql,
                            NULL/*no callback*/,
                            NULL/*no userdata*/,
                            errmsg,
                            ap);
        if(ret!=0) {
                sqlite_exec(db, "ROLLBACK", NULL, NULL, NULL);
        }

        sqlite_busy_timeout(db, -1/*disable, for queries*/);

        return ret;
}
static gboolean execute_update_printf(struct catalog *catalog,
                                      gboolean autocommit,
                                      const char *sql, ...)
{
        va_list ap;
        int ret;
        char *errmsg = NULL;

        va_start(ap, sql);

        if(autocommit) {
                char *buffer = g_strdup_printf("BEGIN;%s;COMMIT;", sql);
                ret = execute_update_nocatalog_vprintf(catalog->db,
                                                       buffer,
                                                       &errmsg,
                                                       ap);
                g_free(buffer);
        } else {
                ret = execute_update_nocatalog_vprintf(catalog->db,
                                                       sql,
                                                       &errmsg,
                                                       ap);
        }
        return handle_sqlite_retval(catalog, ret, errmsg, sql);
}

static int progress_callback(void *userdata)
{
        struct catalog *catalog = (struct catalog *)userdata;
        return catalog->stop ? 1:0;
}

static gboolean execute_query_printf(struct catalog *catalog,
                                     sqlite_callback callback,
                                     void *userdata,
                                     const char *sql, ...)
{
        va_list ap;
        char *errmsg=NULL;
        int ret;

        /* callers should call return_(val_)unless_connected before
         * this function
         */
        g_return_val_if_fail(catalog->db!=NULL, FALSE);

        va_start(ap, sql);

        sqlite_progress_handler(catalog->db, 1, progress_callback, catalog);
        do {
                if(catalog->stop) {
                        ret=SQLITE_ABORT;
                        break;
                }
                ret = sqlite_exec_vprintf(catalog->db,
                                          sql,
                                          callback,
                                          userdata,
                                          &errmsg,
                                          ap);
                if(ret==SQLITE_BUSY) {
                        g_mutex_lock(catalog->busy_wait_mutex);
                        if(!catalog->stop) {
                                GTimeVal timeval;
                                g_get_current_time(&timeval);
                                g_time_val_add(&timeval, 10000/*1/10th of a second*/);
                                g_cond_timed_wait(catalog->busy_wait_cond,
                                                  catalog->busy_wait_mutex,
                                                  &timeval);
                        }
                        g_mutex_unlock(catalog->busy_wait_mutex);
                }
        } while(ret==SQLITE_BUSY);
        sqlite_progress_handler(catalog->db, 0, NULL/*no callback*/, NULL/*no userdata*/);

        return handle_sqlite_retval(catalog, ret, errmsg, sql);
}

static gboolean create_tables(sqlite *db, char **errmsg)
{
        int ret;
        ret = execute_update_nocatalog_printf(db, "BEGIN", errmsg);
        if(ret!=SQLITE_OK) {
                return FALSE;
        }
        ret = execute_update_nocatalog_printf(db,
                                              "CREATE TABLE entries (id INTEGER PRIMARY KEY, "
                                              "path VARCHAR NOT NULL, "
                                              "name VARCHAR NOT NULL, "
                                              "long_name VARCHAR NOT NULL, "
                                              "source_id INTEGER, "
                                              "launcher VARCHAR NOT NULL, "
                                              "lastuse TIMESTAMP, "
                                              "version INTEGER, "
                                              "enabled INTEGER NOT NULL, "
                                              "UNIQUE (id, path));"
                                              "CREATE INDEX lastuse_idx ON entries (lastuse DESC);"
                                              "CREATE INDEX path_idx ON entries (path);"
                                              "CREATE INDEX e_enabled_idx ON entries (enabled);"
                                              "CREATE INDEX source_idx ON entries (source_id);",
                                              errmsg);
        if(ret!=SQLITE_OK) {
                return FALSE;
        }
        ret = execute_update_nocatalog_printf(db,
                                              "CREATE TABLE sources (id INTEGER PRIMARY KEY , "
                                              "type VARCHAR NOT NULL, "
                                              "version INTEGER NOT NULL, "
                                              "enabled INTEGER NOT NULL);"
                                              "CREATE TABLE source_attrs (source_id INTEGER, "
                                              "attribute VARCHAR NOT NULL,"
                                              "value VARCHAR NOT NULL,"
                                              "PRIMARY KEY (source_id, attribute));"
                                              "CREATE INDEX s_enabled_idx ON sources (enabled);",
                                              errmsg);
        if(ret!=SQLITE_OK) {
                return FALSE;
        }
        ret = execute_update_nocatalog_printf(db,
                                              "CREATE TABLE VERSION ( version INTEGER, revision INTEGER );"
                                              "INSERT INTO VERSION VALUES ( %d, %d );",
                                              errmsg,
                                              SCHEMA_VERSION,
                                              SCHEMA_REVISION);
        if(ret!=SQLITE_OK) {
                return FALSE;
        }

        ret = execute_update_nocatalog_printf(db,
                                              "CREATE TABLE history ( event VARCHAR NOT NULL, value VARCHAR NOT NULL);",
                                              errmsg);
        if(ret!=SQLITE_OK) {
                return FALSE;
        }

        ret = execute_update_nocatalog_printf(db, "COMMIT", errmsg);
        return ret==SQLITE_OK;
}

static int result_sqlite_callback(void *userdata,
                                  int col_count,
                                  char **col_data,
                                  char **col_names)
{
        struct catalog *catalog;
        struct catalog_query_result result;
        catalog_callback_f callback;
        gboolean go_on;

        catalog =  (struct catalog *)userdata;
        callback =  catalog->callback;

        /* executequery called by another thread: forbidden */
        g_return_val_if_fail(callback!=NULL, 1);

        if(catalog->stop) {
                return 1;
        }

        /* this must correspond to the query in catalog_executequery()
         * and catalog_get_source_content()
         */
        result.id = atoi(col_data[0]);
        result.entry.path = col_data[1];
        result.entry.name = col_data[2];
        result.entry.long_name = col_data[3];
        result.entry.source_id = atoi(col_data[4]);
        result.entry.launcher = col_data[5];
        result.pertinence = 0.5;
        result.enabled = *col_data[6]=='1';

        go_on = catalog->callback(catalog,
                                  &result,
                                  catalog->callback_userdata);
        return go_on && !catalog->stop ? 0:1;
}

static void get_id(struct catalog  *catalog, int *id_out)
{
        if(id_out!=NULL) {
                *id_out=sqlite_last_insert_rowid(catalog->db);
        }
}

/**
 * sqlite callback that expects an id as the 1st (and only) result
 */
static int findid_callback(void *userdata,
                           int column_count,
                           char **result,
                           char **names)
{
        int *id_out;
        g_return_val_if_fail(userdata!=NULL, 1);
        g_return_val_if_fail(column_count>0, 1);
        id_out =  (int *)userdata;
        *id_out=atoi(result[0]);
        return 1; /* no need for more results */
}


/**
 * sqlite callback that expects a timestame (gulong) as the 1st (and only) result
 */
static int timestamp_callback(void *userdata,
                              int column_count,
                              char **result,
                              char **names)
{
        gulong *id_out;
        g_return_val_if_fail(userdata!=NULL, 1);
        g_return_val_if_fail(column_count>0, 1);
        id_out =  (gulong *)userdata;
        *id_out=strtoul(result[0], NULL/*endptr*/, 10/*base*/);
        return 1; /* no need for more results */
}

static gboolean findentry(struct catalog *catalog,
                          const char *path,
                          int source_id,
                          int *id_out)
{
        int id=-1;

        g_return_val_if_fail(catalog!=NULL, FALSE);
        g_return_val_if_fail(path!=NULL, FALSE);

        if(execute_query_printf(catalog,
                                findid_callback,
                                &id,
                                "SELECT id FROM entries "
                                " WHERE path='%q' AND source_id='%d'",
                                path,
                                source_id)
                        && id!=-1)
        {
                if(id_out)
                        *id_out=id;
                return TRUE;
        }
        return FALSE;
}

static gboolean source_version(struct catalog *catalog,
                               int source_id,
                               int *version_out)
{
        g_return_val_if_fail(catalog, FALSE);
        g_return_val_if_fail(version_out, FALSE);

        if(catalog->current_source_id==source_id) {
                *version_out=catalog->current_source_version;
                return TRUE;
        }

        if(execute_query_printf(catalog,
                                getinteger_callback,
                                version_out/*userdata*/,
                                "SELECT version FROM sources WHERE id=%d",
                                source_id))
        {
                catalog->current_source_id=source_id;
                catalog->current_source_version=*version_out;
                return TRUE;
        }
        return FALSE;
}

/**
 * If not connected, return FALSE and set the catalog error.
 */
static gboolean check_connected(struct catalog *catalog, const char *file, int line)
{
        g_return_val_if_fail(catalog, FALSE);
        if(!catalog_is_connected(catalog)) {
                reset_error(catalog);
                g_string_append_printf(catalog->error,
                                       "%s:%d: not connected. please call catalog_connect() first.",
                                       file,
                                       line);
                return FALSE;
        }
        return TRUE;
}

static void reset_error(struct catalog *catalog)
{
        g_return_if_fail(catalog);
        g_string_truncate(catalog->error, 0);
}
