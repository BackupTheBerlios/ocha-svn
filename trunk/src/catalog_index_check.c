#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "catalog_index.h"
#include "mock_catalog.h"

#define TEMPDIR ".test"

static struct catalog *catalog;

static void deltree(const char *path);
static void touch(const char *path);

static void setup()
{
   catalog=mock_catalog_new();
   deltree(TEMPDIR);
   mkdir(TEMPDIR, 0700);
   mkdir(TEMPDIR "/d1", 0700);
   mkdir(TEMPDIR "/d1/d2", 0700);
   touch(TEMPDIR "/x1.txt");
   touch(TEMPDIR "/x2.gif");
   touch(TEMPDIR "/d1/x3.txt");
   touch(TEMPDIR "/d1/d2/x4.txt");

   catalog_index_init();
}

static void teardown()
{
   deltree(TEMPDIR);
}

START_TEST(test_index_directory)
{
   printf("---test_index_directory\n");
   int gnome_open_id=8;
   mock_catalog_expect_addcommand(catalog, "gnome-open", "gnome-open '%f'", gnome_open_id);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/x1.txt", "x1.txt", gnome_open_id, 1);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/x2.gif", "x2.gif", gnome_open_id, 2);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/d1/x3.txt", "x3.txt", gnome_open_id, 3);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/d1/d2/x4.txt", "x4.txt", gnome_open_id, 4);

   fail_unless(catalog_index_directory(catalog, TEMPDIR, -1, NULL/*ignore*/, false/*not slow*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);

   printf("---test_index_directory OK\n");
}
END_TEST

START_TEST(test_index_directory_maxdepth_1)
{
   printf("---test_index_directory_maxdepth_1\n");
   int gnome_open_id=8;
   mock_catalog_expect_addcommand(catalog, "gnome-open", "gnome-open '%f'", gnome_open_id);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/x1.txt", "x1.txt", gnome_open_id, 1);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/x2.gif", "x2.gif", gnome_open_id, 2);

   fail_unless(catalog_index_directory(catalog, TEMPDIR, 1, NULL/*ignore*/, false/*not slow*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);
   printf("---test_index_directory_maxdepth_1 OK\n");
}
END_TEST

START_TEST(test_index_directory_maxdepth_2)
{
   printf("---test_index_directory_maxdepth_2\n");
   int gnome_open_id=8;
   mock_catalog_expect_addcommand(catalog, "gnome-open", "gnome-open '%f'", gnome_open_id);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/x1.txt", "x1.txt", gnome_open_id, 1);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/x2.gif", "x2.gif", gnome_open_id, 2);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/d1/x3.txt", "x3.txt", gnome_open_id, 3);

   fail_unless(catalog_index_directory(catalog, TEMPDIR, 2, NULL/*ignore*/, false/*not slow*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);
   printf("---test_index_directory_maxdepth_2 OK\n");
}
END_TEST

START_TEST(test_index_directory_default_ignore)
{
   printf("---test_index_directory_default_ignore\n");
   touch(TEMPDIR "/.xA.txt");
   mkdir(TEMPDIR "/CVS", 0777);
   touch(TEMPDIR "/CVS/xB.txt");
   mkdir(TEMPDIR "/.svn", 0777);
   touch(TEMPDIR "/.svn/xC.txt");
   touch(TEMPDIR "/xD.txt~");
   touch(TEMPDIR "/#xE.txt#");
   touch(TEMPDIR "/xF.txt.bak");

   int gnome_open_id=8;
   mock_catalog_expect_addcommand(catalog, "gnome-open", "gnome-open '%f'", gnome_open_id);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/x1.txt", "x1.txt", gnome_open_id, 1);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/x2.gif", "x2.gif", gnome_open_id, 2);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/d1/x3.txt", "x3.txt", gnome_open_id, 3);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/d1/d2/x4.txt", "x4.txt", gnome_open_id, 4);

   fail_unless(catalog_index_directory(catalog, TEMPDIR, -1, NULL/*ignore*/, false/*not slow*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);
   printf("---test_index_directory_default_ignore OK\n");
}
END_TEST

START_TEST(test_index_directory_ignore_others)
{
   const char *ignore[] = { "*.txt", NULL };

   printf("---test_index_directory_ignore_others\n");

   int gnome_open_id=8;
   mock_catalog_expect_addcommand(catalog, "gnome-open", "gnome-open '%f'", gnome_open_id);
   mock_catalog_expect_addentry(catalog, TEMPDIR "/x2.gif", "x2.gif", gnome_open_id, 2);

   fail_unless(catalog_index_directory(catalog, TEMPDIR, -1, ignore, false/*not slow*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);
   printf("---test_index_directory_ignore_others OK\n");
}
END_TEST

/* ------------------------- mock gnome_vfs functions */
void gnome_vfs_init() {}
gchar *gnome_vfs_get_mime_type(const gchar *uri)
{
   return g_strdup("text/plain");
}
gchar *gnome_vfs_mime_get_default_desktop_entry(const gchar *mimetype)
{
   return g_strdup("emacs"); /* emacs can handle anything */
}
/* ------------------------- static functions */
static void deltree(const char *path)
{
   char *command=g_strdup_printf("rm -fr '%s'", path);
   fail_unless(system(command)==0, command);
}

static void touch(const char *path)
{
   FILE *handle=fopen(path, "w");
   fail_unless(handle!=NULL, path);
   fclose(handle);
}

/* ------------------------- test index */

Suite *catalog_index_check_suite(void)
{
   Suite *s = suite_create("catalog_index");
   TCase *tc_core = tcase_create("catalog_index_core");

   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_index_directory);
   tcase_add_test(tc_core, test_index_directory_maxdepth_1);
   tcase_add_test(tc_core, test_index_directory_maxdepth_2);
   tcase_add_test(tc_core, test_index_directory_default_ignore);
   tcase_add_test(tc_core, test_index_directory_ignore_others);

   return s;
}

int main(void)
{
   int nf;
   Suite *s = catalog_index_check_suite ();
   SRunner *sr = srunner_create (s);
   srunner_run_all (sr, CK_NORMAL);
   nf = srunner_ntests_failed (sr);
   srunner_free (sr);
   suite_free (s);
   return (nf == 0) ? 0:10;
}

