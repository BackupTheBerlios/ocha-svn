#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <check.h>
#include <glib.h>
#include "locate.h"

#define DBFILE "updatedb.tmp"
#define VERBOSE false

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

   const char *locate_cmd[] = {
      "locate",
      "--database",
      DBFILE,
      "updatedb.h",
      NULL
   };
   locate = locate_new_cmd(locate_cmd);
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

START_TEST(test_path)
{
   struct stat buf;
   int existing=0;
   while(locate_next(locate, buffer)) {
      if(stat(buffer->str, &buf)==0)
         existing++;
      g_string_set_size(buffer, 0);
   }
   fail_unless(existing>0, "no files returned by locate_next existed");
}
END_TEST

START_TEST(test_has_more)
{
   fail_unless(!locate_has_more(locate),
               "there can't be more data until locate_next has been called at least once");
   locate_next(locate, buffer);
}
END_TEST

START_TEST(test_after_end)
{
   while(locate_next(locate, NULL));
   for(int i=0; i<10; i++) {
      fail_unless(!locate_next(locate, NULL), "more to read ?");
      fail_unless(locate_has_more(locate), "locate_has_more must return true when end reached");
   }
}
END_TEST

START_TEST(test_lotsofdata)
{
   GString *str = g_string_new("");
   struct locate *locate = locate_new("a");
   int count=0;
   while(locate_next(locate, str)) {
      count++;
      if(VERBOSE) {
         if(count<10) {
            printf("[%d]: %20.20s\n", count, str->str);
         } else if(count==10) {
            printf("...\n");
         }
      }
      g_string_set_size(str, 0);
   }
   fail_unless(count>0, "nothing read ? nothing containing 'a' ?");
   locate_delete(locate);
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
   tcase_add_test(tc_core, test_after_end);
   tcase_add_test(tc_core, test_path);

   TCase *tc_lots = tcase_create("locate_lots");
   suite_add_tcase(s, tc_lots);
   tcase_add_test(tc_lots, test_lotsofdata);
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

