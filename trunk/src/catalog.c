/** \file implementation of the API defined in catalog.h */

#include "catalog.h"
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

   char path[];
};

/**
 * Full structure of the results passed to the callback
 */
struct catalog_result
{
   /** catalog_result are result */
   struct result base;
   int entry_id;
   const char *execute;
   const char *catalog_path;

   /** buffer for the path, catalog path, display name and execute string */
   char buffer[];
};



/* ------------------------- prototypes */
static gboolean exists(const char *path);
static gboolean handle_sqlite_retval(struct catalog *catalog, int retval, char *errmsg);
static int execute_update_nocatalog_printf(sqlite *db, const char *sql, char **errmsg, ...);
static int execute_update_nocatalog_vprintf(sqlite *db, const char *sql, char **errmsg, va_list ap);
static gboolean execute_update_printf(struct catalog *catalog, const char *sql, ...);
static int progress_callback(void *userdata);
static gboolean execute_query_printf(struct catalog *catalog, sqlite_callback callback, void *userdata, const char *sql, ...);
static gboolean create_tables(sqlite *db, char **errmsg);
static int count(const char *str, char c);
static void execute_parse(GString *gstr, const char *pattern, const char *path);
static gboolean update_entry_timestamp(struct catalog *catalog, int entry_id);
static gboolean execute_result(struct result *_self, GError **);
static void free_result(struct result *self);
static struct result *create_result(struct catalog *catalog, const char *path, const char *name, const char *execute, int entry_id);
static int result_sqlite_callback(void *userdata, int col_count, char **col_data, char **col_names);
static void get_id(struct catalog  *catalog, int *id_out);
static int findid_callback(void *userdata, int column_count, char **result, char **names);


/* ------------------------- public functions */

GQuark catalog_error_quark()
{
   static GQuark catalog_quark;
   static gboolean initialized;
   if(!initialized)
      {
         catalog_quark=g_quark_from_static_string("CATALOG_ERROR");
         initialized=TRUE;
      }
   return catalog_quark;
}

struct catalog *catalog_connect(const char *path, GError **err)
{
   g_return_val_if_fail(path, NULL);
   g_return_val_if_fail(err==NULL || *err==NULL, NULL);

   gboolean newdb = !exists(path);
   char *errmsg = NULL;


   sqlite *db = sqlite_open(path, 0600, &errmsg);
   if(!db)
      {
         g_set_error(err,
                     catalog_error_quark(),
                     CATALOG_CONNECTION_ERROR,
                     "opening catalog %s failed: %s\n",
                     path,
                     errmsg==NULL ? "unknown error":errmsg);
         if(errmsg)
            sqlite_freemem(errmsg);
         return NULL;
      }
   if(newdb)
      {
         if(!create_tables(db, &errmsg))
            {
               g_set_error(err,
                           catalog_error_quark(),
                           CATALOG_CANNOT_CREATE_TABLES,
                           "initialization of catalog %s failed: %s\n",
                           path,
                           errmsg==NULL ? "unknown error":errmsg);
               if(errmsg)
                  sqlite_freemem(errmsg);

               sqlite_close(db);
               unlink(path);
               return NULL;
            }
      }

   struct catalog *catalog = (struct catalog *)g_malloc(sizeof(struct catalog)+strlen(path)+1);
   catalog->stop=FALSE;
   catalog->db=db;
   catalog->error=g_string_new("");
   catalog->busy_wait_cond=g_cond_new();
   catalog->busy_wait_mutex=g_mutex_new();
   strcpy(catalog->path, path);
   return catalog;
}

const char *catalog_error(struct catalog *catalog)
{
   g_return_val_if_fail(catalog!=NULL, NULL);
   if(catalog->error->len>0)
      return catalog->error->str;
   return "unknown error";
}
void catalog_disconnect(struct catalog *catalog)
{
   g_return_if_fail(catalog!=NULL);
   sqlite_close(catalog->db);
   g_cond_free(catalog->busy_wait_cond);
   g_mutex_free(catalog->busy_wait_mutex);
   g_string_free(catalog->error, TRUE/*free_segment*/);
   g_free(catalog);
}

void catalog_interrupt(struct catalog *catalog)
{
   g_return_if_fail(catalog!=NULL);
   g_mutex_lock(catalog->busy_wait_mutex);
   catalog->stop=TRUE;
   g_cond_broadcast(catalog->busy_wait_cond);
   g_mutex_unlock(catalog->busy_wait_mutex);
}

void catalog_restart(struct catalog *catalog)
{
   g_return_if_fail(catalog!=NULL);
   g_mutex_lock(catalog->busy_wait_mutex);
   catalog->stop=FALSE;
   g_mutex_unlock(catalog->busy_wait_mutex);
}

