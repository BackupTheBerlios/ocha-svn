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
#define TEST_LAUNCHER "test"

static struct catalog *catalog;
static int source_id;
static GMutex *execute_query_mutex;
static GCond  *execute_query_cond;

#define CATALOG_ENTRY(path, filename) { filename, path, path, TEST_LAUNCHER, 0, 0 }
struct catalog_entry entries[] = {
        CATALOG_ENTRY("/tmp/toto.c", "toto.c"),
        CATALOG_ENTRY("/tmp/toto.h", "toto.h"),
        CATALOG_ENTRY("/tmp/total.h", "total.h"),
        CATALOG_ENTRY("/tmp/etalma.c", "etalma.c"),
        CATALOG_ENTRY("/tmp/talm.c", "talm.c"),
        CATALOG_ENTRY("/tmp/talm.h", "talm.h"),
        CATALOG_ENTRY("/tmp/hello.txt", "hello.txt"),
        CATALOG_ENTRY("/tmp/hullo.txt", "hullo.txt")
};
#define entries_length (sizeof(entries)/sizeof(struct catalog_entry))

#define catalog_cmd(catalog, comment, ret) _catalog_cmd(catalog, comment, ret, __FILE__, __LINE__)

/* ------------------------- prototypes */
static Suite *catalog_check_suite(void);
static void assert_array_contains(const char *query, int goal_length, char *goal[], GArray *array, gboolean ordered);
static gboolean collect_result_names_callback(struct catalog *catalog, float pertinence, const struct catalog_entry *entry, void *userdata);
static void _catalog_cmd(struct catalog *catalog, const char *comment, gboolean ret, const char *file, int line);
static gboolean exists(const char *path);
static void unlink_if_exists(const char *path);
static void execute_query_and_expect(const char *query, int goal_length, char *goal[], gboolean ordered);
static void assert_array_contains(const char *query, int goal_length, char *goal[], GArray *array, gboolean ordered);
static gboolean test_source_callback(struct catalog *catalog, float pertinence, const struct catalog_entry *entry, void *userdata);
static gboolean countdown_callback(struct catalog *catalog, float pertinence, const struct catalog_entry *entry, void *userdata);
static gboolean countdown_interrupt_callback(struct catalog *catalog, float pertinence, const struct catalog_entry *entry, void *userdata);
static gpointer execute_query_thread(void *userdata);
static void addentries(struct catalog *catalog, int sourceid, int count, const char *name_pattern);

