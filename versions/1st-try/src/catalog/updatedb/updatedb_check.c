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

START_TEST(test_updatedb_new_and_delete)
{
   struct catalog *catalog = updatedb_new();
   fail_unless(catalog!=NULL, "returned null");
   fail_unless(catalog->new_search!=NULL, "no new_search");
   fail_unless(catalog->update!=NULL, "no update");
   fail_unless(catalog->delete!=NULL, "no delete");
   catalog->delete(catalog);
}
END_TEST

START_TEST(test_is_available)
{
   /* I should disable the tests if this fails
    * on the current machine
    */
   fail_unless(updatedb_is_available(),
               "locate does not work, none of the test cases for updatedb will work");

}
END_TEST

static void cb_ignore(struct catalog_search *search, float confidence, struct target* target) {}

START_TEST(test_get_query_and_append)
{
   const int buflen = 256;
   char buffer[buflen];
   struct catalog *catalog = updatedb_new();
   struct catalog_search *search = catalog->new_search(catalog, cb_ignore);
   fail_unless(search->get_query(search, buffer, buflen)==0,
               "already a query?");
   fail_unless(buffer[0]!=0,
               "buffer not modified");
   search->append(search, "/bin/");
   fail_unless(search->get_query(search, buffer, buflen)==strlen("/bin/"),
               "expected /bin/");
   fail_unless(strcmp("/bin/", buffer)==0,
               "wrong content for buffer (expected /bin/)");
   search->append(search, "sh");
   fail_unless(search->get_query(search, buffer, buflen)==strlen("/bin/sh"),
               "expected /bin/sh");
   fail_unless(strcmp("/bin/sh", buffer)==0,
               "wrong content for buffer (expected /bin/sh)");
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
