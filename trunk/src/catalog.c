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
#include "mempool.h"

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

   struct mempool *escape_mempool;

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
static bool exists(const char *path);
static bool handle_sqlite_retval(struct catalog *catalog, int retval, char *errmsg);
static int execute_update_nocatalog(sqlite *db, const char *sql, char **errmsg);
static bool execute_update(struct catalog *catalog);
static int progress_callback(void *userdata);
static bool execute_query(struct catalog *catalog, sqlite_callback callback, void *userdata);
static bool create_tables(sqlite *db, char **errmsg);
static int count(const char *str, char c);
static const char *escape(struct catalog *catalog, const char *orig);
static void escape_free(struct catalog *catalog);
static void execute_parse(GString *gstr, const char *pattern, const char *path);
static bool update_entry_timestamp(struct catalog *catalog, int entry_id);
static bool execute_result(struct result *_self, GError **);
static void free_result(struct result *self);
static struct result *create_result(struct catalog *catalog, const char *path, const char *display_name, const char *execute, int entry_id);
static int result_sqlite_callback(void *userdata, int col_count, char **col_data, char **col_names);
static bool insert_and_get_id(struct catalog  *catalog, int *id_out);
static int findid_callback(void *userdata, int column_count, char **result, char **names);


/* ------------------------- public functions */

GQuark catalog_error_quark()
{
   static GQuark catalog_quark;
   static bool initialized;
   if(!initialized)
      {
         catalog_quark=g_quark_from_static_string("CATALOG_ERROR");
         initialized=true;
      }
   return catalog_quark;
}

struct catalog *catalog_connect(const char *path, GError **err)
{
   g_return_val_if_fail(path, NULL);
   g_return_val_if_fail(err==NULL || *err==NULL, NULL);

   bool newdb = !exists(path);
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
   catalog->stop=false;
   catalog->db=db;
   catalog->sql=g_string_new("");
   catalog->error=g_string_new("");
   catalog->busy_wait_cond=g_cond_new();
   catalog->busy_wait_mutex=g_mutex_new();
   catalog->escape_mempool=NULL;
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
   g_string_free(catalog->sql, true/*free_segment*/);
   g_string_free(catalog->error, true/*free_segment*/);
   escape_free(catalog);
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