/* ------------------------- test suite: catalog */

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
        int i;
        setup();
        catalog=catalog_connect(PATH, NULL);

        fail_unless(catalog_add_source(catalog,
                                       "test",
                                       &source_id),
                    "source could not be created");
        for(i=0; i<entries_length; i++) {
                entries[i].source_id=source_id;
                fail_unless(catalog_add_entry_struct(catalog,
                                                     &entries[i],
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
        GError *err = NULL;
        struct catalog *catalog;

        printf("--- test_create\n");

        catalog = catalog_connect(PATH, &err);
        if(!catalog) {
                fail_unless(err!=NULL, "no catalog and no error message");
                fail(err->message);
        }
        fail_unless(exists(PATH), "catalog file not created in " PATH);
        catalog_disconnect(catalog);
}
END_TEST


START_TEST(test_addentry)
{
        struct catalog *catalog=catalog_connect(PATH, NULL);
        struct catalog_entry entry = CATALOG_ENTRY("toto", "/tmp/toto.txt");

        printf("--- test_addentry\n");
        catalog = catalog_connect(PATH, NULL);

        catalog_add_source(catalog, "test", &entry.source_id);
        catalog_cmd(catalog,
                    "1st addentry(/tmp/toto.txt)",
                    catalog_add_entry_struct(catalog,
                                      &entry,
                                      &entry.id));
        catalog_disconnect(catalog);
}
END_TEST

START_TEST(test_addentry_escape)
{
        struct catalog *catalog;
        struct catalog_entry entry = CATALOG_ENTRY("To'to", "/tmp/to'to.txt");
        printf("--- test_addentry_escape\n");

        catalog = catalog_connect(PATH, NULL);

        catalog_add_source(catalog, "test", &entry.source_id);
        catalog_cmd(catalog,
                    "1st addentry(/tmp/toto.txt)",
                    catalog_add_entry_struct(catalog,
                                      &entry,
                                      NULL/*id_out*/));

        catalog_disconnect(catalog);
}
END_TEST

START_TEST(test_addentry_noduplicate)
{
        struct catalog *catalog;
        struct catalog_entry entry = CATALOG_ENTRY("toto", "/tmp/toto.txt");
        int entry_id_1=-1;
        int entry_id_2=-1;

        printf("--- test_addentry_noduplicate\n");

        catalog = catalog_connect(PATH, NULL);

        catalog_add_source(catalog, "test", &entry.source_id);

        fail_unless(catalog_add_entry_struct(catalog,
                                      &entry,
                                      &entry_id_1),
                    "1st addentry failed");
        fail_unless(catalog_add_entry_struct(catalog,
                                      &entry,
                                      &entry_id_2),
                    "2nd addentry failed");

        fail_unless(entry_id_1==entry_id_2,
                    "two entries with the same path and source_id but with different IDs");
        catalog_disconnect(catalog);
        printf("--- test_addentry_noduplicate OK\n");
}
END_TEST

START_TEST(test_get_source_content_size)
{
        struct catalog *catalog;
        int source1_id = -1;
        int source2_id = -1;
        unsigned int source1_count = -1;
        unsigned int source2_count = -1;

        printf("--- test_get_source_content_size\n");
        catalog = catalog_connect(PATH, NULL);
        catalog_cmd(catalog,
                    "add_source(edit)",
                    catalog_add_source(catalog,
                                       "test",
                                       &source1_id));

        catalog_cmd(catalog,
                    "add_source(edit)",
                    catalog_add_source(catalog,
                                       "test",
                                       &source2_id));
        addentries(catalog, source1_id, 10, "source1-entry-%d");
        addentries(catalog, source2_id, 3, "source2-entry-%d");

        catalog_cmd(catalog,
                    "count source1",
                    catalog_get_source_content_count(catalog,
                                                     source1_id,
                                                     &source1_count));
        catalog_cmd(catalog,
                    "count source2",
                    catalog_get_source_content_count(catalog,
                                                     source2_id,
                                                     &source2_count));
        printf("source1: %d, source2: %d\n",
               source1_count, source2_count);
        fail_unless(10==source1_count,
                    g_strdup_printf("wrong count for source 1, expected 10, got %d",
                                    source1_count));
        fail_unless(3==source2_count, "wrong count for source 2");

        catalog_disconnect(catalog);

        printf("--- test_get_source_content_size_OK\n");
}
END_TEST

START_TEST(test_get_source_content)
{
        struct catalog *catalog;
        int source1_id = -1;
        int source2_id = -1;
        GArray *array;
        static char *source1_entries[] = {
                "source1-entry-0",
                "source1-entry-1",
                "source1-entry-2",
                "source1-entry-3",
                "source1-entry-4" };

        printf("--- test_get_source_content\n");
        catalog = catalog_connect(PATH, NULL);

        catalog_cmd(catalog,
                    "add_source(edit)",
                    catalog_add_source(catalog, "test", &source1_id));
        catalog_cmd(catalog,
                    "add_source(edit)",
                    catalog_add_source(catalog, "test", &source2_id));

        addentries(catalog, source1_id, 5, "source1-entry-%d");
        addentries(catalog, source2_id, 3, "source1-entry-%d");

        mark_point();
        array =  g_array_new(TRUE/*zero_terminated*/,
                             TRUE/*clear*/,
                             sizeof(char *));
        mark_point();
        catalog_cmd(catalog,
                    "get_source_content(source1)",
                    catalog_get_source_content(catalog,
                                               source1_id,
                                               collect_result_names_callback,
                                               &array));
        mark_point();
        assert_array_contains("get content of source 1",
                              5,
                              source1_entries,
                              array,
                              FALSE/*not ordered*/);

        catalog_disconnect(catalog);

        printf("--- test_get_source_content_size_OK\n");
}
END_TEST

START_TEST(test_remove_entry)
{
        struct catalog *catalog;
        int source_id = -1;
        guint count=-1;
        printf("--- test_remove_entry\n");

        catalog = catalog_connect(PATH, NULL);

        catalog_cmd(catalog,
                    "add_source(edit)",
                    catalog_add_source(catalog, "test", &source_id));
        addentries(catalog, source_id, 5, "entry-%d");

        mark_point();
        catalog_cmd(catalog,
                    "remove_entry",
                    catalog_remove_entry(catalog,
                                         source_id,
                                         "entry-1"));
        mark_point();


        catalog_cmd(catalog,
                    "get count",
                    catalog_get_source_content_count(catalog,
                                                     source_id,
                                                     &count));
        fail_unless(count==4, "entry not deleted");

        printf("--- test_remove_entry OK\n");
}
END_TEST

START_TEST(test_remove_source)
{
        struct catalog *catalog;
        int source1_id = -1;
        guint source1_count=-1;

        printf("--- test_remove_source\n");

        catalog = catalog_connect(PATH, NULL);

        catalog_cmd(catalog,
                    "add_source(edit)",
                    catalog_add_source(catalog, "test", &source1_id));
        addentries(catalog, source1_id, 5, "source1-entry-%d");

        mark_point();
        catalog_cmd(catalog,
                    "remove_source",
                    catalog_remove_source(catalog, source1_id));
        mark_point();


        catalog_cmd(catalog,
                    "get count",
                    catalog_get_source_content_count(catalog,
                                                     source1_id, &
                                                     source1_count));
        fail_unless(source1_count==0, "content not deleted");

        printf("--- test_remove_source OK\n");
}
END_TEST

START_TEST(test_source_update)
{
        struct catalog *catalog;
        int source1_id;
        unsigned int source1_count;
        struct catalog_entry entry = CATALOG_ENTRY("toto", "/tmp/toto.txt");

        printf("--- test_source_update\n");

        catalog = catalog_connect(PATH, NULL);

        catalog_cmd(catalog,
                    "add_source(edit)",
                    catalog_add_source(catalog, "test", &source1_id));

        addentries(catalog, source1_id, 10, "source1-entry-%d");

        catalog_begin_source_update(catalog, source1_id);
        addentries(catalog, source1_id, 3, "source1-entry-%d");

        entry.source_id=source1_id;
        catalog_cmd(catalog,
                    "custom addentry",
                    catalog_add_entry_struct(catalog,
                                             &entry,
                                             &entry.id));
        catalog_end_source_update(catalog, source1_id);

        catalog_cmd(catalog,
                    "get count",
                    catalog_get_source_content_count(catalog,
                                                     source1_id,
                                                     &source1_count));

        fail_unless(source1_count==4,
                    "expected non-refreshed entries to have been deleted");


        printf("--- test_source_update OK\n");
}
END_TEST



START_TEST(test_execute_query)
{
        static char *array[] = { "toto.c" };

        printf("--- test_execute_query\n");
        execute_query_and_expect("toto.c",
                                 1,
                                 array,
                                 FALSE/*not ordered*/);
}
END_TEST

START_TEST(test_execute_query_test_source)
{
        gboolean checked = FALSE;
        printf("--- test_execute_query_test_source\n");
        catalog_cmd(catalog,
                    "executequery()",
                    catalog_executequery(catalog,
                                         "t",
                                         test_source_callback,
                                         &checked));
        fail_unless(checked, "no source checked (query did not match anything)");
        printf("--- test_execute_query_test_source OK\n");
}
END_TEST



START_TEST(test_lastexecuted_first)
{
        static char *goal[] = { "toto.h", "total.h", "toto.c" };
        printf("--- test_lastexecuted_first\n");
        catalog_update_entry_timestamp(catalog, entries[2].id/*total.h*/);
        catalog_update_entry_timestamp(catalog, entries[1].id/*toto.h*/);
        execute_query_and_expect("tot",
                                 3,
                                 goal,
                                 TRUE/*ordered*/);
}
END_TEST

START_TEST(test_execute_query_with_space)
{
        static char *goal[] = { "toto.c" };
        printf("--- test_execute_query\n");
        execute_query_and_expect("to .c",
                                 1,
                                 goal,
                                 FALSE/*not ordered*/);
}
END_TEST

START_TEST(test_callback_stops_query)
{
        int count=1;
        printf("--- test_callback_stops_query\n");

        catalog_cmd(catalog,
                    "executequery(t)",
                    catalog_executequery(catalog,
                                         "t",
                                         countdown_callback,
                                         &count));
        fail_unless(count==0, "wrong count");
}
END_TEST

START_TEST(test_interrupt_stops_query)
{
        int count=1;
        printf("--- test_interrupt_stops_query\n");

        catalog_cmd(catalog,
                    "executequery(t)",
                    catalog_executequery(catalog,
                                         "t",
                                         countdown_interrupt_callback,
                                         &count));
        fail_unless(count==0, "wrong count");
}
END_TEST

START_TEST(test_recover_from_interruption)
{
        int count=1;
        GArray *array;
        printf("--- test_recover_from_interruption\n");

        catalog_cmd(catalog,
                    "executequery(t)",
                    catalog_executequery(catalog,
                                         "t",
                                         countdown_interrupt_callback,
                                         &count));
        fail_unless(count==0, "wrong count");

        array =  g_array_new(TRUE, TRUE, sizeof(char *));
        catalog_cmd(catalog,
                    "executequery(t)",
                    catalog_executequery(catalog,
                                         "t",
                                         collect_result_names_callback,
                                         &array));
        fail_unless(array->len==0,
                    "a result has been added after the catalog has been interrupted");

        catalog_restart(catalog);
        catalog_cmd(catalog,
                    "executequery(t)",
                    catalog_executequery(catalog,
                                         "t",
                                         collect_result_names_callback,
                                         &array));
        fail_unless(array->len>0,
                    "catalog has not recovered from an interruption");
}
END_TEST

START_TEST(test_busy)
{
        GThread *thread;
        static char *goal[] = { "toto.c" };

        printf("--- test_busy\n");
        execute_query_mutex = g_mutex_new();
        execute_query_cond = g_cond_new();

        g_mutex_lock(execute_query_mutex);
        thread =  g_thread_create(execute_query_thread,
                                  NULL/*userdata*/,
                                  FALSE/*not joinable*/,
                                  NULL);
        fail_unless(thread!=NULL,
                    "thread creation failed");

        printf("test_busy: broadcast (interrupt)\n");

        g_cond_wait(execute_query_cond, execute_query_mutex);

/* tell the thread to lock and interrupt us in a few seconds*/
        g_cond_broadcast(execute_query_cond);
        g_mutex_unlock(execute_query_mutex);

        printf("test_busy: execute query\n");
        execute_query_and_expect("toto.c",
                                 0,
                                 NULL,
                                 FALSE/*not ordered*/);
        printf("test_busy: query done\n");
        catalog_restart(catalog);

        g_mutex_lock(execute_query_mutex);

/* tell the thread to rollback in a few seconds*/
        g_cond_broadcast(execute_query_cond);
        g_mutex_unlock(execute_query_mutex);


        execute_query_and_expect("toto.c",
                                 1,
                                 goal,
                                 FALSE/*not ordered*/);

        g_mutex_lock(execute_query_mutex);
        g_cond_broadcast(execute_query_cond); /* tell the thread to finish*/
        g_mutex_unlock(execute_query_mutex);
}
END_TEST

/* ------------------------- main */

static Suite *catalog_check_suite(void)
{
        Suite *s = suite_create("catalog");
        TCase *tc_core = tcase_create("catalog_core");
        TCase *tc_query = tcase_create("catalog_query");

        tcase_add_checked_fixture(tc_core, setup, teardown);
        suite_add_tcase(s, tc_core);
        tcase_add_test(tc_core, test_create);
        tcase_add_test(tc_core, test_addentry);
        tcase_add_test(tc_core, test_addentry_noduplicate);
        tcase_add_test(tc_core, test_addentry_escape);
        tcase_add_test(tc_core, test_get_source_content_size);
        tcase_add_test(tc_core, test_get_source_content);
        tcase_add_test(tc_core, test_remove_entry);
        tcase_add_test(tc_core, test_remove_source);
        tcase_add_test(tc_core, test_source_update);

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

/* ------------------------- static functions */

static void _catalog_cmd(struct catalog *catalog,
                         const char *comment,
                         gboolean ret,
                         const char *file,
                         int line)
{
        if(!ret) {
                GString *msg = g_string_new("");
                g_string_printf(msg,
                                "catalog function %s failed: %s\n",
                                comment,
                                catalog_error(catalog));
                _fail_unless(0, file, line, msg->str);
                g_string_free(msg, TRUE/*free content*/);
        }
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


/**
 * Add the result name into a GArray
 * @param catalog ignored
 * @param pertinence ignored
 * @param entry
 * @param userdata an array of strings (passed by catalog_executequery as userdata)
 * @see #results
 */
static gboolean collect_result_names_callback(struct catalog *catalog,
                                              float pertinence,
                                              const struct catalog_entry *entry,
                                              void *userdata)
{
        GArray **results;
        char *name_dup;

        printf("collect_result_names_callback\n");
        mark_point();
        results =  (GArray **)userdata;
        g_return_val_if_fail(results, FALSE);

        fail_unless(entry->name!=NULL, "missing name");
        fail_unless(entry->long_name!=NULL, "missing long name");
        fail_unless(entry->path!=NULL, "missing long path/uri");

        mark_point();
        name_dup =  g_strdup(entry->name);
        mark_point();
        g_array_append_val(*results,
                           name_dup);
        mark_point();
        printf("result[%u]: %s\n", (*results)->len-1, name_dup);
        mark_point();
        return TRUE;/*continue*/
}

static void execute_query_and_expect(const char *query,
                                     int goal_length,
                                     char *goal[],
                                     gboolean ordered)
{
        GArray *array = g_array_new(TRUE/*zero_terminated*/, TRUE/*clear*/, sizeof(char *));
        catalog_cmd(catalog,
                    "executequery()",
                    catalog_executequery(catalog, query, collect_result_names_callback, &array));
        assert_array_contains(query, goal_length, goal, array, ordered);
}
static void assert_array_contains(const char *query,
                                  int goal_length,
                                  char **goal,
                                  GArray *array,
                                  gboolean ordered)
{
        int i;
        int found_length = array->len;
        char **found = (char **)array->data;
        GString* msg = g_string_new("");

        mark_point();

        g_string_append(msg, "query: '");
        g_string_append(msg, query);
        g_string_append(msg, "' found: [");
        mark_point();
        for(i=0; i<found_length; i++) {
                g_string_append_c(msg, ' ');
                g_string_append(msg, found[i]);
        }
        mark_point();
        g_string_append(msg, " ] expected: [");
        for(i=0; i<goal_length; i++) {
                g_string_append_c(msg, ' ');
                g_string_append(msg, goal[i]);
        }
        g_string_append(msg, " ] - ");
        if(goal_length!=found_length) {
                g_string_append(msg, "wrong length");
                fail(msg->str);
        }
        if(ordered) {
                for(i=0; i<goal_length; i++) {
                        if(strcmp(goal[i], found[i])!=0) {
                                g_string_append_printf(msg,
                                                       "expected '%s' at index %d, got '%s",
                                                       goal[i],
                                                       i,
                                                       found[i]);
                                fail(msg->str);
                        }
                }
        } else /* not ordered */
        {
                for(i=0; i<goal_length; i++) {
                        gboolean ok=FALSE;
                        int j;
                        for(j=0; j<found_length; j++) {
                                if(found[j]!=NULL && strcmp(goal[i], found[j])==0) {
                                        found[j]=NULL;
                                        ok=TRUE;
                                        break;
                                }
                        }
                        if(!ok) {
                                g_string_append_printf(msg, "%s not found", goal[i]);
                                fail(msg->str);
                        }
                }
        }
        mark_point();
        for(i=0; i<found_length; i++)
                g_free(found[i]);
        g_free(array);
}

static gboolean test_source_callback(struct catalog *catalog,
                                     float pertinence,
                                     const struct catalog_entry *entry,
                                     void *userdata)
{
        gboolean *checked = (gboolean *)userdata;
        fail_unless(source_id==entry->source_id,
                    "wrong source id");
        fail_unless(strcmp(TEST_LAUNCHER, entry->launcher)==0,
                    "wrong launcher");
        *checked=TRUE;
        return TRUE/*continue*/;
}

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
                                   const struct catalog_entry *entry,
                                   void *userdata)
{
        int *counter = (int *)userdata;
        g_return_val_if_fail(counter!=NULL, FALSE);
        fail_unless( (*counter)>0, "callback called once too many");

        (*counter)--;
        return (*counter)>0;
}

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
                                             const struct catalog_entry *entry,
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

static gpointer execute_query_thread(void *userdata)
{
        char *errmsg=NULL;
        sqlite *db=sqlite_open(PATH, 0600, &errmsg);
        if(!db) {
                if(errmsg)
                        printf("sqlite error: %s\n", errmsg);
                fail("sqlite_open() failed");
        }

        g_mutex_lock(execute_query_mutex);
        printf("execute_query_thread: lock\n");
        sqlite_exec(db,
                    "BEGIN; INSERT INTO entries VALUES (NULL, '', '', '', '', '', '', 0);",
                    NULL/*no callback*/,
                    NULL/*userdata*/,
                    &errmsg);
        if(errmsg) {
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
        if(errmsg) {
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

static void addentries(struct catalog *catalog,
                       int sourceid,
                       int count,
                       const char *name_pattern)
{
        struct catalog_entry entry;
        int i;
        entry.source_id=sourceid;
        entry.launcher=TEST_LAUNCHER;
        for(i=0; i<count; i++)
        {
                char *name = g_strdup_printf(name_pattern, i);
                entry.name=name;
                entry.path=name;
                entry.long_name=name;
                printf("adding %s into source %d\n", name, sourceid);
                catalog_cmd(catalog,
                            name,
                            catalog_add_entry_struct(catalog,
                                              &entry,
                                              NULL/*id_out*/));
                g_free(name);
        }
}

