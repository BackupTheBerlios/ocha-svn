#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "catalog.h"
#include <stdio.h>
#include <check.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sqlite.h>
#include <fcntl.h>

/** database file */
#define PATH ".catalog_check.db"
/** named pipe */
#define PIPE ".catalog_check.pipe"

static struct catalog *catalog;
static int command_id;
struct entry_definition
{
   const char *path;
   const char *filename;
   int id;
};
struct entry_definition entries[] = {
   { "/tmp/toto.c", "toto.c"},
   { "/tmp/toto.h", "toto.h"},
   { "/tmp/total.h", "total.h"},
   { "/tmp/etalma.c", "etalma.c"},
   { "/tmp/talm.c", "talm.c"},
   { "/tmp/talm.h", "talm.h"},
   { "/tmp/hello.txt", "hello.txt"},
   { "/tmp/hullo.txt", "hullo.txt"}
};
#define entries_length (sizeof(entries)/sizeof(struct entry_definition))

static void catalog_cmd(struct catalog *catalog, const char *comment, bool ret)
{
   if(ret)
      return;
   GString *msg = g_string_new("");
   g_string_printf(msg,
                   "catalog command %s failed: %s\n",
                   comment,
                   catalog_error(catalog));
   fail(msg->str);
   g_string_free(msg, true/*free content*/);
}
static bool exists(const char *path)
{
   struct stat buf;
   if(stat(path, &buf)!=0)
      return false;
   return buf.st_size>0;
}

static void unlink_if_exists(const char *path)
{
   unlink(path);
   fail_unless(!exists(path),
               path);
}

static void setup()
{
   printf("---\n");
   g_thread_init_with_errorcheck_mutexes(NULL/*vtable*/);
   unlink_if_exists(PATH);
}

static void teardown()
{
   unlink(PATH);
   printf("--- end\n");
}

static void setup_query()
{
   setup();
   catalog=catalog_connect(PATH, NULL);

   fail_unless(catalog_addcommand(catalog,
                                  "command",
                                  "echo %f",
                                  &command_id),
               "command could not be created");
   for(int i=0; i<entries_length; i++)
      {
         fail_unless(catalog_addentry(catalog,
                                      entries[i].path,
                                      entries[i].filename,
                                      command_id,
                                      &entries[i].id),
                     "entry could not be created");
      }


}

static void teardown_query()
{
   catalog_disconnect(catalog);
   teardown();
}


static void setup_execute()
{
   setup();

   unlink_if_exists(PIPE);
   fail_unless(mkfifo(PIPE, 0600)==0, "pipe creation failed");

   catalog=catalog_connect(PATH, NULL);

   fail_unless(catalog_addcommand(catalog,
                                  "command",
                                  "echo execute %f >" PIPE,
                                  &command_id),
               "command could not be created");

   fail_unless(catalog_addentry(catalog,
                                "/tmp/hello.c",
                                "hello.c",
                                command_id,
                                NULL),
               "entry could not be created");
}

static void teardown_execute()
{
   teardown();
}

