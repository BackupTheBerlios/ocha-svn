#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include "result.h"
#include "queryrunner.h"
#include "result_queue.h"

struct mock_result
{
   struct result result;
   bool executed;
   bool released;
};

static bool execute_cb(struct result *);
static void release_cb(struct result *);
static struct mock_result results[] =
   {
      { { "result1", "/x/result1", execute_cb, release_cb }, },
      { { "result2", "/x/result2", execute_cb, release_cb }, },
      { { "result3", "/x/result3", execute_cb, release_cb }, },
      { { "result4", "/x/result4", execute_cb, release_cb }, },
      { { "result5", "/x/result5", execute_cb, release_cb }, },
      { { "result6", "/x/result6", execute_cb, release_cb }, },
   };
#define RESULTS_LEN (sizeof(results)/sizeof(struct mock_result))

#define assert_executed(result, expected) fail_unless(##result " executed ? expected " ##expected, results[result].executed==expected)
#define assert_released(result, expected) fail_unless(##result " released ? expected " ##expected, results[result].released==released)

static bool execute_cb(struct result *_result)
{
   struct mock_result *result = (struct mock_result *)_result;
   fail_unless(!result->released, "released");
   fail_unless(!result->executed, "already executed");
   result->executed=true;
}

static void release_cb(struct result *_result)
{
   struct mock_result *result = (struct mock_result *)_result;
   fail_unless(!result->released, "already released");
   result->released=true;
}

static void setup()
{
}

static void teardown()
{
}

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

START_TEST(test_newdelete)
{
   struct result_queue *queue = result_queue_new(NULL/*no context*/,
                                                 handler_not_called,
                                                 NULL/*userdata*/);
   result_queue_delete(queue);
}
END_TEST


START_TEST(test_add_result)
{
   struct result_queue *queue = result_queue_new(NULL/*no context*/,
                                                 handler_release,
                                                 NULL/*userdata*/);
   for(int i=0; i<RESULTS_LEN; i++)
      result_queue_add(queue,
                       NULL/*query runner*/,
                       "myquery",
                       0.0/*pertinence*/,
                       &results[i].result);

   while(g_main_context_pending(NULL/*default context*/))
      g_main_context_iteration(NULL/*default context*/,
                               true/*may block*/);
   for(int i=0; i<RESULTS_LEN; i++)
      {
         printf("check result %d\n", i);
         fail_unless(results[i].released,
                     "result i not released");
      }
}
END_TEST

START_TEST(test_add_result_from_several_threads)
{
   fail("write test");
}
END_TEST

START_TEST(test_gtk_loop)
{

   fail("write test");
}
END_TEST



Suite *result_queue_check_suite(void)
{
   Suite *s = suite_create("result_queue");
   TCase *tc_core = tcase_create("result_queue_core");

   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_newdelete);
   tcase_add_test(tc_core, test_add_result);
   tcase_add_test(tc_core, test_gtk_loop);
   return s;
}

int main(void)
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

