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

/** Hidden catalog structure */
struct catalog
{
   sqlite *db;
   bool stop;
   GString* sql;
   GString* error;

   /** set in catalog_executequery to find the correct callback */
   catalog_callback_f callback;
   /** passed to callback */
   void *callback_userdata;
   GCond *busy_wait_cond;
   GMutex *busy_wait_mutex;
};

/**
 * Full structure of the results passed to the callback
 */
struct catalog_result
{
   /** catalog_result are result */
   struct result base;
   const char *execute;

   /** buffer for the path, display name and execute string */
   char buffer[];
};
static bool exists(const char *path)
{
   struct stat buf;
   return stat(path, &buf)==0 && buf.st_size>0;
}
static bool handle_sqlite_retval(struct catalog *catalog, int retval, char *errmsg)
{
   g_return_val_if_fail(catalog!=NULL, false);

   if(retval==SQLITE_OK || retval==SQLITE_ABORT || retval==SQLITE_INTERRUPT)
      return true;

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
   g_string_append_printf(catalog->error,
                          " (SQL statement: %s)",
                          catalog->sql->str);
   return false;
}
static int execute_update_nocatalog(sqlite *db, const char *sql, char **errmsg)
{
   sqlite_busy_timeout(db, 30000/*30 seconds timout, for updates*/);

   int ret = sqlite_exec(db,
                         sql,
                         NULL/*no callback*/,
                         NULL/*no userdata*/,
                         errmsg/*no errmsg*/);

   sqlite_busy_timeout(db, -1/*disable, for queries*/);

   return ret;
}
static bool execute_update(struct catalog *catalog)
{
   char *errmsg=NULL;
   int ret = execute_update_nocatalog(catalog->db,
                                      catalog->sql->str,
                                      &errmsg);
   return handle_sqlite_retval(catalog, ret, errmsg);
}

static int progress_callback(void *userdata)
{
   struct catalog *catalog = (struct catalog *)userdata;
   return catalog->stop ? 1:0;
}

