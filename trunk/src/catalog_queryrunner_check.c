#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include <unistd.h>
#include <string.h>
#include "catalog_queryrunner.h"
#include "catalog.h"

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
#define get_results_counted(query, count) _get_results_counted(query, count, __FILE__, __LINE__)
#define assert_no_more_results() _assert_no_more_results(__FILE__, __LINE__)

static struct queryrunner *runner;
static struct result_queue *queue;
static const char *expected_query = "<<no query>>";
static int expected_result_count;
static int current_result_count;
static GMainContext *maincontext;
static struct location caller_location;

/* ------------------------- prototypes */
static void _run(const char *query, const char *file, int line);
static gboolean timeout_callback(gpointer userdata);
static void _get_results_counted(const char *query, int count, const char *file, int line);
static void _assert_no_more_results(const char *file, int line);
static void result_handler(struct queryrunner *caller, const char *query, float pertinence, struct result * result, gpointer userdata);

/* ------------------------- test case */

static void setup()
{
        unlink(CATALOG_PATH);
        g_thread_init(NULL/*vtable*/);

        int source_id;
        struct catalog *catalog = catalog_connect(CATALOG_PATH, NULL/*errs*/);
        fail_unless(catalog!=NULL, "no catalog in " CATALOG_PATH);
        fail_unless(catalog_add_source(catalog, "test", &source_id), "add_source");
        for(int i=0; i<entries_len; i++) {
                fail_unless(catalog_add_entry(catalog,
                                              source_id,
                                              entries[i]/*path*/,
                                              entries[i]/*name*/,
                                              entries[i]/*long_name*/,
                                              NULL/*id_out*/),
                            "add source");
        }
        catalog_disconnect(catalog);

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
        run("h");
        get_results_counted("h", 4);
        run("he");
        get_results_counted("he", 4);
        run("hel");
        get_results_counted("hel", 4);
        run("hell");
        get_results_counted("hell", 2);
        run("hello");
        get_results_counted("hello", 1);
        run("hellow");
        assert_no_more_results();
        printf("--test_fast_typer OK\n");
}
END_TEST

START_TEST(test_slow_thinker)
{
        printf("--test_slow_thinker START\n");
        run("h");
        get_results_counted("h", 4);
        run("he");
        get_results_counted("he", 10);
        run("hell");
        get_results_counted("hell", 2);
        assert_no_more_results();
        printf("--test_slow_thinker OK\n");
}
END_TEST


START_TEST(test_back_to_nothing)
{
        printf("--test_back_to_nothing START\n");
        run("hello");
        get_results_counted("hello", 1);
        run("hell");
        get_results_counted("hell", 2);
        run("hel");
        get_results_counted("hel", 4);
        run("he");
        get_results_counted("he", 4);
        run("h");
        get_results_counted("h", 4);
        run("");
        assert_no_more_results();
        printf("--test_back_to_nothing OK\n");
}
END_TEST
START_TEST(test_to_nothing_and_back_again)
{
        printf("--test_to_nothing_and_back_again START\n");
        run("h");
        get_results_counted("h", 4);
        run("");
        assert_no_more_results();
        run("e");
        get_results_counted("e", 4);
        run("");
        assert_no_more_results();
        printf("--test_to_nothing_and_back_again OK\n");
}
END_TEST


START_TEST(test_start_stop)
{
        printf("--test_start_stop START\n");
        run("h");
        get_results_counted("h", 4);
        run("he");
        get_results_counted("he", 4);
        run("hel");
        get_results_counted("hel", 4);
        runner->stop(runner);
        assert_no_more_results();
        runner->start(runner);
        run("");
        run("e");
        get_results_counted("e", 4);
        runner->stop(runner);
        assert_no_more_results();
        runner->start(runner);
        run("he");
        get_results_counted("he", 4);
        run("hellow");
        assert_no_more_results();
        printf("--test_start_stop OK\n");
}
END_TEST

