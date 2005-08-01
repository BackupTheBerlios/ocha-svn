#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include <unistd.h>
#include <string.h>
#include "catalog_queryrunner.h"
#include "catalog.h"
#include "mock_launchers.h"

#define CATALOG_PATH ".catalog.test"

struct location
{
        const char *file;
        int line;
};

/**
 * 2 entries matches "hell",
 * 1 entry matches "hello",
 * 0 entries matches "hellow",
 * 10 entries matches "he",
 * >4 entries matches "h", "he", "hel"
 * >4 entries matches "e"
 */
static char *entries[] = {
                                 "hell, no",      /* "h", "he", "hell", "e" */
                                 "hellovathing",  /* "h", "he", "hell", "hello", "e" */
                                 "hehel",         /* "h", "he", "hel", "e" */
                                 "bahel",         /* "h", "he", "hel", "e" */
                                 "lahelt",        /* "h", "he", "hel","e" */
                                 "bahareth",      /* "h", "e" */
                                 "ta he ho",      /* "h", "he", "e" */
                                 "barhat",        /* "h" */
                                 "xoloth",        /* "h" */
                                 "balaxilth",     /* "h" */
                                 "bobo, xag ?",
                                 "behere",        /* "h", "he", "e" */
                                 "heboxit",       /* "h", "he", "e" */
                                 "cahet",         /* "h", "he", "e" */
                                 "herat",         /* "h", "he", "e" */
                         };
#define entries_len (sizeof(entries)/sizeof(char *))
#define run(query) _run(query, __FILE__, __LINE__)
#define get_results_counted(query_id, count) _get_results_counted(query_id, count, __FILE__, __LINE__)
#define assert_no_more_results() _assert_no_more_results(__FILE__, __LINE__)

static struct queryrunner *runner;
static struct result_queue *queue;
static int expected_result_count;
static QueryId expected_query_id;
static int current_result_count;
static GMainContext *maincontext;
static struct location caller_location;
/**
 * SList of struct result_queue_element.
 * The elements on this list are copies and should be freed with
 * result_queue_element_copy_free()
 *
 * Elements are added to this list by collect_result_handler()
 * if collected_enabled is TRUE
 */
static GSList *collected;
gboolean collected_enabled;

/* ------------------------- prototypes */
static Suite *catalog_queryrunner_check_suite(void);
static QueryId _run(const char *query, const char *file, int line);
static gboolean timeout_callback(gpointer userdata);
static void _get_results_counted(QueryId query_id, int count, const char *file, int line);
static void _assert_no_more_results(const char *file, int line);
static void result_handler(struct result_queue_element *element, gpointer userdata);
static void counter_result_handler(struct result_queue_element *element, gpointer userdata);
static void collect_result_handler(struct result_queue_element *element, gpointer userdata);
static struct result_queue_element *result_queue_element_copy(struct result_queue_element *element);
static void result_queue_element_copy_free(struct result_queue_element *element);
static void assert_collected_query_id_is(QueryId id1);

/* ------------------------- test case */

static void setup()
{
        int source_id;
        struct catalog *catalog;
        struct catalog_entry entry;
        int i;

        unlink(CATALOG_PATH);
        g_thread_init(NULL/*vtable*/);


        catalog =  catalog_new_and_connect(CATALOG_PATH, NULL/*errs*/);
        fail_unless(catalog!=NULL, "no catalog in " CATALOG_PATH);
        fail_unless(catalog_add_source(catalog, "test", &source_id), "add_source");

        CATALOG_ENTRY_INIT(&entry);
        entry.source_id=source_id;
        entry.launcher=TEST_LAUNCHER;
        for(i=0; i<entries_len; i++) {
                entry.name=entries[i];
                entry.path=entries[i];
                entry.long_name=entries[i];
                fail_unless(catalog_add_entry(catalog,
                                              &entry,
                                              NULL/*id_out*/),
                            "add source");
        }
        catalog_free(catalog);

        maincontext=g_main_context_default();

        queue=result_queue_new(NULL/*default context*/,
                               result_handler,
                               &caller_location/*userdata*/);
        runner=catalog_queryrunner_new(CATALOG_PATH, queue);
        runner->start(runner);
}

static void teardown()
{
        printf("--teardown START\n");
        g_main_context_unref(maincontext);

        if(runner)
                runner->release(runner);

        /*     if(queue) */
        /*         result_queue_delete(queue); */

        unlink(CATALOG_PATH);
        printf("--teardown OK\n");
}

