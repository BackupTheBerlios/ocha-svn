#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 
#include <stdio.h>
#include <string.h>
#include <check.h>
#include "target.h"

static void setup()
{
}

static void teardown()
{
}

START_TEST(test_new_url_target_with_url) 
{
   const char *name = "Search the web";
   const char *url = "http://www.google.com/";
   struct target *target = target_new_url_target(name, url);
   fail_unless(target!=NULL, "not created");
   fail_unless(target->name!=NULL, "target->name is null");
   fail_unless(strcmp(name, target->name)==0, "target->name");
   fail_unless(name!=target->name, "target->name not reallocated");
   fail_unless(strcmp(url, target->url)==0, "target->url");
   fail_unless(url!=target->url, "target->url not reallocated");
   fail_unless(strcmp(url, target->id)==0, "target->id");
   fail_unless(url!=target->id, "target->id not reallocated");
   fail_unless(target->filepath==NULL, "target->filepath");
   fail_unless(target->mempool!=NULL, "target->mempool");
   fail_unless(mempool_size(target->mempool)>=(sizeof(struct target)+strlen(name)+strlen(url)),
			   "suspicious mempool size");
} 
END_TEST

START_TEST(test_new_url_target_with_file) 
{
   const char *name="My Resume";
   const char *url="file:///home/stephane/resume.tex";
   struct target *target = target_new_url_target(name, url);
   fail_unless(target->filepath!=NULL, "target->filepath not set");
   fail_unless(strcmp("/home/stephane/resume.tex", target->filepath)==0,
			   "wrong filepath");
   fail_unless(target->filepath<url || target->filepath>(url+strlen(url)),
			   "memory from url used to generate target->filepath");
} 
END_TEST


START_TEST(test_new_file_target) 
{
   const char *filepath = "/home/stephane/resume.tex";
   const struct target *target = target_new_file_target(filepath);
   fail_unless(strcmp(filepath, target->filepath)==0,
			   "target->filepath");
   fail_unless(strcmp("file:///home/stephane/resume.tex", 
						target->url)==0, 
			   "target->filepath");
   fail_unless(strcmp("file:///home/stephane/resume.tex", 
						target->id)==0, 
			   "target->id should be url");
   fail_unless(target->name!=NULL, 
			   "target->name is null");
   fail_unless(strcmp("resume.tex", 
						target->name),
			   "target->name");
   fail_unless(filepath!=target->filepath,
			   "filepath's memory reused for target");
   fail_unless(target->name<filepath || target->name>(filepath+strlen(filepath)),
			   "memory from filepath used to generate target->name");
} 
END_TEST

static int test_target_pool_calls=0;
static void test_target_pool_cb(void *ptr)
{
   fail_unless(ptr==(void*)0x1ee1, "wrong resource pointer");
   fail_unless(test_target_pool_calls==0, "too many calls");
   test_target_pool_calls++;
}
START_TEST(test_target_pool) 
{
   struct target *target = target_new_file_target("/tmp/toto");
   test_target_pool_calls=0;
   mempool_enlist(target->mempool, 
				  (void*)0x1ee1, 
				  test_target_pool_cb);
   fail_unless(test_target_pool_calls==0, 
			   "called too early");
   target_unref(target);
   fail_unless(test_target_pool_calls==1, 
			   "not called");
} 
END_TEST

START_TEST(test_refcounting) 
{
   struct target *target = target_new_file_target("/tmp/toto");
   test_target_pool_calls=0;
   mempool_enlist(target->mempool, 
				  (void*)0x1ee1, 
				  test_target_pool_cb);
   fail_unless(target==target_ref(target), 
			   "wrong return value for target_ref()");
   target_unref(target);
   fail_unless(test_target_pool_calls==0, 
			   "called too early (1st unref)");

   for(int i=0; i<5; i++)
	  target_ref(target);
   for(int i=0; i<5; i++) {
	  target_unref(target);
	  fail_unless(test_target_pool_calls==0, 
				  "called too early (serial unrefs)");
   }

   target_unref(target);
   fail_unless(test_target_pool_calls==1, 
			   "not called");   
} 
END_TEST


