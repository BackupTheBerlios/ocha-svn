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
   g_thread_init(NULL/*vtable*/);
   printf("---- start:\n");
}

static void teardown()
{
   printf("---- end\n");
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

static void add_result(struct result_queue  *queue, int i) {
   result_queue_add(queue,
                    NULL/*query runner*/,
                    "myquery",
                    0.0/*pertinence*/,
                    &results[i].result);
}

static void loop_as_long_as_necessary() {
   while(g_main_context_pending(NULL/*default context*/))
      g_main_context_iteration(NULL/*default context*/,
                               true/*may block*/);
}



static struct result_queue* process_queue() {
   struct result_queue* queue = result_queue_new(NULL/*no context*/,
                                                 handler_release,
                                                 NULL/*userdata*/);
   for(int i=0; i<RESULTS_LEN; i++)
      add_result(queue, i);

   while(g_main_context_pending(NULL/*default context*/))
      g_main_context_iteration(NULL/*default context*/,
                               true/*may block*/);
   loop_as_long_as_necessary();
   return queue;
}

static void assert_all_released() {
   for(int i=0; i<RESULTS_LEN; i++)
      {
         printf("check result %d\n", i);
         fail_unless(results[i].released,
                     "result i not released");
      }
}




START_TEST(test_add_result)
{
   process_queue();
   assert_all_released();
}
END_TEST

START_TEST(test_quit)
{
   int oldcount = result_queue_counter;
   struct result_queue* queue = process_queue();
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
   if(data->step<RESULTS_LEN)
      {
         add_result(queue, data->step);
         data->step++;
         return TRUE;
      }
   else
      {
         result_queue_delete(queue);
         gtk_main_quit();
         return FALSE;
      }
}
START_TEST(test_gtk_loop)
{
   gtk_init(0, NULL);
   struct result_queue* queue = result_queue_new(NULL/*no context*/,
                                                 handler_release,
                                                 NULL/*userdata*/);
   struct add_result_cb_data data = { .queue=queue, .step=0 };
   g_timeout_add(10/*10 ms*/,
                 test_gtk_loop_cb,
                 &data);
   gtk_main();
   assert_all_released();
}
END_TEST




START_TEST(test_add_result_from_several_threads)
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
   tcase_add_test(tc_core, test_quit);
   tcase_add_test(tc_core, test_gtk_loop);
   tcase_add_test(tc_core, test_add_result_from_several_threads);
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