gboolean catalog_executequery(struct catalog *catalog,
                          const char *query,
                          catalog_callback_f callback,
                          void *userdata)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(query!=NULL, FALSE);
   g_return_val_if_fail(callback!=NULL, FALSE);

   if(catalog->stop)
      return TRUE;

   GString *sql = g_string_new("SELECT e.id, e.path, e.name, c.execute, e.lastuse "
                               "FROM entries e, command c "
                               "WHERE e.name LIKE '%%");
   gboolean has_space=FALSE;
   for(const char *cptr=query; *cptr!='\0'; cptr++)
      {
         char c = *cptr;
         switch(c)
            {
            case ' ':
               has_space=TRUE;
               break;

            case '\'':
            case '"':
               break;

            default:
               if(has_space)
                  {
                     g_string_append(sql,
                                     "%%' AND e.name LIKE '%%");
                     has_space=FALSE;
                  }
               g_string_append_c(sql, c);
            }
      }
   g_string_append(sql,
                   "%%' AND e.command_id=c.id "
                   "ORDER BY e.lastuse DESC");
   catalog->callback=callback;
   catalog->callback_userdata=userdata;
   gboolean ret = execute_query_printf(catalog,
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



gboolean catalog_addcommand(struct catalog *catalog, const char *name, const char *execute, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(name!=NULL, FALSE);
   g_return_val_if_fail(execute!=NULL, FALSE);

   int old_id = -1;
   if(catalog_findcommand(catalog, name, &old_id))
      {
         if(id_out)
            *id_out=old_id;
         return execute_update_printf(catalog,
                                      "UPDATE command SET execute='%q' WHERE id=%d",
                                      execute,
                                      old_id);
      }
   else
      {
         if(execute_update_printf(catalog,
                                  "INSERT INTO command (id, name, execute) VALUES (NULL, '%q', '%q')",
                                  name,
                                  execute))
            {
               get_id(catalog, id_out);
               return TRUE;
            }
         return FALSE;
      }
}


gboolean catalog_findcommand(struct catalog *catalog, const char *name, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(name!=NULL, FALSE);

   int id=-1;
   if( execute_query_printf(catalog,
                            findid_callback,
                            &id,
                            "SELECT id FROM command where name='%q'",
                            name)
       && id!=-1)
      {
         if(id_out)
            *id_out=id;
         return TRUE;
      }
   return FALSE;
}

gboolean catalog_findentry(struct catalog *catalog, const char *path, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(path!=NULL, FALSE);

   int id=-1;
   if(execute_query_printf(catalog,
                           findid_callback,
                           &id,
                           "SELECT id FROM entries WHERE path='%q'",
                           path)
      && id!=-1)
      {
         if(id_out)
            *id_out=id;
         return TRUE;
      }
   return FALSE;
}

gboolean catalog_addentry(struct catalog *catalog, const char *path, const char *name, const char *long_name, int command_id, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);
   g_return_val_if_fail(path!=NULL, FALSE);
   g_return_val_if_fail(name!=NULL, FALSE);
   g_return_val_if_fail(long_name!=NULL, FALSE);

   int old_id=-1;
   if(catalog_findentry(catalog, path, &old_id))
      {
         return execute_update_printf(catalog,
                                      "UPDATE entries SET name='%q', command_id=%d WHERE id=%d",
                                      name,
                                      command_id,
                                      old_id);
      }
   else
      {
         if(execute_update_printf(catalog,
                                  "INSERT INTO entries (id, path, name, long_name, command_id) VALUES (NULL, '%q', '%q', '%q', %d)",
                                  path,
                                  name,
                                  long_name,
                                  command_id,
                                  NULL))
            {
               get_id(catalog, id_out);
               return TRUE;
            }
         return FALSE;
      }
}

/* ------------------------- static functions */
static gboolean exists(const char *path)
{
   struct stat buf;
   return stat(path, &buf)==0 && buf.st_size>0;
}
static gboolean handle_sqlite_retval(struct catalog *catalog, int retval, char *errmsg)
{
   g_return_val_if_fail(catalog!=NULL, FALSE);

   if(retval==SQLITE_OK || retval==SQLITE_ABORT || retval==SQLITE_INTERRUPT)
      return TRUE;

   g_string_truncate(catalog->error, 0);
   if(errmsg==NULL)
      {
         const char *staticerror=sqlite_error_string(retval);
         if(staticerror)
            g_string_append(catalog->error, staticerror);
         else
            g_string_printf(catalog->error, "unknown error (%d)", retval);
      }
   else
      {
         g_string_append(catalog->error, errmsg);
         sqlite_freemem(errmsg);
      }
   return FALSE;
}

