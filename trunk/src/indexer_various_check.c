#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include "indexer_files.h"
#include "indexer_applications.h"
#include "indexer_mozilla.h"
#include "ocha_gconf.h"
#include "mock_catalog.h"
#include <libgnome/gnome-url.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnome/gnome-exec.h>

/** \file test indexer_files AND indexer_applications */

#define TEMPDIR ".test"

static struct catalog *catalog;
static char *current_dir;
#define SOURCE_ID 23

/* ------------------------- prototypes */
static Suite *indexer_files_check_suite(void);
static void set_depth(int depth);
static void set_ignore(char *ignore);
static char *to_uri(const char *filename);
static char *to_path(const char *filename);
static void index(struct indexer *indexer);
static void verify(void);
static void expect_entry(const char *file, const char *name, const char *long_name);
static void expect_url_entry(const char *url, const char *label);
static void expect_file_entry(const char *file);
static void deltree(const char *path);
static void touch(const char *path);
static void copyfile(const char *from, const char *to);

/* ------------------------- test case */
static void base_setup()
{
        catalog=mock_catalog_new();
        deltree(TEMPDIR);
        mkdir(TEMPDIR, 0700);
}

static void base_teardown()
{
        deltree(TEMPDIR);
}

static void setup()
{
        base_setup();
        mkdir(TEMPDIR "/d1", 0700);
        mkdir(TEMPDIR "/d1/d2", 0700);

        current_dir=g_get_current_dir();
        ocha_gconf_set_source_attribute("test",
                                        SOURCE_ID,
                                        "path",
                                        g_strdup_printf("%s/%s",
                                                        current_dir,
                                                        TEMPDIR));
}

static void teardown()
{
        base_teardown();
}

/* ------------------------- files */
static void setup_files()
{
        setup();
        mkdir(TEMPDIR "/CVS", 0700);
        touch(TEMPDIR "/x1.txt");
        touch(TEMPDIR "/x2.gif");
        touch(TEMPDIR "/d1/x3.txt");
        touch(TEMPDIR "/d1/d2/x4.txt");

}

static void teardown_files()
{
        teardown();
}


static void index_files()
{
        index(&indexer_files);
}

START_TEST(test_index)
{
        printf("test-index START");
        expect_file_entry("x1.txt");
        expect_file_entry("x2.gif");
        expect_file_entry("d1");
        expect_file_entry("d1/x3.txt");
        expect_file_entry("d1/d2");
        expect_file_entry("d1/d2/x4.txt");

        index_files();

        verify();
        printf("test-index PASS");
}
END_TEST

START_TEST(test_limit_depth_1)
{
        printf("test_limit_depth_1 START");
        set_depth(1);

        expect_file_entry("x1.txt");
        expect_file_entry("x2.gif");
        expect_file_entry("d1");

        index_files();

        verify();
        printf("test_limit_depth_1 PASS");
}
END_TEST

START_TEST(test_limit_depth_2)
{
        printf("test_limit_depth_2 START");
        set_depth(2);

        expect_file_entry("x1.txt");
        expect_file_entry("x2.gif");
        expect_file_entry("d1");
        expect_file_entry("d1/x3.txt");
        expect_file_entry("d1/d2");

        index_files();

        verify();
        printf("test_limit_depth_2 PASS");
}
END_TEST

START_TEST(test_ignore)
{
        printf("test_ignore START");
        set_ignore("*.txt,x3.txt,d2");

        touch(TEMPDIR "/x1.txt~"); /* hardcoded ignore pattern */
        touch(TEMPDIR "/CVS/x5.gif"); /* hardcoded ignore pattern */

        expect_file_entry("d1");
        expect_file_entry("x2.gif");

        index_files();

        verify();
        printf("test_ignore PASS");
}
END_TEST

/* ------------------------- test cases: applications */
static void setup_applications()
{
        setup();
}
static void setup_3_applications()
{
        copyfile("test_data/xmms.desktop", TEMPDIR "/xmms.desktop");
        copyfile("test_data/xman.desktop", TEMPDIR "/d1/xman.desktop");
        copyfile("test_data/xpdf.desktop", TEMPDIR "/d1/d2/xpdf.desktop");
}


static void teardown_applications()
{
        teardown();
}

START_TEST(test_index_applications)
{
        setup_3_applications();

        expect_entry("xmms.desktop", "XMMS", "X Multimedia System");
        expect_entry("d1/xman.desktop", "Xman", "Xman: manual page browser for X");
        expect_entry("d1/d2/xpdf.desktop", "Xpdf", "Xpdf: Portable Document Format (PDF) file viewer for X");

        index(&indexer_applications);

        verify();
}
END_TEST

START_TEST(test_index_applications_depth)
{
        setup_3_applications();
        set_depth(1);

        expect_entry("xmms.desktop", "XMMS", "X Multimedia System");

        index(&indexer_applications);

        verify();
}
END_TEST

START_TEST(test_index_applications_skip_withargs)
{
        copyfile("test_data/digikam.desktop", TEMPDIR "/digikam.desktop");
        index(&indexer_applications);

        verify();
}
END_TEST

/* ------------------------- test cases: bookmarks */
static void setup_bookmarks()
{
        base_setup();
        current_dir=g_get_current_dir();
        ocha_gconf_set_source_attribute("test",
                                        SOURCE_ID,
                                        "path",
                                        g_strdup_printf("%s/%s/%s",
                                                        current_dir,
                                                        TEMPDIR,
                                                        "bookmarks.html"));
}

static void teardown_bookmarks()
{
        base_teardown();
}

