#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 
#include <stdio.h>
#include <check.h>
#include "updatedb.h"

static void setup()
{
}

static void teardown()
{
}

START_TEST(test_) 
{
} 
END_TEST


Suite *updatedb_check_suite(void)
{
   Suite *s = suite_create("updatedb");
   TCase *tc_core = tcase_create("updatedb_core");
   
   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_);
   
   return s;
}

int main(void)
{
  int nf; 
  Suite *s = updatedb_check_suite (); 
  SRunner *sr = srunner_create (s); 
  srunner_run_all (sr, CK_NORMAL); 
  nf = srunner_ntests_failed (sr); 
  srunner_free (sr); 
  suite_free (s); 
  return (nf == 0) ? 0:10;
}