   g_string_assign(catalog->sql,
                   "SELECT e.id, e.path, e.display_name, c.execute, e.lastuse "
                   "FROM entries e, command c "
                   "WHERE e.display_name LIKE '%");
   bool has_space=false;
   for(const char *cptr=query; *cptr!='\0'; cptr++)
      {
         char c = *cptr;
         switch(c)
            {
            case ' ':
               has_space=true;
               break;

            case '\'':
            case '"':
               break;

            default:
               if(has_space)
                  {
                     g_string_append(catalog->sql,
                                     "%' AND e.display_name LIKE '%");
                     has_space=false;
                  }
               g_string_append_c(catalog->sql,
                               c);
            }
      }
   g_string_append(catalog->sql,
                   "%' AND e.command_id=c.id "
                   "ORDER BY e.lastuse DESC;");
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



bool catalog_addcommand(struct catalog *catalog, const char *name, const char *execute, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(name!=NULL, false);
   g_return_val_if_fail(execute!=NULL, false);

   int old_id = -1;
   if(catalog_findcommand(catalog, name, &old_id))
      {
         g_string_printf(catalog->sql,
                         "UPDATE command SET execute='%s' WHERE id=%d;",
                         escape(catalog, execute),
                         old_id);
         escape_free(catalog);
         if(id_out)
            *id_out=old_id;
         return execute_update(catalog);
      }
   else
      {
         g_string_printf(catalog->sql,
                         "INSERT INTO command (id, display_name, execute) VALUES (NULL, '%s', '%s');",
                         escape(catalog, name),
                         escape(catalog, execute));
         escape_free(catalog);
         return insert_and_get_id(catalog, id_out);
      }
}


bool catalog_findcommand(struct catalog *catalog, const char *name, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(name!=NULL, false);

   g_string_printf(catalog->sql,
                   "SELECT id FROM command where display_name='%s';",
                   escape(catalog, name));
   escape_free(catalog);
   int id=-1;

   if( execute_query(catalog, findid_callback, &id) && id!=-1)
      {
         if(id_out)
            *id_out=id;
         return true;
      }
   return false;
}

bool catalog_findentry(struct catalog *catalog, const char *path, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(path!=NULL, false);

   g_string_printf(catalog->sql,
                   "SELECT id FROM entries WHERE path='%s';",
                   escape(catalog, path));
   escape_free(catalog);
   int id=-1;

   if(execute_query(catalog, findid_callback, &id) && id!=-1)
      {
         if(id_out)
            *id_out=id;
         return true;
      }
   return false;
}

bool catalog_addentry(struct catalog *catalog, const char *path, const char *display_name, int command_id, int *id_out)
{
   g_return_val_if_fail(catalog!=NULL, false);
   g_return_val_if_fail(path!=NULL, false);
   g_return_val_if_fail(display_name!=NULL, false);

   int old_id=-1;
   if(catalog_findentry(catalog, path, &old_id))
      {
         g_string_printf(catalog->sql,
                         "UPDATE entries SET display_name='%s', command_id=%d WHERE id=%d;",
                         escape(catalog, display_name),
                         command_id,
                         old_id);
         escape_free(catalog);
         return execute_update(catalog);
      }
   else
      {
         g_string_printf(catalog->sql,
                         "INSERT INTO entries (id, path, display_name, command_id) VALUES (NULL, '%s', '%s', %d);",
                         escape(catalog, path),
                         escape(catalog, display_name),
                         command_id);
         escape_free(catalog);
         return insert_and_get_id(catalog, id_out);
      }
}

/* ------------------------- static functions */
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
                         errmsg);
   if(ret!=0)
      sqlite_exec(db, "ROLLBACK;", NULL, NULL, NULL);

   sqlite_busy_timeout(db, -1/*disable, for queries*/);

   return ret;
}
static bool execute_update(struct catalog *catalog)
{
   char *errmsg=NULL;
   g_string_prepend(catalog->sql, "BEGIN;");
   if(catalog->sql->str[catalog->sql->len-1]!=';')
      g_string_append_c(catalog->sql, ';');
   g_string_append(catalog->sql, "COMMIT;");
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
                                      "path VARCHAR UNIQUE NOT NULL, "
                                      "display_name VARCHAR NOT NULL, "
                                      "command_id INTEGER, "
                                      "lastuse TIMESTAMP);"
                                      "CREATE INDEX lastuse_idx ON entries (lastuse DESC);"
                                      "CREATE INDEX path_idx ON entries (path);"
                                      "CREATE TABLE command (id INTEGER PRIMARY KEY , "
                                      "display_name VARCHAR NOT NULL, "
                                      "execute VARCHAR NOT NULL, "
                                      "lastuse TIMESTAMP);"
                                      "CREATE INDEX command_idx ON command (display_name);"
                                      "CREATE TABLE history (query VARCHAR NOT NULL, "
                                      "entry_id INTEGER NOT NULL, "
                                      "command_id INTEGER NOT NULL, "
                                      "lastuse TIMESTAMP NOT NULL, "
                                      "frequence FLOAT NOT NULL);"
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
static const char *escape(struct catalog *catalog, const char *orig)
{
   int quotes = count(orig, '\'');
   if(quotes==0)
      return orig;
   if(!catalog->escape_mempool)
      catalog->escape_mempool=mempool_new();
   char *retval = (char *)mempool_alloc(catalog->escape_mempool, strlen(orig)+quotes+1);
   char *retval_ptr = retval;
   const char *orig_ptr = orig;
   while(*orig_ptr!='\0')
      {
         *retval_ptr=*orig_ptr;
         retval_ptr++;
         if(*orig_ptr=='\'')
            {
               *retval_ptr='\'';
               retval_ptr++;
            }
         orig_ptr++;
      }
   *retval_ptr='\0';
   return retval;
}
static void escape_free(struct catalog *catalog)
{
   if(catalog->escape_mempool)
      {
         mempool_delete(catalog->escape_mempool);
         catalog->escape_mempool=NULL;
      }
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

static bool update_entry_timestamp(struct catalog *catalog, int entry_id)
{
   g_return_val_if_fail(catalog, false);
   GTimeVal timeval;
   g_get_current_time(&timeval);

   g_string_printf(catalog->sql,
                   "UPDATE entries SET lastuse='%16.16lx.%6.6lu' WHERE id=%d;",
                   (unsigned long)timeval.tv_sec,
                   (unsigned long)timeval.tv_usec,
                   entry_id);
   return execute_update(catalog);
}

static bool execute_result(struct result *_self, GError **err)
{
   struct catalog_result *self = (struct catalog_result *)_self;
   g_return_val_if_fail(self, false);
   g_return_val_if_fail(err==NULL || *err==NULL, false);

   pid_t pid = fork();
   if(pid<0)
      {
         g_set_error(err,
                     RESULT_ERROR,
                     RESULT_ERROR_MISSING_RESOURCE,
                     "failed at fork(): %s\n",
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

   return true;
}

static void free_result(struct result *self)
{
   g_return_if_fail(self);
   g_free(self);
}
static struct result *create_result(struct catalog *catalog, const char *path, const char *display_name, const char *execute, int entry_id)
{
   g_return_val_if_fail(catalog, NULL);
   g_return_val_if_fail(path, NULL);
   g_return_val_if_fail(display_name, NULL);
   g_return_val_if_fail(execute, NULL);

   struct catalog_result *result = g_malloc(sizeof(struct catalog_result)
                                            +strlen(path)+1
                                            +strlen(display_name)+1
                                            +strlen(execute)+1
                                            +strlen(catalog->path)+1);
   result->entry_id=entry_id;
   char *buf = result->buffer;

   result->base.path=buf;
   strcpy(buf, path);
   buf+=strlen(path)+1;

   result->base.name=buf;
   strcpy(buf, display_name);
   buf+=strlen(display_name)+1;

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
   const char *display_name = col_data[2];
   const char *execute = col_data[3];

   if(catalog->stop)
      return 1;

   struct result *result = create_result(catalog,
                                         path,
                                         display_name,
                                         execute,
                                         entry_id);
   bool go_on = catalog->callback(catalog,
                                  0.5/*pertinence*/,
                                  result,
                                  catalog->callback_userdata);
   return go_on && !catalog->stop ? 0:1;
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
