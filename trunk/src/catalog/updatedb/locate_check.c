#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include <glib.h>
#include "locate.h"

#define DBFILE "updatedb.tmp"

static GString *buffer;
static struct locate *locate;
static void setup()
{
   unlink(DBFILE);
   GString *gstr = g_string_new("updatedb --localpaths=\"");
   gchar *cwd=g_get_current_dir();
   g_string_append(gstr, cwd);
   g_free(cwd);
   g_string_append(gstr, "\" --output=" DBFILE);
   int retval = system(gstr->str);
   fail_unless(retval==0, "updatedb of " DBFILE " failed");

   buffer = g_string_new("");
   locate = locate_new("updatedb.h");
}

static void teardown()
{
   locate_delete(locate);
   g_string_free(buffer, true/*free content*/);
   unlink(DBFILE);
}

START_TEST(test_new_read_and_delete)
{
   fail_unless(locate!=NULL, "locate_new returned null");
   fail_unless(locate->fd >= 0, "locat.fd");
   fail_unless(locate->child_pid > 0, "locate.child_pid");

   int count=0;
   while(locate_next(locate, buffer)) {
      g_string_set_size(buffer, 0);
      count++;
   }
   fail_unless(count>1, "no results found for updatedb.h");
}
END_TEST

START_TEST(test_append)
{
   g_string_append(buffer, "XXX");
   locate_next(locate, buffer);
   fail_unless(strncmp("XXX", buffer->str, 3)==0, "start overwritten");
   fail_unless(buffer->len>3, "nothing appended");
   fail_unless(strstr(buffer->str, "updatedb.h")!=NULL, "updatedb.h not found in returned string");
}
END_TEST

START_TEST(test_has_more)
{
   do {
      while(!locate_has_more(locate));
      g_string_set_size(buffer, 0);
   }while(locate_next(locate, buffer));
}
END_TEST



Suite *locate_check_suite(void)
{
   Suite *s = suite_create("locate");
   TCase *tc_core = tcase_create("locate_core");

   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_new_read_and_delete);
   tcase_add_test(tc_core, test_append);
   tcase_add_test(tc_core, test_has_more);
   return s;
}

int main(void)
{
   int nf;
   Suite *s = locate_check_suite ();
   SRunner *sr = srunner_create (s);
   srunner_run_all (sr, CK_NORMAL);
   nf = srunner_ntests_failed (sr);
   srunner_free (sr);
   suite_free (s);
   return (nf == 0) ? 0:10;
}

