#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include "result.h"
#include "queryrunner.h"
#include "result_queue.h"
#include <gtk/gtk.h>

/** defined in result_queue.c, but not part of the public API */
extern int result_queue_counter;

struct mock_result
{
        struct result result;
        gboolean executed;
        gboolean released;
};

#define RESULTS_LEN (sizeof(results)/sizeof(struct mock_result))

#define assert_executed(result, expected) fail_unless(##result " executed ? expected " ##expected, results[result].executed==expected)
#define assert_released(result, expected) fail_unless(##result " released ? expected " ##expected, results[result].released==released)


#define TEST_VERBOSE

#ifdef TEST_VERBOSE
# define TEST_HEADER(name) printf("---- " #name "\n")
# define TEST_FOOTER() printf("----\n")
#else
# define TEST_HEADER(name)
# define TEST_FOOTER()
#endif

#define PRODUCER_THREAD_RESULT_COUNT 1000
#define PRODUCER_COUNT 10

static GPrivate* thread_identity;
static GMutex* thread_mutex;
static struct result_queue *thread_queue;
static int result_count;

/* ------------------------- prototypes: mock_result */
static gboolean mock_result_execute(struct result *, GError **);
static void mock_result_release(struct result *);
static gboolean mock_result_validate(struct result *);

/* ------------------------- prototypes: other */
static Suite *result_queue_check_suite(void);
static void handler_not_called(struct queryrunner *caller, const char *query, float pertinence, struct result *result, gpointer userdata);
static void handler_release(struct queryrunner *caller, const char *query, float pertinence, struct result *result, gpointer userdata);
static void add_result(struct result_queue  *queue, int i);
static void loop_as_long_as_necessary(void);
static struct result_queue* process_queue(void);
static void assert_all_released(void);
static gpointer producer_thread(gpointer mocks);
static void test_add_result_from_several_thread_handler(struct queryrunner *caller, const char *query, float pertinence, struct result *result, gpointer userdata);

/* ------------------------- definitions */
static struct mock_result results[] = {
        { { "result1", "Result 1", "/x/result1", TRUE/*enabled*/,
            mock_result_execute, mock_result_validate, mock_result_release }, },
        { { "result2", "Result 2", "/x/result2", TRUE/*enabled*/,
            mock_result_execute, mock_result_validate, mock_result_release }, },
        { { "result3", "Result 3", "/x/result3", TRUE/*enabled*/,
            mock_result_execute, mock_result_validate, mock_result_release }, },
        { { "result4", "Result 4", "/x/result4", TRUE/*enabled*/,
            mock_result_execute, mock_result_validate, mock_result_release }, },
        { { "result5", "Result 5", "/x/result5", TRUE/*enabled*/,
            mock_result_execute, mock_result_validate, mock_result_release }, },
        { { "result6", "Result 6", "/x/result6", TRUE/*enabled*/,
            mock_result_execute, mock_result_validate, mock_result_release }, },
};

/* ------------------------- test cases */
static void setup()
{
        g_thread_init(NULL/*vtable*/);
}

static void teardown()
{
        TEST_FOOTER();
}

START_TEST(test_newdelete)
{
        struct result_queue *queue;

        TEST_HEADER(test_newdelete);

        queue =  result_queue_new(NULL/*no context*/,
                                  handler_not_called,
                                  NULL/*userdata*/);
        result_queue_delete(queue);

}
END_TEST


START_TEST(test_add_result)
{
        TEST_HEADER(test_add_result);

        process_queue();
        assert_all_released();
}
END_TEST

START_TEST(test_quit)
{
        int oldcount;
        struct result_queue* queue;

        TEST_HEADER(test_quit);

        oldcount =  result_queue_counter;
        queue =  process_queue();
        result_queue_delete(queue);
        loop_as_long_as_necessary();
        fail_unless(oldcount==result_queue_counter, "result_queue not released");
}
END_TEST

struct add_result_cb_data
{
        struct result_queue *queue;
        int step;
};
static gboolean test_gtk_loop_cb(gpointer _data)
{
        struct add_result_cb_data *data = (struct add_result_cb_data *)_data;
        struct result_queue *queue=data->queue;
        if(data->step<RESULTS_LEN) {
                add_result(queue, data->step);
                data->step++;
                return TRUE;
        } else {
                result_queue_delete(queue);
                gtk_main_quit();
                return FALSE;
        }
}
START_TEST(test_gtk_loop)
{
        struct result_queue* queue;
        struct add_result_cb_data data;

        TEST_HEADER(test_gtk_loop);

        gtk_init(0, NULL);
        queue =  result_queue_new(NULL/*no context*/,
                                  handler_release,
                                  NULL/*userdata*/);
        data.queue=queue;
        data.step=0;
        g_timeout_add(10/*10 ms*/,
                      test_gtk_loop_cb,
                      &data);
        gtk_main();
        assert_all_released();
}
END_TEST


