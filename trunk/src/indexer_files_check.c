#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include "indexer_files.h"
#include "mock_catalog.h"
#include <libgnome/gnome-url.h>
#include <libgnomevfs/gnome-vfs.h>

#define TEMPDIR ".test"

static struct catalog *catalog;
static char *current_dir;
#define SOURCE_ID 23

static void deltree(const char *path);
static void touch(const char *path);
static void createfile(const char *path, const char *content);

static void setup()
{
   catalog=mock_catalog_new();
   deltree(TEMPDIR);
   mkdir(TEMPDIR, 0700);
   mkdir(TEMPDIR "/d1", 0700);
   mkdir(TEMPDIR "/d1/d2", 0700);
   mkdir(TEMPDIR "/CVS", 0700);
   touch(TEMPDIR "/x1.txt");
   touch(TEMPDIR "/x2.gif");
   touch(TEMPDIR "/d1/x3.txt");
   touch(TEMPDIR "/d1/d2/x4.txt");

   current_dir=g_get_current_dir();
   catalog_set_source_attribute(catalog,
                                SOURCE_ID,
                                "path",
                                g_strdup_printf("%s/%s",
                                                current_dir,
                                                TEMPDIR));
}

static void teardown()
{
   deltree(TEMPDIR);
}

static void set_depth(int depth)
{
   catalog_set_source_attribute(catalog,
                                SOURCE_ID,
                                "depth",
                                g_strdup_printf("%d",
                                                depth));
}
static void set_ignore(char *ignore)
{
   catalog_set_source_attribute(catalog,
                                SOURCE_ID,
                                "ignore",
                                ignore);
}

static char *to_uri(const char *filename)
{
   return g_strdup_printf("file://%s/%s/%s",
                          current_dir,
                          TEMPDIR,
                          filename);
}
static char *to_path(const char *filename)
{
   return g_strdup_printf("%s/%s/%s",
                          current_dir,
                          TEMPDIR,
                          filename);
}
static void expect_entry(const char *file)
{
   static int last_id=0;
   char *uri = to_uri(file);
   last_id++;
   const char *basename = strrchr(file, '/');
   if(basename)
      basename++;/* skip the '/' */
   else
      basename=file;

   mock_catalog_expect_addentry(catalog,
                                uri,
                                basename,
                                to_path(file),
                                SOURCE_ID,
                                last_id);
}

static void index()
{
   mark_point();
   struct indexer_source *source=indexer_files.load_indexer_source(&indexer_files, catalog, SOURCE_ID);
   fail_unless(source!=NULL, "no indexer_source");
   fail_unless(source->id==SOURCE_ID, "wrong source id");
   fail_unless(source->index!=NULL, "no index function in indexer_source");

   mark_point();
   GError *err=NULL;
   gboolean ret=source->index(source, catalog, &err);
   if(!ret)
      {
         mark_point();
         fail_unless(err!=NULL, "indexing failed and err==NULL");
         fail(g_strdup_printf("indexing failed: %s", err->message));
      }
   mark_point();
}

static void verify()
{
   mark_point();
   mock_catalog_assert_expectations_met(catalog);
}

START_TEST(test_index)
{
   expect_entry("x1.txt");
   expect_entry("x2.gif");
   expect_entry("d1/x3.txt");
   expect_entry("d1/d2/x4.txt");

   index();

   verify();
}
END_TEST

START_TEST(test_limit_depth_1)
{
   set_depth(1);

   expect_entry("x1.txt");
   expect_entry("x2.gif");

   index();

   verify();
}
END_TEST

START_TEST(test_limit_depth_2)
{
   set_depth(2);

   expect_entry("x1.txt");
   expect_entry("x2.gif");
   expect_entry("d1/x3.txt");

   index();

   verify();
}
END_TEST

START_TEST(test_ignore)
{
   set_ignore("*.txt:x3.txt:d2");

   touch(TEMPDIR "/x1.txt~"); /* hardcoded ignore pattern */
   touch(TEMPDIR "/CVS/x5.gif"); /* hardcoded ignore pattern */

   expect_entry("x2.gif");

   index();

   verify();
}
END_TEST

/* ------------------------- mock gnome functions */
gboolean gnome_vfs_init() {}
gchar *gnome_vfs_get_mime_type(const gchar *uri)
{
   return g_strdup("text/plain");
}
gchar *gnome_vfs_mime_get_default_desktop_entry(const gchar *mimetype)
{
   return g_strdup("emacs"); /* emacs can handle anything */
}

gboolean gnome_url_show(const char *url, GError **err)
{
   g_return_val_if_fail(url!=NULL, FALSE);
   g_return_val_if_fail(err==NULL || (*err)==NULL, FALSE);

   printf("show %s\n", url);
   return TRUE;
}

GnomeVFSURI *gnome_vfs_uri_new(const char *uri)
{
   g_return_val_if_fail(uri, FALSE);
   return (GnomeVFSURI *)0xd00d;
}
void gnome_vfs_uri_unref(GnomeVFSURI *uri)
{
   g_return_if_fail(uri!=(GnomeVFSURI *)0xd00d);
}
gboolean gnome_vfs_uri_exists(GnomeVFSURI *uri)
{
   g_return_val_if_fail(uri!=(GnomeVFSURI *)0xd00d, FALSE);
   return TRUE;
}
/* ------------------------- test suite */

Suite *indexer_files_check_suite(void)
{
   Suite *s = suite_create("indexer_files");
   TCase *tc_core = tcase_create("indexer_files_core");

   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_index);
   tcase_add_test(tc_core, test_limit_depth_1);
   tcase_add_test(tc_core, test_limit_depth_2);
   tcase_add_test(tc_core, test_ignore);

   return s;
}

int main(void)
{
   int nf;
   Suite *s = indexer_files_check_suite ();
   SRunner *sr = srunner_create (s);
   srunner_run_all (sr, CK_NORMAL);
   nf = srunner_ntests_failed (sr);
   srunner_free (sr);
   suite_free (s);
   return (nf == 0) ? 0:10;
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
static void createfile(const char *path, const char *content)
{
   FILE *handle=fopen(path, "w");
   fail_unless(handle!=NULL, path);
   fwrite(content, sizeof(char), strlen(content), handle);
   fclose(handle);
}
