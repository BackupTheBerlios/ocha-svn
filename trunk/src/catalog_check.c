#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "catalog.h"
#include "catalog_result.h"
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

static struct catalog *catalog;
static int source_id;
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

static void catalog_cmd(struct catalog *catalog, const char *comment, gboolean ret)
{
   if(ret)
      return;
   GString *msg = g_string_new("");
   g_string_printf(msg,
                   "catalog function %s failed: %s\n",
                   comment,
                   catalog_error(catalog));
   fail(msg->str);
   g_string_free(msg, TRUE/*free content*/);
}
static gboolean exists(const char *path)
{
   struct stat buf;
   if(stat(path, &buf)!=0)
      return FALSE;
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

   fail_unless(catalog_add_source(catalog,
                                  "test",
                                  &source_id),
               "source could not be created");
   for(int i=0; i<entries_length; i++)
      {
         fail_unless(catalog_addentry(catalog,
                                      entries[i].path,
                                      entries[i].filename,
                                      entries[i].path/*long_name*/,
                                      source_id,
                                      &entries[i].id),
                     "entry could not be created");
      }


}

static void teardown_query()
{
   catalog_disconnect(catalog);
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


START_TEST(test_add_source)
{
   printf("--- test_add_source\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);
   int id = -1;
   int count = -1;
   catalog_cmd(catalog,
               "list_sources('test')",
               catalog_list_sources(catalog, "test", NULL/*ids_out*/, &count));
   fail_unless(count==0,
               "found test source(s) that shouldn't be there");

   id=-1;
   catalog_cmd(catalog,
               "add_source(edit)",
               catalog_add_source(catalog, "test", &id));
   fail_unless(id!=-1,
               "id not modified");

   int *all_sources = NULL;
   catalog_cmd(catalog,
               "list_sources('test')",
               catalog_list_sources(catalog, "test", &all_sources, &count));
   fail_unless(count==1,
               g_strdup_printf("expected add_source to have added one 'test' source, not %d", count));
   fail_unless(all_sources!=NULL,
               "source array not set");
   fail_unless(all_sources[0]==id,
               "expected one source with the id of the one I just added");

   g_free(all_sources);

   catalog_disconnect(catalog);

   printf("--- test_add_source_OK\n");
}
END_TEST

START_TEST(test_add_source_escape)
{
   printf("--- test_add_source_escape\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);
   int id;
   fail_unless(catalog_add_source(catalog, "te'st", &id),
               "addsource failed");
   catalog_disconnect(catalog);
   printf("--- test_add_source_escape_OK\n");
}
END_TEST

START_TEST(test_list_sources)
{
   printf("--- test_list_sources\n");

   struct catalog *catalog=catalog_connect(PATH, NULL);

   int test1_source_1 = -1;
   int test1_source_2 = -1;
   int test2_source_1 = -1;
   catalog_cmd(catalog,
               "add_source(test1 (1st))",
               catalog_add_source(catalog, "test1", &test1_source_1));
   catalog_cmd(catalog,
               "add_source(test1 (2nd))",
               catalog_add_source(catalog, "test1", &test1_source_2));
   catalog_cmd(catalog,
               "add_source(test2)",
               catalog_add_source(catalog, "test2", &test2_source_1));

   int *test1_sources;
   int test1_sources_count;
   catalog_cmd(catalog,
               "list_sources('test1')",
               catalog_list_sources(catalog, "test1", &test1_sources, &test1_sources_count));
   fail_unless(test1_sources!=NULL,
               "test1_sources not set");
   fail_unless(test1_sources_count==2,
               "wrong count for test1 sources (2 added before)");
   fail_unless((test1_sources[0]==test1_source_1 && test1_sources[1]==test1_source_2)
               || (test1_sources[0]==test1_source_2 && test1_sources[1]==test1_source_1),
               "expected [test1_source1, test1_source2]");
   g_free(test1_sources);

   int *test2_sources;
   int test2_sources_count;
   catalog_cmd(catalog,
               "list_sources('test2')",
               catalog_list_sources(catalog, "test2", &test2_sources, &test2_sources_count));
   fail_unless(test2_sources!=NULL,
               "test2_sources not set");
   fail_unless(test2_sources_count==1,
               "wrong count for test2 sources (1 added before)");
   fail_unless(test2_sources[0]==test2_source_1,
               "expected test2_source1");


   int *test3_sources = (int *)0xf001;
   int test3_sources_count = 92;
   catalog_cmd(catalog,
               "list_sources('test3')",
               catalog_list_sources(catalog, "test3", &test3_sources, &test3_sources_count));
   fail_unless(test3_sources==NULL,
               "test3_sources not set to NULL");
   fail_unless(test3_sources_count!=1,
               "wrong count for test3 sources (expected 0)");

   printf("--- test_list_sources_OK\n");
}
END_TEST

START_TEST(test_source_attributes)
{
   printf("--- test_source_attributes\n");

   struct catalog *catalog=catalog_connect(PATH, NULL);

   int source1 = -1;
   int source2 = -1;
   catalog_cmd(catalog,
               "add_source(test1, source1)",
               catalog_add_source(catalog, "test", &source1));
   catalog_cmd(catalog,
               "add_source(test1, source2)",
               catalog_add_source(catalog, "test", &source2));


   catalog_cmd(catalog,
               "set(index, 1)",
               catalog_set_source_attribute(catalog,
                                            source1,
                                            "index",
                                            "1"));
   catalog_cmd(catalog,
               "set(index, 2)",
               catalog_set_source_attribute(catalog,
                                            source2,
                                            "index",
                                            "2"));
   catalog_cmd(catalog,
               "set(is_source1, yes)",
               catalog_set_source_attribute(catalog,
                                            source1,
                                            "is_source1",
                                            "yes"));
   char *index_from_1 = NULL;
   char *index_from_2 = NULL;
   char *is_source1_from_1 = NULL;
   char *is_source1_from_2 = (char *)0xf001;

   catalog_cmd(catalog,
               "get(1, index)",
               catalog_get_source_attribute(catalog,
                                            source1,
                                            "index",
                                            &index_from_1));
   catalog_cmd(catalog,
               "get(2, index)",
               catalog_get_source_attribute(catalog,
                                            source2,
                                            "index",
                                            &index_from_2));

   catalog_cmd(catalog,
               "get(1, is_source1)",
               catalog_get_source_attribute(catalog,
                                            source1,
                                            "is_source1",
                                            &is_source1_from_1));
   catalog_cmd(catalog,
               "get(2, is_source1)",
               catalog_get_source_attribute(catalog,
                                            source2,
                                            "is_source1",
                                            &is_source1_from_2));

   fail_unless(strcmp(index_from_1, "1")==0,
               "wrong index_from_1");
   fail_unless(strcmp(index_from_2, "2")==0,
               "wrong index_from_2");
   fail_unless(strcmp(is_source1_from_1, "yes")==0,
               "wrong is_source1_from_1");
   fail_unless(is_source1_from_2==NULL,
               "wrong is_source1_from_2");

   printf("--- test_source_attributes OK\n");
}
END_TEST

START_TEST(test_addentry)
{
   printf("--- test_addentry\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);

   int source_id=-1;
   catalog_add_source(catalog, "test", &source_id);

   int entry_id=-1;
   catalog_cmd(catalog,
               "1st addentry(/tmp/toto.txt)",
               catalog_addentry(catalog, "/tmp/toto.txt", "Toto", "/tmp/toto.txt", source_id, &entry_id));

   catalog_disconnect(catalog);
}
END_TEST

START_TEST(test_addentry_escape)
{
   printf("--- test_addentry_escape\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);

   catalog_cmd(catalog,
               "1st addentry(/tmp/toto.txt)",
               catalog_addentry(catalog, "/tmp/to'to.txt", "To'to", "/tmp/toto.txt", source_id, NULL));

   catalog_disconnect(catalog);
}
END_TEST

START_TEST(test_addentry_noduplicate)
{
   printf("--- test_addentry_noduplicate\n");
   struct catalog *catalog=catalog_connect(PATH, NULL);

   int source_id=-1;
   catalog_add_source(catalog, "test", &source_id);

   int entry_id_1=-1;
   fail_unless(catalog_addentry(catalog, "/tmp/toto.txt", "Toto", "/tmp/toto.txt", source_id, &entry_id_1),
               "1st addentry failed");
   int entry_id_2=-1;
   fail_unless(catalog_addentry(catalog, "/tmp/toto.txt", "Toto", "/tmp/toto.txt", source_id, &entry_id_2),
               "2nd addentry failed");

   catalog_disconnect(catalog);
   printf("--- test_addentry_noduplicate OK\n");
}
END_TEST

/**
 * Add the result name into a GArray
 * @param catalog ignored
 * @param pertinence ignored
 * @param name
 * @param long_name
 * @param path
 * @param userdata an array of strings (passed by catalog_executequery as userdata)
 * @see #results
 */
static gboolean collect_result_names_callback(struct catalog *catalog,
                                              float pertinence,
                                              int entry_id,
                                              const char *name,
                                              const char *long_name,
                                              const char *path,
                                              int source_id,
                                              const char *source_type,
                                              void *userdata)
{
   GArray **results = (GArray **)userdata;
   g_return_val_if_fail(results, FALSE);

   fail_unless(name!=NULL, "missing name");
   fail_unless(long_name!=NULL, "missing long name");
   fail_unless(path!=NULL, "missing long path/uri");

   mark_point();
   char *name_dup = g_strdup(name);
   mark_point();
   g_array_append_val(*results,
                      name_dup);
   mark_point();
   printf("result[%u]: %s\n", (*results)->len-1, name_dup);
   mark_point();
   return TRUE;/*continue*/
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
static void execute_query_and_expect(const char *query, int goal_length, char *goal[], gboolean ordered)
{
   GArray *array = g_array_new(TRUE/*zero_terminated*/, TRUE/*clear*/, sizeof(char *));
   catalog_cmd(catalog,
               "executequery()",
               catalog_executequery(catalog, query, collect_result_names_callback, &array));
   mark_point();
   int found_length = array->len;
   mark_point();
   char **found = (char **)array->data;
   mark_point();
   GString* msg = g_string_new("");
   g_string_append(msg, "query: '");
   g_string_append(msg, query);
   g_string_append(msg, "' found: [");
   mark_point();
   for(int i=0; i<found_length; i++)
      {
         g_string_append_c(msg, ' ');
         g_string_append(msg, found[i]);
      }
   mark_point();
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
               gboolean ok=FALSE;
               for(int j=0; j<found_length; j++)
                  {
                     if(found[j]!=NULL && strcmp(goal[i], found[j])==0)
                        {
                           found[j]=NULL;
                           ok=TRUE;
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
   mark_point();
   for(int i=0; i<found_length; i++)
      g_free(found[i]);
   g_free(array);
}


START_TEST(test_execute_query)
{
   printf("--- test_execute_query\n");
   execute_query_and_expect("toto.c",
                            1,
                            (char *[]){ "toto.c" },
                            FALSE/*not ordered*/);
}
END_TEST

static gboolean test_source_callback(struct catalog *catalog,
                                     float pertinence,
                                     int entry_id,
                                     const char *name,
                                     const char *long_name,
                                     const char *path,
                                     int result_source_id,
                                     const char *source_type,
                                     void *userdata)
{
   gboolean *checked = (gboolean *)userdata;
   fail_unless(source_id==result_source_id,
               "wrong source id");
   fail_unless(strcmp("test", source_type)==0,
               "wrong source type");
   *checked=TRUE;
}

START_TEST(test_execute_query_test_source)
{
   printf("--- test_execute_query_test_source\n");
   gboolean checked = FALSE;
   catalog_cmd(catalog,
               "executequery()",
               catalog_executequery(catalog, "t", test_source_callback, &checked));
   fail_unless(checked, "no source checked (query did not match anything)");
   printf("--- test_execute_query_test_source OK\n");
}
END_TEST

static gboolean contains_result(GList *list, const char *name)
{
   gboolean executed=FALSE;
   for(GList *current = list; current; current=g_list_next(current))
      {
         char *result_name=(char *)current->data;
         if(strcmp(name, result_name)==0)
            {
               executed=TRUE;
               printf("execute: %s\n");
            }
      }
   return executed;
}


START_TEST(test_lastexecuted_first)
{
   printf("--- test_lastexecuted_first\n");
   catalog_update_entry_timestamp(catalog, entries[2].id/*total.h*/);
   catalog_update_entry_timestamp(catalog, entries[1].id/*toto.h*/);
   execute_query_and_expect("tot",
                            3,
                            (char *[]){ "toto.h", "total.h", "toto.c" },
                            TRUE/*ordered*/);
}
END_TEST

START_TEST(test_execute_query_with_space)
{
   printf("--- test_execute_query\n");
   execute_query_and_expect("to .c",
                            1,
                            (char *[]){ "toto.c" },
                            FALSE/*not ordered*/);
}
END_TEST

/**
 * Let the query return only that many result and return FALSE.
 *
 * @param catalog ignored
 * @param pertinence ignored
 * @param result released
 * @param userdata a pointer to an integer (the counter), which will be decremented to 0
 */
static gboolean countdown_callback(struct catalog *catalog,
                                   float pertinence,
                                   int entry_id,
                                   const char *name,
                                   const char *long_name,
                                   const char *path,
                                   int source_id,
                                   const char *source_type,
                                   void *userdata)
{
   int *counter = (int *)userdata;
   g_return_val_if_fail(counter!=NULL, FALSE);
   fail_unless( (*counter)>0, "callback called once too many");

   (*counter)--;
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
 * @param name
 * @param long_name
 * @param path
 * @param userdata a pointer to an integer (the counter), which will be decremented to 0
 */
static gboolean countdown_interrupt_callback(struct catalog *catalog,
                                             float pertinence,
                                             int entry_id,
                                             const char *name,
                                             const char *long_name,
                                             const char *path,
                                             int source_id,
                                             const char *source_type,
                                             void *userdata)
{
   int *counter = (int *)userdata;
   g_return_val_if_fail(counter!=NULL, FALSE);
   fail_unless( (*counter)>0, "countdown_interrupt_callback called once too many");

   (*counter)--;

   if( (*counter) == 0 )
      catalog_interrupt(catalog);
   return TRUE/*continue*/;
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

   GArray *array = g_array_new(TRUE, TRUE, sizeof(char *));
   catalog_cmd(catalog,
               "executequery(t)",
               catalog_executequery(catalog, "t", collect_result_names_callback, &array));
   fail_unless(array->len==0, "a result has been added after the catalog has been interrupted");

   catalog_restart(catalog);
   catalog_cmd(catalog,
               "executequery(t)",
               catalog_executequery(catalog, "t", collect_result_names_callback, &array));
   fail_unless(array->len>0, "catalog has not recovered from an interruption");
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
   gboolean *stop = (gboolean *)userdata;
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
               "BEGIN; INSERT INTO entries VALUES (NULL, '', '', '', '', '');",
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
                                     FALSE/*not joinable*/,
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
                            (char *[]) { NULL }, FALSE/*not ordered*/);
   printf("test_busy: query done\n");
   catalog_restart(catalog);

   g_mutex_lock(execute_query_mutex);

   g_cond_broadcast(execute_query_cond); /* tell the thread to rollback in a few seconds*/
   g_mutex_unlock(execute_query_mutex);


   execute_query_and_expect("toto.c",
                            1,
                            (char *[]) { "toto.c" }, FALSE/*not ordered*/);

   g_mutex_lock(execute_query_mutex);
   g_cond_broadcast(execute_query_cond); /* tell the thread to finish*/
   g_mutex_unlock(execute_query_mutex);
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
   tcase_add_test(tc_core, test_add_source);
   tcase_add_test(tc_core, test_add_source_escape);
   tcase_add_test(tc_core, test_list_sources);
   tcase_add_test(tc_core, test_source_attributes);
   tcase_add_test(tc_core, test_addentry);
   tcase_add_test(tc_core, test_addentry_noduplicate);
   tcase_add_test(tc_core, test_addentry_escape);

   tcase_add_checked_fixture(tc_query, setup_query, teardown_query);
   suite_add_tcase(s, tc_query);
   tcase_add_test(tc_query, test_execute_query);
   tcase_add_test(tc_query, test_execute_query_with_space);
   tcase_add_test(tc_query, test_execute_query_test_source);
   tcase_add_test(tc_query, test_callback_stops_query);
   tcase_add_test(tc_query, test_interrupt_stops_query);
   tcase_add_test(tc_query, test_recover_from_interruption);
   tcase_add_test(tc_query, test_busy);
   tcase_add_test(tc_query, test_lastexecuted_first);

   return s;
}

int main(void)
{
   int nf;
   Suite *s = catalog_check_suite ();
   SRunner *sr = srunner_create (s);
   srunner_set_log(sr, "catalog_check.log");
   srunner_run_all (sr, CK_NORMAL);
   nf = srunner_ntests_failed (sr);
   srunner_free (sr);
   suite_free (s);
   return (nf == 0) ? 0:10;
}