START_TEST(test_add_result_from_several_threads)
{
        GMainLoop* main_loop;
        struct mock_result* mocks[PRODUCER_COUNT];
        int i;

        fail_unless(g_main_context_acquire(NULL), "failed to acquire main context");
        TEST_HEADER(test_add_result_from_several_threads);
        thread_identity=g_private_new(NULL/*destructor*/);
        g_private_set(thread_identity, NULL);

        main_loop =  g_main_loop_new(NULL/*default context*/, FALSE/*not running*/);
        thread_mutex=g_mutex_new();
        g_mutex_lock(thread_mutex);


        thread_queue = result_queue_new(NULL/*no context*/,
                                        test_add_result_from_several_thread_handler,
                                        main_loop/*userdata*/);

        for(i=0; i<PRODUCER_COUNT; i++) {
                mocks[i]=g_new(struct mock_result, PRODUCER_THREAD_RESULT_COUNT);
                g_thread_create(producer_thread,
                                mocks[i]/*data*/,
                                FALSE/*joinable*/,
                                NULL/*error*/);
        }

        g_mutex_unlock(thread_mutex); /* launch producers */

        g_main_loop_run(main_loop);
        for(i=0; i<PRODUCER_COUNT; i++) {
                int j;
                for(j=0; j<PRODUCER_THREAD_RESULT_COUNT; j++) {
                        if(!mocks[i][j].released) {
                                printf("mock[%d][%d] not released\n", i, j);
                                fail("mock not released");
                        }
                }
                g_free(mocks[i]);
        }
}
END_TEST

/* ------------------------- member functions: mock_result */

static gboolean mock_result_execute(struct result *_result, GError **err)
{
        struct mock_result *result = (struct mock_result *)_result;
        fail_unless(!result->released, "released");
        fail_unless(!result->executed, "already executed");
        result->executed=TRUE;
        return TRUE;
}

static gboolean mock_result_validate(struct result *_result)
{
        fail(g_strdup_printf("unexpected call to validate on %s %s", _result->name, _result->path));
        return TRUE;
}
static void mock_result_release(struct result *_result)
{
        struct mock_result *result = (struct mock_result *)_result;
        fail_unless(!result->released, "already released");
        result->released=TRUE;
}

/* ------------------------- test suite */
static Suite *result_queue_check_suite(void)
{
        Suite *s = suite_create("result_queue");
        TCase *tc_core = tcase_create("result_queue_core");

        suite_add_tcase(s, tc_core);
        tcase_add_checked_fixture(tc_core, setup, teardown);
        tcase_add_test(tc_core, test_newdelete);
        tcase_add_test(tc_core, test_add_result);
        tcase_add_test(tc_core, test_quit);
        tcase_add_test(tc_core, test_gtk_loop);
        tcase_add_test(tc_core, test_add_result_from_several_threads);
        return s;
}

int main(int argc, char* argv[])
{
        int nf;
        Suite *s = result_queue_check_suite ();
        SRunner *sr = srunner_create (s);
        srunner_run_all (sr, CK_NORMAL);
        nf = srunner_ntests_failed (sr);
        srunner_free (sr);
        suite_free (s);
        return (nf == 0) ? 0:10;
}

/* ------------------------- static functions */
static void handler_not_called(struct queryrunner *caller,
                               const char *query,
                               float pertinence,
                               struct result *result,
                               gpointer userdata)
{
        fail("unexpected call");
}

static void handler_release(struct queryrunner *caller,
                            const char *query,
                            float pertinence,
                            struct result *result,
                            gpointer userdata)
{
        result->release(result);
}

static void add_result(struct result_queue  *queue, int i)
{
        result_queue_add(queue,
                         NULL/*query runner*/,
                         "myquery",
                         0.0/*pertinence*/,
                         &results[i].result);
}

static void loop_as_long_as_necessary()
{
        while(g_main_context_pending(NULL/*default context*/))
                g_main_context_iteration(NULL/*default context*/,
                                         TRUE/*may block*/);
}



static struct result_queue* process_queue()
{
        struct result_queue* queue = result_queue_new(NULL/*no context*/,
                                                             handler_release,
                                                             NULL/*userdata*/);
        int i;
        for(i=0; i<RESULTS_LEN; i++)
                add_result(queue, i);

        while(g_main_context_pending(NULL/*default context*/))
                g_main_context_iteration(NULL/*default context*/,
                                         TRUE/*may block*/);
        loop_as_long_as_necessary();
        return queue;
}

static void assert_all_released()
{
        int i;
        for(i=0; i<RESULTS_LEN; i++) {
                fail_unless(results[i].released,
                            "result i not released");
        }
}


static gpointer producer_thread(gpointer mocks)
{
        struct result_queue* queue = thread_queue;
        struct mock_result *mock = (struct mock_result *)mocks;
        int i;

        g_private_set(thread_identity, mocks);
        g_mutex_lock(thread_mutex);
        g_mutex_unlock(thread_mutex);

        for(i=0; i<PRODUCER_THREAD_RESULT_COUNT; i++) {
                mock[i].result.name="resultx";
                mock[i].result.long_name="resultx (long)";
                mock[i].result.path="resultx/path";
                mock[i].result.release=mock_result_release;
                mock[i].result.validate=mock_result_validate;
                mock[i].result.execute=mock_result_execute;
                mock[i].executed=FALSE;
                mock[i].released=FALSE;
                result_queue_add(queue,
                                 NULL/*runner*/,
                                 "myquery",
                                 1.0/*pertinence*/,
                                 &mock[i].result);
                g_thread_yield();
        }
        return NULL;
}

static void test_add_result_from_several_thread_handler(struct queryrunner *caller,
                                                        const char *query,
                                                        float pertinence,
                                                        struct result *result,
                                                        gpointer userdata)
{

        fail_unless(g_private_get(thread_identity)==NULL, "not on main thread");
        result->release(result);
        result_count++;
        if(result_count>=(PRODUCER_COUNT*PRODUCER_THREAD_RESULT_COUNT))
        {
                g_main_loop_quit((GMainLoop*)userdata);
        }
}