START_TEST(test_fast_typer)
{
        printf("--test_fast_typer START\n");
        get_results_counted(run("h"), 4);
        get_results_counted(run("he"), 4);
        get_results_counted(run("hel"), 4);
        get_results_counted(run("hell"), 2);
        get_results_counted(run("hello"), 1);
        run("hellow");
        assert_no_more_results();
        printf("--test_fast_typer OK\n");
}
END_TEST

START_TEST(test_slow_thinker)
{
        printf("--test_slow_thinker START\n");
        get_results_counted(run("h"), 4);
        get_results_counted(run("he"), 10);
        get_results_counted(run("hell"), 2);
        assert_no_more_results();
        printf("--test_slow_thinker OK\n");
}
END_TEST


START_TEST(test_back_to_nothing)
{
        printf("--test_back_to_nothing START\n");
        get_results_counted(run("hello"), 1);
        get_results_counted(run("hell"), 2);
        get_results_counted(run("hel"), 4);
        get_results_counted(run("he"), 4);
        get_results_counted(run("h"), 4);
        run("");
        assert_no_more_results();
        printf("--test_back_to_nothing OK\n");
}
END_TEST
START_TEST(test_to_nothing_and_back_again)
{
        printf("--test_to_nothing_and_back_again START\n");
        get_results_counted(run("h"), 4);
        run("");
        assert_no_more_results();
        get_results_counted(run("e"), 4);
        run("");
        assert_no_more_results();
        printf("--test_to_nothing_and_back_again OK\n");
}
END_TEST


START_TEST(test_start_stop)
{
        printf("--test_start_stop START\n");
        get_results_counted(run("h"), 4);
        get_results_counted(run("he"), 4);
        get_results_counted(run("hel"), 4);
        runner->stop(runner);
        assert_no_more_results();
        runner->start(runner);
        run("");
        get_results_counted(run("e"), 4);
        runner->stop(runner);
        assert_no_more_results();
        runner->start(runner);
        get_results_counted(run("he"), 4);
        run("hellow");
        assert_no_more_results();
        printf("--test_start_stop OK\n");
}
END_TEST


START_TEST(test_query_id)
{
        QueryId id1;
        QueryId id2;

        printf("--test_query_id START\n");

        collected_enabled=TRUE;

        id1=run("hell");
        get_results_counted(id1, 2);
        fail_unless(g_slist_length(collected)==2, "collected length");

        assert_collected_query_id_is(id1);

        id2=run("hel");
        fail_unless(id1!=id2, "same ID twice: 0x%lx", id1);
        get_results_counted(id2, 5);
        fail_unless(g_slist_length(collected)==5, "collected length");
        assert_collected_query_id_is(id2);

        printf("--test_query_id OK\n");
}
END_TEST

/* ------------------------- main */

static Suite *catalog_queryrunner_check_suite(void)
{
        Suite *s = suite_create("catalog_queryrunner");
        TCase *tc_core = tcase_create("catalog_queryrunner_core");

        suite_add_tcase(s, tc_core);
        tcase_set_timeout(tc_core, 60/*s.*/);
        tcase_add_checked_fixture(tc_core, setup, teardown);
        tcase_add_test(tc_core, test_fast_typer);
        tcase_add_test(tc_core, test_slow_thinker);
        tcase_add_test(tc_core, test_back_to_nothing);
        tcase_add_test(tc_core, test_to_nothing_and_back_again);
        tcase_add_test(tc_core, test_start_stop);
        tcase_add_test(tc_core, test_query_id);

        return s;
}

int main(void)
{
        int nf;
        Suite *s = catalog_queryrunner_check_suite ();
        SRunner *sr = srunner_create (s);
        srunner_run_all (sr, CK_NORMAL);
        nf = srunner_ntests_failed (sr);
        srunner_free (sr);
        return (nf == 0) ? 0:10;
}

/* ------------------------- static functions */
static QueryId _run(const char *query, const char *file, int line)
{
        QueryId id;
        _mark_point(file, line);

        id=runner->run_query(runner, query);

        printf("%s:%d: running query '%s' 0x%lx\n", /*@nocommit@*/
               __FILE__,
               __LINE__,
               query,
               id
               );

        return id;
}
static gboolean timeout_callback(gpointer userdata)
{
        gboolean *flag = (gboolean *)userdata;
        g_return_val_if_fail(flag, FALSE);
        g_return_val_if_fail(*flag==FALSE, FALSE);
        *flag=TRUE;
        return FALSE;
}