/* ------------------------- main */

Suite *catalog_queryrunner_check_suite(void)
{
        Suite *s = suite_create("catalog_queryrunner");
        TCase *tc_core = tcase_create("catalog_queryrunner_core");

        suite_add_tcase(s, tc_core);
        tcase_add_checked_fixture(tc_core, setup, teardown);
        tcase_add_test(tc_core, test_fast_typer);
        tcase_add_test(tc_core, test_slow_thinker);
        tcase_add_test(tc_core, test_back_to_nothing);
        tcase_add_test(tc_core, test_to_nothing_and_back_again);
        tcase_add_test(tc_core, test_start_stop);

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
        suite_free (s);
        return (nf == 0) ? 0:10;
}

/* ------------------------- static functions */
static void _run(const char *query, const char *file, int line)
{
        _mark_point(file, line);

        runner->run_query(runner, query);
}
static gboolean timeout_callback(gpointer userdata)
{
        gboolean *flag = (gboolean *)userdata;
        g_return_val_if_fail(flag, FALSE);
        g_return_val_if_fail(*flag==FALSE, FALSE);
        *flag=TRUE;
        return FALSE;
}

static void _get_results_counted(const char *query, int count, const char *file, int line)
{
        _mark_point(file, line);
        caller_location.file=file;
        caller_location.line=line;
        expected_result_count=count;
        current_result_count=0;
        expected_query=query;

        printf("%s:%d: expecting %d results for query %s...\n",
               file,
               line,
               count,
               query);

        gboolean timed_out = FALSE;
        guint timeout_1 = g_timeout_add(10000, timeout_callback, &timed_out);

        while(!timed_out && current_result_count<expected_result_count) {
                g_main_context_iteration(maincontext, TRUE/*may block*/);
                printf("%s:%d: result count %d/%d timeout:%s\n",
                       file,
                       line,
                       current_result_count,
                       expected_result_count,
                       timed_out ? "true":"false");
        }
        if(timed_out)
                _fail_unless(0, file, line, "timed out while waiting for query results");
        g_source_remove(timeout_1);
        if(current_result_count==expected_result_count) {
                gboolean timed_out_2 = FALSE;
                g_timeout_add(300, timeout_callback, &timed_out_2);
                while(!timed_out_2) {
                        g_main_context_iteration(maincontext, TRUE/*may block*/);
                        _fail_unless(current_result_count==expected_result_count, file, line, "too many results");
                }
        }

        printf("%s:%d: expectation met\n",
               file,
               line);
}

static void _assert_no_more_results(const char *file, int line)
{
        _mark_point(file, line);
        printf("%s:%d: waiting 3s to make sure there are no more results\n",
               file,
               line);
        caller_location.file=file;
        caller_location.line=line;

        expected_result_count=0;
        current_result_count=0;
        expected_query="<<no query>>";

        gboolean timed_out = FALSE;

        g_timeout_add(3000, timeout_callback, &timed_out);
        while(!timed_out)
                g_main_context_iteration(maincontext, TRUE/*may block*/);
}


static void result_handler(struct queryrunner *caller,
                           const char *query,
                           float pertinence,
                           struct result * result,
                           gpointer userdata)
{
        struct location *location = (struct location *)userdata;
        printf("got result for %s: %s\n",
               query,
               result->name);
        _fail_unless(caller==runner,
                     location->file,
                     location->line,
                     "wrong caller passed to callback");
        _fail_unless(strcmp(query, expected_query)==0,
                     location->file,
                     location->line,
                     g_strdup_printf("expected %d results for query '%s', but got a result for query '%s'",
                                     expected_result_count-current_result_count,
                                     expected_query,
                                     query));
        _fail_unless(current_result_count<expected_result_count,
                     location->file,
                     location->line,
                     g_strdup_printf("expected %d results for query '%s', got %d so far",
                                     expected_result_count,
                                     query,
                                     current_result_count+1));
        current_result_count++;
}