static int execute_update_nocatalog_printf(sqlite *db, const char *sql, char **errmsg, ...)
{
   va_list ap;
   va_start(ap, errmsg);
   return execute_update_nocatalog_vprintf(db, sql, errmsg, ap);
}
static int execute_update_nocatalog_vprintf(sqlite *db, const char *sql, char **errmsg, va_list ap)
{
   sqlite_busy_timeout(db, 30000/*30 seconds timout, for updates*/);

   int ret = sqlite_exec_vprintf(db,
                                 sql,
                                 NULL/*no callback*/,
                                 NULL/*no userdata*/,
                                 errmsg,
                                 ap);
   if(ret!=0)
      sqlite_exec(db, "ROLLBACK", NULL, NULL, NULL);

   sqlite_busy_timeout(db, -1/*disable, for queries*/);

   return ret;
}
static gboolean execute_update_printf(struct catalog *catalog, const char *sql, ...)
{
   va_list ap;
   va_start(ap, sql);

   char *errmsg=NULL;
   int sql_len = strlen(sql);
   char buffer[strlen("BEGIN;")+sql_len+strlen(";COMMIT;")+1];
   strcpy(buffer, "BEGIN;");
   strcat(buffer, sql);
   strcat(buffer, ";COMMIT;");
   int ret = execute_update_nocatalog_vprintf(catalog->db,
                                              buffer,
                                              &errmsg,
                                              ap);
   return handle_sqlite_retval(catalog, ret, errmsg);
}

static int progress_callback(void *userdata)
{
   struct catalog *catalog = (struct catalog *)userdata;
   return catalog->stop ? 1:0;
}

static gboolean execute_query_printf(struct catalog *catalog, sqlite_callback callback, void *userdata, const char *sql, ...)
{
   va_list ap;
   va_start(ap, sql);
   char *errmsg=NULL;

   sqlite_progress_handler(catalog->db, 1, progress_callback, catalog);
   int ret;
   do
      {
         if(catalog->stop)
            {
               ret=SQLITE_ABORT;
               break;
            }
         ret = sqlite_exec_vprintf(catalog->db,
                                   sql,
                                   callback,
                                   userdata,
                                   &errmsg,
                                   ap);
         if(ret==SQLITE_BUSY)
            {
               g_mutex_lock(catalog->busy_wait_mutex);
               if(!catalog->stop)
                  {
                     GTimeVal timeval;
                     g_get_current_time(&timeval);
                     g_time_val_add(&timeval, 10000/*1/10th of a second*/);
                     g_cond_timed_wait(catalog->busy_wait_cond, catalog->busy_wait_mutex, &timeval);
                  }
               g_mutex_unlock(catalog->busy_wait_mutex);
            }
      }
   while(ret==SQLITE_BUSY);
   sqlite_progress_handler(catalog->db, 0, NULL/*no callback*/, NULL/*no userdata*/);

   return handle_sqlite_retval(catalog, ret, errmsg);
}

static gboolean create_tables(sqlite *db, char **errmsg)
{
   int ret = execute_update_nocatalog_printf(db,
                                             "BEGIN; "
                                             "CREATE TABLE entries (id INTEGER PRIMARY KEY, "
                                             "path VARCHAR UNIQUE NOT NULL, "
                                             "name VARCHAR NOT NULL, "
                                             "long_name VARCHAR NOT NULL, "
                                             "command_id INTEGER, "
                                             "lastuse TIMESTAMP);"
                                             "CREATE INDEX lastuse_idx ON entries (lastuse DESC);"
                                             "CREATE INDEX path_idx ON entries (path);"
                                             "CREATE TABLE command (id INTEGER PRIMARY KEY , "
                                             "name VARCHAR NOT NULL, "
                                             "execute VARCHAR NOT NULL, "
                                             "lastuse TIMESTAMP);"
                                             "CREATE INDEX command_idx ON command (name);"
                                             "COMMIT;",
                                             errmsg);
   return ret==SQLITE_OK;
}

static int count(const char *str, char c)
{
   int count=0;
   for(const char *ptr = str; *ptr!='\0'; ptr++)
      {
         if(*ptr==c)
            count++;
      }
   return count;
}

/**
 * Replace %f with the path.
 *
 * @param pattern pattern with %f
 * @param path path to replace %f with
 * @return a string to free with g_free
 */
static void execute_parse(GString *gstr, const char *pattern, const char *path)
{
   const char *current=pattern;
   const char *found;
   while( (found=strstr(current, "%f")) != NULL )
      {
         gssize len = found-current;
         g_string_append_len(gstr,
                             current,
                             len);
         current+=len+2;
         g_string_append(gstr, path);
      }
   g_string_append(gstr, current);
}

