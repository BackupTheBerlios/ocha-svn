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

/** \file test indexer_files AND indexer_applications */

#define TEMPDIR ".test"

static struct catalog *catalog;
static char *current_dir;
#define SOURCE_ID 23

static void deltree(const char *path);
static void touch(const char *path);
static void createfile(const char *path, const char *content);

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

/* ------------------------- private functions */
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
   mark_point();
   struct indexer_source *source=indexer_load_source(indexer, catalog, SOURCE_ID);
   fail_unless(source!=NULL, "no indexer_source");
   fail_unless(source->id==SOURCE_ID, "wrong source id");
   fail_unless(source->index!=NULL, "no index function in indexer_source");

   mark_point();
   GError *err=NULL;
   gboolean ret=indexer_source_index(source, catalog, &err);
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

/* ------------------------- applications */
static void setup_applications()
{
   setup();
}
static void setup_3_applications()
{
   createfile(TEMPDIR "/xmms.desktop",
              "[Desktop Entry]\n"
              "Name=XMMS\n"
              "Comment=X Multimedia System\n"
              "comment[az]=X Multimedya Sistemi\n"
              "comment[ca]=Sistema MultimËdia per a X\n"
              "comment[cs]=Multimedi·lnÌ p¯ehr·vaË\n"
              "comment[es]=Sistema Multimedia para X\n"
              "comment[gl]=Sistema Multimedia para X\n"
              "comment[hr]=X Multimedijski Sustav\n"
              "comment[nn]=X Multimedia-system\n"
              "comment[pt_BR]=X Multimedia System\n"
              "comment[ro]=X MultiMedia Sistem\n"
              "comment[th]=√–∫∫¡—≈µ‘¡’‡¥’¬∫π X\n"
              "comment[tr]=X Multimedya Sistem\n"
              "comment[zh_TW]=X ¶h¥C≈È®t≤Œ\n"
              "Encoding=Legacy-Mixed\n"
              "Exec=xmms\n"
              "Icon=/usr/share/icons/xmms_mini.xpm\n"
              "MimeType=audio/x-scpls;audio/x-mpegurl;audio/mpegurl;audio/mp3;audio/x-mp3;audio/mpeg;audio/x-mpeg;audio/x-wav;application/x-ogg\n"
              "Terminal=0\n"
              "Type=Application\n");
   createfile(TEMPDIR "/d1/xman.desktop",
              "[Desktop Entry]\n"
              "Type=Application\n"
              "Encoding=UTF-8\n"
              "Name=Xman\n"
              "Comment=Xman: manual page browser for X\n"
              "Icon=\n"
              "Exec=xman\n"
              "Terminal=FALSE\n"
              "Categories=X-Debian-Help\n");
   createfile(TEMPDIR "/d1/d2/xpdf.desktop",
              "[Desktop Entry]\n"
              "Type=Application\n"
              "Encoding=UTF-8\n"
              "Name=Xpdf\n"
              "Comment=Xpdf: Portable Document Format (PDF) file viewer for X\n"
              "Icon=\n"
              "Exec=/usr/bin/xpdf\n"
              "Terminal=FALSE\n"
              "Categories=X-Debian-Apps-Viewers\n");

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
   createfile(TEMPDIR "/digikam.desktop",
              "# KDE Config File\n"
              "[Desktop Entry]\n"
              "Encoding=UTF-8\n"
              "Type=Application\n"
              "Exec=digikam -caption \"%c\" %i %m\n"
              "Icon=digikam.png\n"
              "DocPath=digikam/index.html\n"
              "Name=Digikam\n"
              "Name[de]=Digikam\n"
              "Name[es]=Digikam\n"
              "Name[fr]=Digikam\n"
              "Name[da]=Digikam\n"
              "Name[it]=Digikam\n"
              "Name[hu]=Digikam\n"
              "Name[pl]=Digikam\n"
              "Name[nl]=Digikam\n"
              "Comment=KDE Photo Management\n"
              "Comment[de]=KDE Fotomanagement Programm\n"
              "Comment[es]=Programa de gesti√≥n de fotograf√≠as para KDE\n"
              "Comment[fr]=Interface KDE pour GPhoto2\n"
              "Comment[it]=Interfacchia KDE per la gestione di fotografie\n"
              "Comment[da]=KDE fotoh√•ndtering\n"
              "Comment[hu]=Digit√°lis fot√≥z√°s\n"
              "Comment[pl]=Program do zarzƒÖdzania zdjƒôciami\n"
              "Comment[nl]=Foto beheerprogramma\n"
              "Terminal=0\n");

   index(&indexer_applications);

   verify();
}
END_TEST