START_TEST(test_index_bookmarks)
{
        copyfile("test_data/bookmarks.html", TEMPDIR "/bookmarks.html");

        expect_url_entry("http://slashdot.org/",
                         "slashdot");
        expect_url_entry("http://www.groklaw.net/",
                         "GROKLAW");
        expect_url_entry("http://texturizer.net/firefox/extensions/",
                         "Firefox Extensions");
        expect_url_entry("http://texturizer.net/firefox/themes/",
                         "Firefox Themes");

        index(&indexer_mozilla);

        verify();
}
END_TEST

START_TEST(test_index_bookmarks_escape)
{
        copyfile("test_data/bookmarks_with_space.html", TEMPDIR "/bookmarks.html");
        expect_url_entry("http://texturizer.net/firefox/extensions/",
                         "<Firefox> add-ons & (Extensions)");

        index(&indexer_mozilla);

        verify();
}
END_TEST


/* ------------------------- mock gnome functions */
gboolean gnome_vfs_init()
{
        return TRUE;
}
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
int gnome_execute_shell (const char *dir, const char *commandline)
{
        printf("gnome_execute_shell(%s, %s)\n", dir, commandline);
        return 0;
}
int gnome_execute_terminal_shell (const char *dir, const char *commandline)
{
        printf("gnome_execute_terminal_shell(%s, %s)\n", dir, commandline);
        return 0;
}
int gnome_execute_async (const char *dir, int argc, char * const argv[])
{
        return 0;
}
GnomeVFSResult gnome_vfs_get_file_info(const gchar *text_uri, GnomeVFSFileInfo *info, GnomeVFSFileInfoOptions options)
{
        return 0;
}
const gchar *gnome_vfs_result_to_string(GnomeVFSResult r)
{
        return "mock ! mock !";
}
GnomeVFSResult gnome_vfs_open(GnomeVFSHandle **handle, const gchar *text_uri, GnomeVFSOpenMode open_mode)
{
        return 0;
}
GnomeVFSResult gnome_vfs_close(GnomeVFSHandle *h)
{
        return 0;
}
GnomeVFSResult gnome_vfs_read(GnomeVFSHandle *h, gpointer buffer, GnomeVFSFileSize bytes, GnomeVFSFileSize *bytes_read)
{
        return 0;
}
GnomeVFSResult gnome_vfs_read_entire_file(const char *uri, int *file_size, char **file_contents)
{
        return 0;
}

/* ------------------------- test suite */

static Suite *indexer_files_check_suite(void)
{
        Suite *s;
        TCase *tc_files;
        TCase *tc_applications;
        TCase *tc_bookmarks;

        s =  suite_create("indexer_various");

        tc_files =  tcase_create("tc_files");
        suite_add_tcase(s, tc_files);
        tcase_add_checked_fixture(tc_files, setup_files, teardown_files);
        tcase_add_test(tc_files, test_index);
        tcase_add_test(tc_files, test_limit_depth_1);
        tcase_add_test(tc_files, test_limit_depth_2);
        tcase_add_test(tc_files, test_ignore);

        tc_applications =  tcase_create("tc_applications");
        suite_add_tcase(s, tc_applications);
        tcase_add_checked_fixture(tc_applications, setup_applications, teardown_applications);
        tcase_add_test(tc_applications, test_index_applications);
        tcase_add_test(tc_applications, test_index_applications_depth);
        tcase_add_test(tc_applications, test_index_applications_skip_withargs);

        tc_bookmarks =  tcase_create("tc_bookmarks");
        suite_add_tcase(s, tc_bookmarks);
        tcase_add_checked_fixture(tc_bookmarks, setup_bookmarks, teardown_bookmarks);
        tcase_add_test(tc_bookmarks, test_index_bookmarks);
        tcase_add_test(tc_bookmarks, test_index_bookmarks_escape);

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
static void set_depth(int depth)
{
        ocha_gconf_set_source_attribute("test",
                                        SOURCE_ID,
                                        "depth",
                                        g_strdup_printf("%d",
                                                        depth));
}
static void set_ignore(char *ignore)
{
        ocha_gconf_set_source_attribute("test",
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

static void index(struct indexer *indexer)
{
        struct indexer_source *source;
        GError *err = NULL;
        gboolean ret;

        mark_point();
        source=indexer_load_source(indexer, catalog, SOURCE_ID);
        fail_unless(source!=NULL, "no indexer_source");
        fail_unless(source->id==SOURCE_ID, "wrong source id");
        fail_unless(source->index!=NULL, "no index function in indexer_source");

        mark_point();
        ret=indexer_source_index(source, catalog, &err);
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

static void expect_entry(const char *file, const char *name, const char *long_name)
{
        static int last_id=0;
        char *uri = to_uri(file);
        last_id++;
        mock_catalog_expect_addentry(catalog,
                                     uri,
                                     name,
                                     long_name,
                                     SOURCE_ID,
                                     last_id);
}

static void expect_url_entry(const char *url, const char *label)
{
        static int last_id=0;
        last_id++;
        mock_catalog_expect_addentry(catalog,
                                     url,
                                     label,
                                     url,
                                     SOURCE_ID,
                                     last_id);
}

static void expect_file_entry(const char *file)
{
        const char *basename = strrchr(file, '/');
        if(basename)
                basename++;/* skip the '/' */
        else
                basename=file;

        expect_entry(file, basename, to_path(file));
}


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

static void copyfile(const char *from, const char *to)
{
#define buffer_len 1024
        FILE *in, *out;
        char buffer[buffer_len];
        size_t count;

        in = fopen(from, "r");
        fail_unless(in!=NULL, g_strdup_printf("could not open file %s\n", from));

        out = fopen(to, "w");
        fail_unless(out!=NULL, g_strdup_printf("could not create file %s\n", to));

        while( (count=fread(buffer, 1, buffer_len, in)) >0 )
                fwrite(buffer, 1, count, out);

        fclose(in);
        fclose(out);
#undef buffer_len
}