static bool execute_query(struct catalog *catalog, sqlite_callback callback, void *userdata)
{
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
         ret = sqlite_exec(catalog->db,
                           catalog->sql->str,
                           callback,
                           userdata,
                           &errmsg);
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

static bool create_tables(sqlite *db, char **errmsg)
{
   int ret = execute_update_nocatalog(db,
                                      "BEGIN; "
                                      "CREATE TABLE entries (id INTEGER PRIMARY KEY, "
                                      "filename VARCHAR NOT NULL, "
                                      "directory VARCHAR, "
                                      "path VARCHAR UNIQUE NOT NULL, "
                                      "display_name VARCHAR NOT NULL, "
                                      "command_id INTEGER, "
                                      "lastuse TIMESTAMP);"
                                      "CREATE TABLE command (id INTEGER PRIMARY KEY , "
                                      "display_name VARCHAR NOT NULL, "
                                      "execute VARCHAR NOT NULL, "
                                      "lastuse TIMESTAMP);"
                                      "CREATE TABLE history (query VARCHAR NOT NULL, "
                                      "entry_id INTEGER NOT NULL, "
                                      "command_id INTEGER NOT NULL, "
                                      "lastuse TIMESTAMP NOT NULL, "
                                      "frequence FLOAT NOT NULL);"
                                      "COMMIT;",
                                      errmsg);
   return ret==SQLITE_OK;
}


struct catalog *catalog_connect(const char *path, char **_errmsg)
{
   g_return_val_if_fail(path, NULL);

   bool newdb = !exists(path);
   char *errmsg = NULL; /* not using _errmsg because I haven't solved the memory deallocation problem for it */


   sqlite *db = sqlite_open(path, 0600, &errmsg);
   if(!db)
      {
         if(errmsg)
            {
               fprintf(stderr, "opening catalog %s failed: %s\n", path, errmsg);
               sqlite_freemem(errmsg);
            }
         return NULL;
      }
   if(newdb)
      {
         if(!create_tables(db, &errmsg))
            {
               if(errmsg)
                  {
                     fprintf(stderr, "creating tables in catalog %s failed: %s\n", path, errmsg);
                     sqlite_freemem(errmsg);
                  }

               sqlite_close(db);
               return NULL;
            }
      }

   struct catalog *catalog = g_new(struct catalog, 1);
   catalog->stop=false;
   catalog->db=db;
   catalog->sql=g_string_new("");
   catalog->error=g_string_new("");
   catalog->busy_wait_cond=g_cond_new();
   catalog->busy_wait_mutex=g_mutex_new();
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
   g_string_free(catalog->sql, true/*free_segment*/);
   g_string_free(catalog->error, true/*free_segment*/);
   g_free(catalog);
}

void catalog_interrupt(struct catalog *catalog)
{
   g_return_if_fail(catalog!=NULL);
   g_mutex_lock(catalog->busy_wait_mutex);
   catalog->stop=true;
   g_cond_broadcast(catalog->busy_wait_cond);
   g_mutex_unlock(catalog->busy_wait_mutex);
}

void catalog_restart(struct catalog *catalog)
{
   g_return_if_fail(catalog!=NULL);
   g_mutex_lock(catalog->busy_wait_mutex);
   catalog->stop=false;
   g_mutex_unlock(catalog->busy_wait_mutex);
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
   printf("execute_parse(,%s,%s)->%s\n", pattern, path, gstr->str);
}

static bool execute_result(struct result *_self)
{
   struct catalog_result *self = (struct catalog_result *)_self;
   g_return_val_if_fail(self, false);
   pid_t pid = fork();
   if(pid<0)
      {
         fprintf(stderr,
                 "result.execute() failed at fork(): %s\n",
                 strerror(errno));
         return false;
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
   return true;
}

static void free_result(struct result *self)
{
   g_return_if_fail(self);
   g_free(self);
}
static struct result *create_result(const char *path, const char *display_name, const char *execute)
{
   g_return_val_if_fail(path && display_name && execute, NULL);

   struct catalog_result *result = g_malloc(sizeof(struct catalog_result)
                                            +strlen(path)+1
                                            +strlen(display_name)+1
                                            +strlen(execute)+1);
   char *buf = result->buffer;

   result->base.path=buf;
   strcpy(buf, path);
   buf+=strlen(path)+1;

   result->base.name=buf;
   strcpy(buf, display_name);
   buf+=strlen(display_name)+1;

   result->execute=buf;
   strcpy(buf, execute);

   result->base.execute=execute_result;
   result->base.release=free_result;

   return &result->base;
}



static int result_sqlite_callback(void *userdata, int col_count, char **col_data, char **col_names)
{
   struct catalog *catalog = (struct catalog *)userdata;
   catalog_callback_f callback = catalog->callback;
   g_return_val_if_fail(callback!=NULL, 1); /* executequery called by another thread: forbidden */

   const char *path = col_data[0];
   const char *display_name = col_data[1];
   const char *execute = col_data[2];

   if(catalog->stop)
      return 1;

   struct result *result = create_result(path, display_name, execute);
   bool go_on = catalog->callback(catalog,
                                  0.5/*pertinence*/,
                                  result,
                                  catalog->callback_userdata);
   return go_on && !catalog->stop ? 0:1;
}

bool catalog_executequery(struct catalog *catalog,
                          const char *query,
                          catalog_callback_f callback,
                          void *userdata)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(query!=NULL, false);
   g_return_val_if_fail(callback!=NULL, false);

   if(catalog->stop)
      return true;

   g_string_printf(catalog->sql,
                   "SELECT e.path, e.display_name, c.execute "
                   "FROM entries e, command c "
                   "WHERE e.display_name LIKE '%%%s%%' AND e.command_id=c.id "
                   "ORDER BY e.path;",
                   query);
   catalog->callback=callback;
   catalog->callback_userdata=userdata;
   bool ret = execute_query(catalog,
                            result_sqlite_callback,
                            catalog/*userdata*/);
   /* the catalog was probably called from two threads
    * at the same time: this is forbidden
    */
   g_return_val_if_fail(catalog->callback==callback,
                        false);

   catalog->callback=NULL;
   catalog->callback_userdata=NULL;

   return ret;
}

static bool insert_and_get_id(struct catalog  *catalog, int *id_out)
{
   if(execute_update(catalog))
      {
         if(id_out!=NULL)
            *id_out=sqlite_last_insert_rowid(catalog->db);
         return true;
      }
   return false;
}



bool catalog_addcommand(struct catalog *catalog, const char *name, const char *execute, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(name!=NULL, false);
   g_return_val_if_fail(execute!=NULL, false);

   g_string_printf(catalog->sql,
                   "INSERT INTO command (id, display_name, execute) VALUES (NULL, '%s', '%s');",
                   name,
                   execute);
   return insert_and_get_id(catalog, id_out);
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

bool catalog_findcommand(struct catalog *catalog, const char *name, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(name!=NULL, false);

   g_string_printf(catalog->sql,
                   "SELECT id FROM command where display_name='%s';",
                   name);
   int id=-1;

   if( execute_query(catalog, findid_callback, &id) && id!=-1)
      {
         if(id_out)
            *id_out=id;
         return true;
      }
   return false;
}

bool catalog_addentry(struct catalog *catalog, const char *directory, const char *filename, const char *display_name, int command_id, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(directory!=NULL, false);
   g_return_val_if_fail(filename!=NULL, false);
   g_return_val_if_fail(display_name!=NULL, false);

   g_string_printf(catalog->sql,
                   "INSERT INTO entries (id, directory, filename, path, display_name, command_id) VALUES (NULL, '%s', '%s', '%s/%s', '%s', %d);",
                   directory,
                   filename,
                   directory,
                   filename,
                   display_name,
                   command_id);
   return insert_and_get_id(catalog, id_out);
}