START_TEST(test_set_mime) 
{
   const char *mimetype = "text/plain";
   struct target *target = target_new_file_target("/tmp/toto");
   
   fail_unless(target_get_mimetype(target)==NULL, 
			   "expected no mimetype");
   target_set_mimetype(target, mimetype);
   fail_unless(target_get_mimetype(target)!=NULL, 
			   "expected mimetype");
   fail_unless(strcmp(target_get_mimetype(target),
						mimetype)==0, 
			   "wrong mimetype");
   fail_unless(mimetype!=target_get_mimetype(target),
			   "memory from mimetype reusef by target_set_mimetype()");

   target_set_mimetype(target, NULL);
   fail_unless(target_get_mimetype(target)==NULL,
			   "mimetype not cleared");
} 
END_TEST

static struct target *target;
static struct target_action action1 = {
   .name = "action1"
};
static struct target_action action2 = {
   .name = "action2"
};
static struct target_action action3 = {
   .name = "action3"
};
static struct target_action* last_action_called;
static int action_calls;
static void target_action_callback(struct target *t, struct target_action *a) 
{
   fail_unless(t==target, "wrong target");
   fail_unless(a!=NULL, "wrong action");
   fail_unless(a->name!=NULL, "invalid action");
   last_action_called=a;
   action_calls++;
}
static void setup_actions()
{
   target=target_new_file_target("/tmp/not_found");
}
static void teardown_actions()
{
   target_unref(target);
}
START_TEST(test_add_and_execute_action) 
{
   fail("write");
} 
END_TEST

START_TEST(test_add_and_remove_action) 
{
   fail("write");
} 
END_TEST

START_TEST(test_remove_unknown_action) 
{
   fail("write");
} 
END_TEST

START_TEST(test_execute_unknown_action) 
{
   fail("write");
} 
END_TEST

static void setup_get_actions()
{
   setup_actions();
   target_add_action(target, &action1, target_action_callback);
   target_add_action(target, &action2, target_action_callback);
   target_add_action(target, &action3, target_action_callback);
}
static void teardown_get_actions()
{

   teardown_actions();
}

START_TEST(test_get_actions_null) 
{
   fail("write");
} 
END_TEST
START_TEST(test_get_actions_exact) 
{
   fail("write");
} 
END_TEST
START_TEST(test_get_actions_toosmall) 
{
   fail("write");
} 
END_TEST
START_TEST(test_get_actions_toolarge) 
{
   fail("write");
} 
END_TEST



Suite *target_check_suite(void)
{
   Suite *s = suite_create("target");

   TCase *tc_core = tcase_create("target_core");
   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_new_url_target_with_url);
   tcase_add_test(tc_core, test_new_url_target_with_file);
   tcase_add_test(tc_core, test_new_file_target);
   tcase_add_test(tc_core, test_target_pool);
   tcase_add_test(tc_core, test_refcounting);
   tcase_add_test(tc_core, test_set_mime);

   TCase *tc_actions = tcase_create("target_actions");
   suite_add_tcase(s, tc_actions);
   tcase_add_checked_fixture(tc_actions, setup_actions, teardown_actions);
   tcase_add_test(tc_actions, test_add_and_execute_action);   
   tcase_add_test(tc_actions, test_add_and_remove_action);   
   tcase_add_test(tc_actions, test_remove_unknown_action);   
   tcase_add_test(tc_actions, test_execute_unknown_action);   

   TCase *tc_get_actions = tcase_create("target_get_actions");
   suite_add_tcase(s, tc_get_actions);
   tcase_add_checked_fixture(tc_get_actions, setup_get_actions, teardown_get_actions);
   tcase_add_test(tc_get_actions, test_get_actions_null);      
   tcase_add_test(tc_get_actions, test_get_actions_exact);      
   tcase_add_test(tc_get_actions, test_get_actions_toosmall);      
   tcase_add_test(tc_get_actions, test_get_actions_toolarge);      
   return s;
}

int main(void)
{
  int nf; 
  Suite *s = target_check_suite (); 
  SRunner *sr = srunner_create (s); 
  srunner_run_all (sr, CK_NORMAL); 
  nf = srunner_ntests_failed (sr); 
  srunner_free (sr); 
  suite_free (s); 
  return (nf == 0) ? 0:10;
}