START_TEST(test_create)
{
   printf("--- test_create\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);
   fail_unless(catalog!=NULL, "NULL catalog");
   fail_unless(exists(PATH), "catalog file not created in " PATH);
   catalog_disconnect(catalog);
}
END_TEST


START_TEST(test_addcommand)
{
   printf("--- test_addcommand\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);
   int id = -1;
   fail_unless(!catalog_findcommand(catalog, "edit", &id),
               "found edit command that shouldn't be there");
   id=-1;
   catalog_cmd(catalog,
               "addcommand(edit)",
               catalog_addcommand(catalog, "edit", "edit %s", &id));
   fail_unless(id!=-1,
               "id not modified");

   int find_id = -1;
   catalog_cmd(catalog,
               "findcommand(edit)",
               catalog_findcommand(catalog, "edit", &find_id));

   fail_unless(find_id==id,
               "wrong id for findcommand");
   catalog_disconnect(catalog);
}
END_TEST

START_TEST(test_addcommand_escape)
{
   printf("--- test_addcommand_escape\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);
   fail_unless(catalog_addcommand(catalog, "ed'it", "edit '%s'", NULL),
               "addcommand failed");
   catalog_disconnect(catalog);
}
END_TEST

START_TEST(test_addcommand_noduplicate)
{
   printf("--- test_addcommand_noduplicate\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);
   int id_1 = -1;
   fail_unless(catalog_addcommand(catalog, "edit", "edit %s", &id_1),
               "1st addcommand failed");
   int id_2 = -1;
   fail_unless(catalog_addcommand(catalog, "edit", "edit2 %s", &id_2),
               "2nd addcommand failed");
   fail_unless(id_1==id_2, "different ids");

   int id_found = -1;
   catalog_findcommand(catalog, "edit", &id_found);
   fail_unless(id_1==id_found, "different ids");
}
END_TEST

START_TEST(test_addentry)
{
   printf("--- test_addentry\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);

   int command_id=-1;
   catalog_addcommand(catalog, "edit", "edit %s", &command_id);

   int entry_id=-1;
   catalog_cmd(catalog,
               "1st addentry(/tmp/toto.txt)",
               catalog_addentry(catalog, "/tmp/toto.txt", "Toto", command_id, &entry_id));

   catalog_disconnect(catalog);
}
END_TEST

START_TEST(test_addentry_escape)
{
   printf("--- test_addentry_escape\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);

   catalog_cmd(catalog,
               "1st addentry(/tmp/toto.txt)",
               catalog_addentry(catalog, "/tmp/to'to.txt", "To'to", command_id, NULL));

   catalog_disconnect(catalog);
}
END_TEST

START_TEST(test_addentry_noduplicate)
{
   printf("--- test_addentry_noduplicate\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);

   int command_id=-1;
   catalog_addcommand(catalog, "edit", "edit %s", &command_id);

   int entry_id_1=-1;
   fail_unless(catalog_addentry(catalog, "/tmp/toto.txt", "Toto", command_id, &entry_id_1),
               "1st addentry failed");
   int entry_id_2=-1;
   fail_unless(catalog_addentry(catalog, "/tmp/toto.txt", "Toto", command_id, &entry_id_2),
               "2nd addentry failed");

   catalog_disconnect(catalog);
}
END_TEST


/**
 * Keep the result into the list
 * @param catalog ignored
 * @param pertinence ignored
 * @param result added into the list
 * @param list the list itself (passed by catalog_executequery as userdata)
 * @see #results
 */
static bool collect_results_callback(struct catalog *catalog, float pertinence, struct result *result, void *userdata)
{
   GList **results = (GList **)userdata;
   g_return_val_if_fail(results, false);
   *results=g_list_append(*results,
                          result);
   printf("result %d: %s\n", g_list_length(*results), result->name);
   return true;/*continue*/
}

/**
 * GFunc that release a result passed as data
 *
 * @param data a result
 * @param userdata ignored
 */
static void free_result(gpointer data, gpointer userdata)
{
   struct result *result = (struct result *)data;
   g_return_if_fail(result!=NULL);
   result->release(result);
}
static void execute_query_and_expect(const char *query, int goal_length, char *goal[], bool ordered)
{
   GList *list = NULL;
   catalog_cmd(catalog,
               "executequery()",
               catalog_executequery(catalog, query, collect_results_callback, &list));
   int found_length = g_list_length(list);
   const char *found[found_length];
   for(int i=0; i<found_length; i++)
      found[i]=((struct result *)g_list_nth_data(list, i))->name;

   GString* msg = g_string_new("");
   g_string_append(msg, "query: '");
   g_string_append(msg, query);
   g_string_append(msg, "' found: [");
   for(int i=0; i<found_length; i++)
      {
         g_string_append_c(msg, ' ');
         g_string_append(msg, found[i]);
      }
   g_string_append(msg, " ] expected: [");
   for(int i=0; i<goal_length; i++)
      {
         g_string_append_c(msg, ' ');
         g_string_append(msg, goal[i]);
      }
   g_string_append(msg, " ] - ");
   if(goal_length!=found_length)
      {
         g_string_append(msg, "wrong length");
         fail(msg->str);
      }
   if(ordered)
      {
         for(int i=0; i<goal_length; i++)
            {
               if(strcmp(goal[i], found[i])!=0)
                  {
                     g_string_append_printf(msg,
                                            "expected '%s' at index %d, got '%s",
                                            goal[i],
                                            i,
                                            found[i]);
                     fail(msg->str);
                  }
            }
      }
   else /* not ordered */
      {
         for(int i=0; i<goal_length; i++)
            {
               bool ok=false;
               for(int j=0; j<found_length; j++)
                  {
                     if(found[j]!=NULL && strcmp(goal[i], found[j])==0)
                        {
                           found[j]=NULL;
                           ok=true;
                           break;
                        }
                  }
               if(!ok)
                  {
                     g_string_append_printf(msg, "%s not found", goal[i]);
                     fail(msg->str);
                  }
            }
      }
   g_list_foreach(list, free_result, NULL/*userdata*/);
   g_list_free(list);
}

START_TEST(test_execute_query)
{
   printf("--- test_execute_query\n");
   execute_query_and_expect("toto.c",
                            1,
                            (char *[]){ "toto.c" },
                            false/*not ordered*/);
}
END_TEST

static bool execute_result(GList *list, const char *name)
{
   bool executed=false;
   for(GList *current = list; current; current=g_list_next(current))
      {
         struct result *result=current->data;
         if(strcmp(name, result->name)==0)
            {
               executed=true;
               result->execute(result);
            }
      }
   return executed;
}


START_TEST(test_lastexecuted_first)
{
   printf("--- test_lastexecuted_first\n");
   GList *list = NULL;
   catalog_cmd(catalog,
               "test_lastexecuted_first()",
               catalog_executequery(catalog, "tot", collect_results_callback, &list));
   fail_unless(execute_result(list, "total.h"), "total.h");
   fail_unless(execute_result(list, "toto.h"), "toto.h");
   execute_query_and_expect("tot",
                            3,
                            (char *[]){ "toto.h", "total.h", "toto.c" },
                            true/*ordered*/);
}
END_TEST

START_TEST(test_execute_query_with_space)
{
   printf("--- test_execute_query\n");
   execute_query_and_expect("to .c",
                            1,
                            (char *[]){ "toto.c" },
                            false/*not ordered*/);
}
END_TEST

/**
 * Let the query return only that many result and return false.
 *
 * @param catalog ignored
 * @param pertinence ignored
 * @param result released
 * @param userdata a pointer to an integer (the counter), which will be decremented to 0
 */
static bool countdown_callback(struct catalog *catalog, float pertinence, struct result *result, void *userdata)
{
   int *counter = (int *)userdata;
   g_return_val_if_fail(counter!=NULL, false);
   fail_unless( (*counter)>0, "callback called once too many");

   (*counter)--;
   result->release(result);
   return (*counter)>0;
}
START_TEST(test_callback_stops_query)
{
   printf("--- test_callback_stops_query\n");
   int count=1;
   catalog_cmd(catalog,
               "executequery(t)",
               catalog_executequery(catalog, "t", countdown_callback, &count));
   fail_unless(count==0, "wrong count");
}
END_TEST

/**
 * When the counter rearches 0, call catalog_interrupt
 *
 * @param catalog ignored
 * @param pertinence ignored
 * @param result released
 * @param userdata a pointer to an integer (the counter), which will be decremented to 0
 */
static bool countdown_interrupt_callback(struct catalog *catalog, float pertinence, struct result *result, void *userdata)
{
   int *counter = (int *)userdata;
   g_return_val_if_fail(counter!=NULL, false);
   fail_unless( (*counter)>0, "countdown_interrupt_callback called once too many");

   (*counter)--;

   result->release(result);
   if( (*counter) == 0 )
      catalog_interrupt(catalog);
   return true/*continue*/;
}
START_TEST(test_interrupt_stops_query)
{
   printf("--- test_interrupt_stops_query\n");
   int count=1;
   catalog_cmd(catalog,
               "executequery(t)",
               catalog_executequery(catalog, "t", countdown_interrupt_callback, &count));
   fail_unless(count==0, "wrong count");
}
END_TEST

START_TEST(test_recover_from_interruption)
{
   printf("--- test_recover_from_interruption\n");
   int count=1;
   catalog_cmd(catalog,
               "executequery(t)",
               catalog_executequery(catalog, "t", countdown_interrupt_callback, &count));
   fail_unless(count==0, "wrong count");

   GList *list = NULL;
   catalog_cmd(catalog,
               "executequery(t)",
               catalog_executequery(catalog, "t", collect_results_callback, &list));
   fail_unless(list==NULL, "a result has been added after the catalog has been interrupted");

   catalog_restart(catalog);
   catalog_cmd(catalog,
               "executequery(t)",
               catalog_executequery(catalog, "t", collect_results_callback, &list));
   fail_unless(list!=NULL, "catalog has not recovered from an interruption");
}
END_TEST

static int sleeper_progress_handler(void *userdata)
{
   sleep(1);
   return 0;/*continue*/
}

GMutex *execute_query_mutex;
GCond  *execute_query_cond;

static gpointer execute_query_thread(void *userdata)
{
   bool *stop = (bool *)userdata;
   char *errmsg=NULL;
   sqlite *db=sqlite_open(PATH, 0600, &errmsg);
   if(!db)
      {
         if(errmsg)
            printf("sqlite error: %s\n", errmsg);
         fail("sqlite_open() failed");
      }

   g_mutex_lock(execute_query_mutex);
   printf("execute_query_thread: lock\n");
   sqlite_exec(db,
               "BEGIN; INSERT INTO entries VALUES (NULL, '', '', '', '');",
               NULL/*no callback*/,
               NULL/*userdata*/,
               &errmsg);
   if(errmsg)
      {
         printf("sqlite error: %s\n", errmsg);
         fail("sqlite_exec(lock) failed");
      }
   printf("execute_query_thread: wait (before interrupt)\n");
   g_cond_broadcast(execute_query_cond);
   g_cond_wait(execute_query_cond, execute_query_mutex);
   printf("execute_query_thread: sleep (before interrupt)\n");
   sleep(2);
   printf("execute_query_thread: interrupt\n");
   catalog_interrupt(catalog);
   printf("execute_query_thread: wait (before rollback)\n");
   g_cond_wait(execute_query_cond, execute_query_mutex);
   printf("execute_query_thread: sleep (before rollback)\n");
   sleep(2);
   printf("execute_query_thread: rollback\n");
   sqlite_exec(db,
               "ROLLBACK;",
               NULL/*no callback*/,
               NULL/*userdata*/,
               &errmsg);
   if(errmsg)
      {
         printf("sqlite error: %s\n", errmsg);
         fail("sqlite_exec(rollback) failed");
      }

   sqlite_close(db);
   printf("execute_query_thread: wait (before end)\n");
   g_cond_wait(execute_query_cond, execute_query_mutex);
   printf("execute_query_thread: done\n");
   g_mutex_unlock(execute_query_mutex);

   return NULL;
}
START_TEST(test_busy)
{
   printf("--- test_busy\n");
   execute_query_mutex = g_mutex_new();
   execute_query_cond = g_cond_new();

   g_mutex_lock(execute_query_mutex);
   GThread *thread = g_thread_create(execute_query_thread,
                                     NULL/*userdata*/,
                                     false/*not joinable*/,
                                     NULL);
   fail_unless(thread!=NULL,
               "thread creation failed");

   printf("test_busy: broadcast (interrupt)\n");

   g_cond_wait(execute_query_cond, execute_query_mutex);
   g_cond_broadcast(execute_query_cond); /* tell the thread to lock and interrupt us in a few seconds*/
   g_mutex_unlock(execute_query_mutex);

   printf("test_busy: execute query\n");
   execute_query_and_expect("toto.c",
                            0,
                            (char *[]) { NULL }, false/*not ordered*/);
   printf("test_busy: query done\n");
   catalog_restart(catalog);

   g_mutex_lock(execute_query_mutex);

   g_cond_broadcast(execute_query_cond); /* tell the thread to rollback in a few seconds*/
   g_mutex_unlock(execute_query_mutex);


   execute_query_and_expect("toto.c",
                            1,
                            (char *[]) { "toto.c" }, false/*not ordered*/);

   g_mutex_lock(execute_query_mutex);
   g_cond_broadcast(execute_query_cond); /* tell the thread to finish*/
   g_mutex_unlock(execute_query_mutex);
}
END_TEST

START_TEST(test_execute)
{
   printf("--- test_execute\n");
   GList *list = NULL;
   catalog_executequery(catalog, "hello", collect_results_callback, &list);
   fail_unless(g_list_length(list)>=1, "result not available");

   printf("open pipe\n");
   int pipe_fd = open(PIPE, O_NONBLOCK);

   printf("execute result\n");
   /* child */
   struct result *result = (struct result *)g_list_nth_data(list, 0);
   result->execute(result);

   printf("read from pipe\n");
   int buffer_len=1024;
   char buffer[buffer_len+1];
   int sofar=0;

   while(true)
      {
         ssize_t bytes = read(pipe_fd, &buffer[sofar], buffer_len-sofar);
         if(bytes>0)
            {
               sofar+=bytes;
               buffer[sofar]='\0';
               if(strchr(buffer, '\n'))
                  break;
            }
      }
   close(pipe_fd);

   printf("read: '%s'\n", buffer);
   fail_unless(strcmp("execute /tmp/hello.c\n", buffer)==0,
               "wrong content for pipe");
}
END_TEST

Suite *catalog_check_suite(void)
{
   Suite *s = suite_create("catalog");
   TCase *tc_core = tcase_create("catalog_core");
   TCase *tc_query = tcase_create("catalog_query");
   TCase *tc_execute = tcase_create("catalog_execute");

   tcase_add_checked_fixture(tc_core, setup, teardown);
   suite_add_tcase(s, tc_core);
   tcase_add_test(tc_core, test_create);
   tcase_add_test(tc_core, test_addcommand);
   tcase_add_test(tc_core, test_addcommand_noduplicate);
   tcase_add_test(tc_core, test_addcommand_escape);
   tcase_add_test(tc_core, test_addentry);
   tcase_add_test(tc_core, test_addentry_noduplicate);
   tcase_add_test(tc_core, test_addentry_escape);

   tcase_add_checked_fixture(tc_query, setup_query, teardown_query);
   suite_add_tcase(s, tc_query);
   tcase_add_test(tc_query, test_execute_query);
   tcase_add_test(tc_query, test_execute_query_with_space);
   tcase_add_test(tc_query, test_callback_stops_query);
   tcase_add_test(tc_query, test_interrupt_stops_query);
   tcase_add_test(tc_query, test_recover_from_interruption);
   tcase_add_test(tc_query, test_busy);
   tcase_add_test(tc_query, test_lastexecuted_first);

   tcase_add_checked_fixture(tc_execute, setup_execute, teardown_execute);
   suite_add_tcase(s, tc_execute);
   tcase_add_test(tc_execute, test_execute);

   return s;
}

int main(void)
{
   int nf;
   Suite *s = catalog_check_suite ();
   SRunner *sr = srunner_create (s);
   srunner_run_all (sr, CK_NORMAL);
   nf = srunner_ntests_failed (sr);
   srunner_free (sr);
   suite_free (s);
   return (nf == 0) ? 0:10;
}