static void _get_results_counted(QueryId query_id, int count, const char *file, int line)
{
        gboolean timed_out = FALSE;
        guint timeout_1;

        _mark_point(file, line);
        caller_location.file=file;
        caller_location.line=line;
        expected_result_count=count;
        current_result_count=0;
        expected_query_id=query_id;

        printf("%s:%d: expecting %d results for query 0x%lx...\n",
               file,
               line,
               count,
               query_id);

        timeout_1 = g_timeout_add(10000, timeout_callback, &timed_out);

        while(!timed_out && current_result_count<expected_result_count) {
                g_main_context_iteration(maincontext, TRUE/*may block*/);
                printf("%s:%d: result count %d/%d timeout:%s\n",
                       file,
                       line,
                       current_result_count,
                       expected_result_count,
                       timed_out ? "true":"false");
        }
        if(timed_out) {
                _fail_unless(0, file, line, "failure", "timed out while waiting for query results", NULL);
        }
        g_source_remove(timeout_1);
        if(current_result_count==expected_result_count) {
                gboolean timed_out_2 = FALSE;
                g_timeout_add(300, timeout_callback, &timed_out_2);
                while(!timed_out_2) {
                        g_main_context_iteration(maincontext, TRUE/*may block*/);
                        _fail_unless(current_result_count==expected_result_count,
                                     file,
                                     line,
                                     "current_result_count==expected_result_count",
                                     "too many results",
                                     NULL);
                }
        }

        printf("%s:%d: expectation met\n",
               file,
               line);
}

static void _assert_no_more_results(const char *file, int line)
{
        gboolean timed_out = FALSE;
        _mark_point(file, line);
        printf("%s:%d: waiting 3s to make sure there are no more results\n",
               file,
               line);
        caller_location.file=file;
        caller_location.line=line;

        expected_result_count=0;
        current_result_count=0;
        expected_query_id=0;

        g_timeout_add(3000, timeout_callback, &timed_out);
        while(!timed_out)
                g_main_context_iteration(maincontext, TRUE/*may block*/);
}


static void result_handler(struct result_queue_element *element,
                           gpointer userdata)
{
        counter_result_handler(element, userdata);
        collect_result_handler(element, userdata);
}

static void counter_result_handler(struct result_queue_element *element,
                                   gpointer userdata)
{
        struct location *location = (struct location *)userdata;
        struct result *result = element->result;

        printf("got result for query 0x%lx: %s\n",
               element->query_id,
               result->name);

        _fail_unless(element->caller==runner,
                     location->file,
                     location->line,
                     "caller==runner",
                     "wrong caller passed to callback",
                     NULL);
        _fail_unless(element->query_id==expected_query_id,
                     location->file,
                     location->line,
                     "query_id!=expected_query",
                     "expected %d results for query 0x%lx, but got a result for query 0x%lx",
                     expected_result_count-current_result_count,
                     expected_query_id,
                     element->query_id,
                     NULL);
        _fail_unless(current_result_count<expected_result_count,
                     location->file,
                     location->line,
                     "current_result_count<expected_result_count",
                     "expected %d results for query '%s', got %d so far",
                     expected_result_count,
                     element->query,
                     current_result_count+1,
                     NULL);
        current_result_count++;
}

static void collect_result_handler(struct result_queue_element *element,
                                   gpointer userdata)
{
        struct result_queue_element *copy;

        if(collected_enabled) {
                copy=result_queue_element_copy(element);
                collected=g_slist_append(collected, copy);
        }
}

static struct result_queue_element *result_queue_element_copy(struct result_queue_element *element)
{
        struct result_queue_element *copy;
        g_return_val_if_fail(element, NULL);

        copy = g_new(struct result_queue_element, 1);
        memcpy(copy, element, sizeof(struct result_queue_element));
        copy->query=g_strdup(element->query);
        return copy;
}

static void result_queue_element_copy_free(struct result_queue_element *element)
{
        g_return_if_fail(element);

        g_free(element->query);
        g_free(element);
}

static void assert_collected_query_id_is(QueryId id1)
{
        GSList *item;
        int i=0;

        for(item=collected; item!=NULL; item=g_slist_next(item), i++) {
                struct result_queue_element *element;

                element = (struct result_queue_element *)item->data;
                fail_unless(id1==element->query_id, "wrong query_id for result %d", i);
                result_queue_element_copy_free(element);
        }
        g_slist_free(collected);
        collected=NULL;
}