/* ------------------------- bookmarks */
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
   createfile(TEMPDIR "/bookmarks.html",
              "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n"
              "<!-- This is an automatically generated file.\n"
              "     It will be read and overwritten.\n"
              "     DO NOT EDIT! -->\n"
              "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n"
              "<TITLE>Bookmarks</TITLE>\n"
              "<H1>Bookmarks</H1>\n"
              "\n"
              "<DL><p>\n"
              "    <DT><H3 LAST_MODIFIED=\"1092503470\" PERSONAL_TOOLBAR_FOLDER=\"TRUE\" ID=\"rdf:#$0GJ1m2\">Bookmarks Toolbar Folder</H3>\n"
              "<DD>Add bookmarks to this folder to see them displayed on the Bookmarks Toolbar\n"
              "    <DL><p>\n"
              "        <DT><A HREF=\"http://slashdot.org/\" ADD_DATE=\"1084121070\" LAST_VISIT=\"1105484124\" LAST_MODIFIED=\"1084121093\" SHORTCUTURL=\"/.\" ICON=\"data:image/x-icon;base64,AAABAAEAEBAQAAAAAAAoAQAAFgAAACgAAAAQAAAAIAAAAAEABAAAAAAAwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgIAAAP///wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAREAAAERAAABEQAAEREQAAAREAARERAAABEQABEREAAAAREAAREAAAABEQAAAAAAAAAREAAAAAAAABEQAAAAAAAAAREAAAAAAAABEQAAAAAAAAAREAAAAAAAABEQAAAAAAAAAAAAAAAAAAAAAAAAD//wAA5+cAAMPDAADDgQAA4YEAAOGDAADwxwAA8P8AAPh/AAD4fwAA/D8AAPw/AAD+HwAA/j8AAP//AAD//wAA\" LAST_CHARSET=\"ISO-8859-1\" ID=\"rdf:#$iwElo2\">slashdot</A>\n"
              "        <DT><A HREF=\"http://www.groklaw.net/\" ADD_DATE=\"1084121165\" LAST_VISIT=\"1100344903\" ICON=\"data:image/x-icon;base64,AAABAAEAEBAQAAAAAABoAwAAFgAAACgAAAAQAAAAIAAAAAEAGAAAAAAAAAMAABILAAASCwAAAAAAAAAAAAD////////////X3t5eYmEzOjdMWVJ3i4B8kYVBS0U5PDxBS0UpLSvMzMz////////////////f5uZLTk5FT0p0iH18kYV8kYVxhHozOjczOjdqfXJMWVJeYmH///////////////9GSklGSkl8kYV8kYV8kYV8kYVFT0pXWlozOjd8kYV0iH0zOje/xcX///////90eXkzOjd3i4B8kYV8kYV8kYV0iH0eIB90eXlBS0V8kYV8kYVHUkxtcnH////X3t4cIB50iH18kYV8kYV8kYV8kYVBS0VXWlpBRURoeW98kYV8kYVxhHopLSvo7+9tcnFHUkx8kYV8kYV8kYV8kYV0iH0iJiSxt7czOjd6j4N8kYV8kYV8kYU5QDy1u7s7RUBtgHV8kYV8kYV8kYV8kYVHUkxeYmGAhIRFT0p8kYV8kYV8kYV8kYU7RUCZmZkzMzN8kYV8kYV8kYV8kYVxhHo5QDzMzMwzOjdjdGp8kYV8kYV8kYV8kYU7RUCJjo47RUB8kYV8kYV8kYV8kYVHUkxmZmbX3t4iJiR6j4N8kYV8kYV8kYV8kYU7RUCZmZlBS0V8kYV8kYV8kYV3i4AzMzPDycmHi4tFT0p8kYV8kYV8kYV8kYV8kYUzMzO/xcU5QDx8kYV8kYV8kYVSWlJeYmH///85QDxaaWB8kYV8kYV8kYV8kYVoeW8zMzPo7+85QDx8kYV8kYV8kYUiJiS/xcXf5uYKCwt6j4N8kYV8kYV8kYV3i4AnKymZmZn///87RUBaaWB8kYVMWVJeYmH///+Ok5M5QDx8kYV8kYV8kYV8kYU5QDxWWVn///////+orq4pLSt3i4AcIB61u7v///9NUFBaaWB8kYV8kYVqfXIzMzNeYmHo7+/////////o7+9eYmEzMzNBRUT////o7+8UFRV6j4NoeW9BS0U5QDyOk5Po7+/////////////////o7+9tcnHMzMz///+orq4cIB4zOjcpLSuHi4vX3t7////////////////////gAwAAwAMAAMABAACAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAwAAAAMAAAgHAACIHwAA\" LAST_CHARSET=\"ISO-8859-1\" ID=\"rdf:#$jwElo2\">GROKLAW</A>\n"
              "    </DL><p>\n"
              "</DL><p>\n"
              "<DL><p>\n"
              "    <DT><H3 ID=\"rdf:#$5GJ1m2\">Firefox &amp; Mozilla Information</H3>\n"
              "<DD>Information about Firefox and Mozilla\n"
              "    <DL><p>\n"
              "        <DT><A HREF=\"http://texturizer.net/firefox/extensions/\" LAST_VISIT=\"1086291939\" ICON=\"http://www.mozilla.org/products/firefox/ffico.png\" LAST_CHARSET=\"ISO-8859-1\" ID=\"rdf:#$6GJ1m2\">Firefox Extensions</A>\n"
              "<DD>Firefox add-ons and extensions\n"
              "        <DT><A HREF=\"http://texturizer.net/firefox/themes/\" LAST_VISIT=\"1086535629\" ICON=\"http://www.mozilla.org/products/firefox/ffico.png\" LAST_CHARSET=\"ISO-8859-1\" ID=\"rdf:#$7GJ1m2\">Firefox Themes</A>\n"
              "<DD>Firefox themes\n"
              "    </DL><p>\n"
              "</DL><p> \n");

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
   createfile(TEMPDIR "/bookmarks.html",
              "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n"
              "<!-- This is an automatically generated file.\n"
              "     It will be read and overwritten.\n"
              "     DO NOT EDIT! -->\n"
              "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n"
              "<TITLE>Bookmarks</TITLE>\n"
              "<H1>Bookmarks</H1>\n"
              "    <DT><H3 ID=\"rdf:#$5GJ1m2\">Firefox &amp; Mozilla Information</H3>\n"
              "<DD>Information about Firefox and Mozilla\n"
              "    <DL><p>\n"
              "        <DT><A HREF=\"http://texturizer.net/firefox/extensions/\" LAST_VISIT=\"1086291939\" ICON=\"http://www.mozilla.org/products/firefox/ffico.png\" LAST_CHARSET=\"ISO-8859-1\" ID=\"rdf:#$6GJ1m2\">&lt;Firefox&gt; add-ons &amp; &#40;Extensions&#41;</A>\n"
              "<DD>Firefox add-ons and extensions\n"
              "    </DL><p>\n"
              "</DL><p> \n");

   expect_url_entry("http://texturizer.net/firefox/extensions/",
                    "<Firefox> add-ons & (Extensions)");

   index(&indexer_mozilla);

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
gboolean gnome_execute_shell(char *dir, char *command)
{
   printf("gnome_execute_shell(%s, %s)\n", dir, command);
   return TRUE;
}
gboolean gnome_execute_terminal_shell(char *dir, char *command)
{
   printf("gnome_execute_terminal_shell(%s, %s)\n", dir, command);
   return TRUE;
}
gboolean gnome_execute_async(char *dir, int argc, char *const argv[])
{
   return TRUE;
}
/* ------------------------- test suite */

Suite *indexer_files_check_suite(void)
{
   Suite *s = suite_create("indexer_various");

   TCase *tc_files = tcase_create("tc_files");
   suite_add_tcase(s, tc_files);
   tcase_add_checked_fixture(tc_files, setup_files, teardown_files);
   tcase_add_test(tc_files, test_index);
   tcase_add_test(tc_files, test_limit_depth_1);
   tcase_add_test(tc_files, test_limit_depth_2);
   tcase_add_test(tc_files, test_ignore);

   TCase *tc_applications = tcase_create("tc_applications");
   suite_add_tcase(s, tc_applications);
   tcase_add_checked_fixture(tc_applications, setup_applications, teardown_applications);
   tcase_add_test(tc_applications, test_index_applications);
   tcase_add_test(tc_applications, test_index_applications_depth);
   tcase_add_test(tc_applications, test_index_applications_skip_withargs);

   TCase *tc_bookmarks = tcase_create("tc_bookmarks");
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
