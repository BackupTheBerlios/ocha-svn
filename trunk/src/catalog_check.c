#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "catalog.h"

#define PATH ".catalog_check.tmp"

static bool exists(const char *path)
{
   struct stat buf;
   return stat(path, &buf)==0 && buf.st_size>0;
}

static void setup()
{
   if(exists(PATH))
      {
         fail_unless(unlink(PATH)==0,
                     "temporary file " PATH "exists and could not be deleted\n");
      }
}

static void teardown()
{
}


START_TEST(test_create)
{
   struct catalog *catalog=catalog_connect(PATH, NULL);
   fail_unless(catalog!=NULL, "NULL catalog");
   fail_unless(exists(PATH), "catalog file not created in " PATH);
   catalog_disconnect(catalog);
}
END_TEST

START_TEST(test_create_errmsg)
{
   fail("write test");
}
END_TEST

START_TEST(test_execute_query)
{
   fail("write test");
}
END_TEST

START_TEST(test_execute_query_errmsg)
{
   fail("write test");
}
END_TEST


START_TEST(test_callback_stops_quey)
{
   fail("write test");
}
END_TEST

START_TEST(test_interrupt_stops_query)
{
   fail("write test");
}
END_TEST

START_TEST(test_interrupt_from_other_thread)
{
   fail("write test");
}
END_TEST

START_TEST(test_wait_for_busy)
{
   fail("write test");
}
END_TEST

START_TEST(test_wait_for_busy_interrupted)
{
   fail("write test");
}
END_TEST


Suite *catalog_check_suite(void)
{
   Suite *s = suite_create("catalog");
   TCase *tc_core = tcase_create("catalog_core");

   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_create);
   tcase_add_test(tc_core, test_create_errmsg);
   tcase_add_test(tc_core, test_execute_query);
   tcase_add_test(tc_core, test_execute_query_errmsg);
   tcase_add_test(tc_core, test_callback_stops_quey);
   tcase_add_test(tc_core, test_interrupt_stops_query);
   tcase_add_test(tc_core, test_interrupt_from_other_thread);
   tcase_add_test(tc_core, test_wait_for_busy);
   tcase_add_test(tc_core, test_wait_for_busy_interrupted);

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