static gboolean update_entry_timestamp(struct catalog *catalog, int entry_id)
{
   g_return_val_if_fail(catalog, FALSE);
   GTimeVal timeval;
   g_get_current_time(&timeval);

   return execute_update_printf(catalog,
                                "UPDATE entries SET lastuse='%16.16lx.%6.6lu' WHERE id=%d",
                                (unsigned long)timeval.tv_sec,
                                (unsigned long)timeval.tv_usec,
                                entry_id);
}

static gboolean execute_result(struct result *_self, GError **err)
{
   struct catalog_result *self = (struct catalog_result *)_self;
   g_return_val_if_fail(self, FALSE);
   g_return_val_if_fail(err==NULL || *err==NULL, FALSE);

   pid_t pid = fork();
   if(pid<0)
      {
         g_set_error(err,
                     RESULT_ERROR,
                     RESULT_ERROR_MISSING_RESOURCE,
                     "failed at fork(): %s\n",
                     strerror(errno));
         return FALSE;
      }

   if(pid==0)
      {
         /* the child */
         GString *gstr = g_string_new("");
         execute_parse(gstr, self->execute, self->base.path);
         printf("execute: /bin/sh -c '%s'\n", gstr->str);
         execl("/bin/sh", "/bin/sh", "-c", gstr->str, NULL);
         fprintf(stderr, "result.execute() failed at exec(%s): %s\n",
                 gstr->str,
                 strerror(errno));
         exit(99);
      }
   /* the parent */

   struct catalog *catalog = catalog_connect(self->catalog_path, NULL/*errmsg*/);
   if(catalog)
      {
         if(!update_entry_timestamp(catalog, self->entry_id))
            {
               fprintf(stderr,
                       "warning: lastuse timestamp update failed: %s\n",
                       catalog_error(catalog));
            }
         catalog_disconnect(catalog);
      }

   return TRUE;
}

static void free_result(struct result *self)
{
   g_return_if_fail(self);
   g_free(self);
}
static struct result *create_result(struct catalog *catalog, const char *path, const char *name, const char *execute, int entry_id)
{
   g_return_val_if_fail(catalog, NULL);
   g_return_val_if_fail(path, NULL);
   g_return_val_if_fail(name, NULL);
   g_return_val_if_fail(execute, NULL);

   struct catalog_result *result = g_malloc(sizeof(struct catalog_result)
                                            +strlen(path)+1
                                            +strlen(name)+1
                                            +strlen(execute)+1
                                            +strlen(catalog->path)+1);
   result->entry_id=entry_id;
   char *buf = result->buffer;

   result->base.path=buf;
   strcpy(buf, path);
   buf+=strlen(path)+1;
   result->base.long_name=result->base.path;

   result->base.name=buf;
   strcpy(buf, name);
   buf+=strlen(name)+1;

   result->execute=buf;
   strcpy(buf, execute);
   buf+=strlen(execute)+1;

   result->catalog_path=catalog->path;
   strcpy(buf, catalog->path);

   result->base.execute=execute_result;
   result->base.release=free_result;

   return &result->base;
}



static int result_sqlite_callback(void *userdata, int col_count, char **col_data, char **col_names)
{
   struct catalog *catalog = (struct catalog *)userdata;
   catalog_callback_f callback = catalog->callback;
   g_return_val_if_fail(callback!=NULL, 1); /* executequery called by another thread: forbidden */

   const int entry_id = atoi(col_data[0]);
   const char *path = col_data[1];
   const char *name = col_data[2];
   const char *execute = col_data[3];

   if(catalog->stop)
      return 1;

   struct result *result = create_result(catalog,
                                         path,
                                         name,
                                         execute,
                                         entry_id);
   gboolean go_on = catalog->callback(catalog,
                                  0.5/*pertinence*/,
                                  result,
                                  catalog->callback_userdata);
   return go_on && !catalog->stop ? 0:1;
}

static void get_id(struct catalog  *catalog, int *id_out)
{
   if(id_out!=NULL)
      *id_out=sqlite_last_insert_rowid(catalog->db);
}

/**
 * sqlite callback that expects an id as the 1st (and only) result
 */
static int findid_callback(void *userdata, int column_count, char **result, char **names)
{
   g_return_val_if_fail(userdata!=NULL, 1);
   g_return_val_if_fail(column_count>0, 1);
   int *id_out = (int *)userdata;
   *id_out=atoi(result[0]);
   return 1; /* no need for more results */
}
