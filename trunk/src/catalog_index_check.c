#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include "catalog_index.h"
#include "mock_catalog.h"

#define TEMPDIR ".test"

static struct catalog *catalog;

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

   catalog_index_init();
}

static void teardown()
{
   deltree(TEMPDIR);
}

/* ------------------------- index_directory */
static void setup_index_directory()
{
   setup();
   touch(TEMPDIR "/x1.txt");
   touch(TEMPDIR "/x2.gif");
   touch(TEMPDIR "/d1/x3.txt");
   touch(TEMPDIR "/d1/d2/x4.txt");
}

static void teardown_index_directory()
{
   teardown();
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



/* ------------------------- index applications */
static void setup_index_applications()
{
   setup();


}

static void setup_three_applications()
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
              "Terminal=false\n"
              "Categories=X-Debian-Help\n");
   createfile(TEMPDIR "/d1/d2/xpdf.desktop",
              "[Desktop Entry]\n"
              "Type=Application\n"
              "Encoding=UTF-8\n"
              "Name=Xpdf\n"
              "Comment=Xpdf: Portable Document Format (PDF) file viewer for X\n"
              "Icon=\n"
              "Exec=/usr/bin/xpdf\n"
              "Terminal=false\n"
              "Categories=X-Debian-Apps-Viewers\n");
}

static void teardown_index_applications()
{

   teardown();
}

START_TEST(test_index_applications)
{
   printf("---test_index_applications\n");
   setup_three_applications();

   int run_id=2;
   mock_catalog_expect_addcommand(catalog,
                                  "run-desktop-entry",
                                  "run-desktop-entry '%f'",
                                  run_id);
   mock_catalog_expect_addentry(catalog,
                                TEMPDIR "/xmms.desktop",
                                "XMMS",
                                run_id,
                                1);
   mock_catalog_expect_addentry(catalog,
                                TEMPDIR "/d1/xman.desktop",
                                "Xman",
                                run_id,
                                2);
   mock_catalog_expect_addentry(catalog,
                                TEMPDIR "/d1/d2/xpdf.desktop",
                                "Xpdf",
                                run_id,
                                3);

   fail_unless(catalog_index_applications(catalog,
                                          TEMPDIR,
                                          -1,
                                          false/*not slow*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);

   printf("---test_index_applications OK\n");
}
END_TEST

START_TEST(test_index_applications_depth)
{
   printf("---test_index_applications_depth\n");
   setup_three_applications();

   int run_id=2;
   mock_catalog_expect_addcommand(catalog,
                                  "run-desktop-entry",
                                  "run-desktop-entry '%f'",
                                  run_id);
   mock_catalog_expect_addentry(catalog,
                                TEMPDIR "/xmms.desktop",
                                "XMMS",
                                run_id,
                                1);

   fail_unless(catalog_index_applications(catalog,
                                          TEMPDIR,
                                          1,
                                          false/*not slow*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);

   printf("---test_index_applications_depth OK\n");
}
END_TEST

START_TEST(test_index_applications_skip_terminal)
{
   printf("---test_index_applications_skip_terminal\n");
   createfile(TEMPDIR "/zile.desktop",
              "[Desktop Entry]\n"
              "Type=Application\n"
              "Encoding=UTF-8\n"
              "Name=zile\n"
              "Comment=\n"
              "Icon=\n"
              "Exec=/usr/bin/zile\n"
              "Terminal=true\n"
              "Categories=X-Debian-Apps-Editors\n");

   int run_id=2;
   mock_catalog_expect_addcommand(catalog,
                                  "run-desktop-entry",
                                  "run-desktop-entry '%f'",
                                  run_id);

   fail_unless(catalog_index_applications(catalog,
                                          TEMPDIR,
                                          1,
                                          false/*not slow*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);

   printf("---test_index_applications_skip_terminal OK\n");
}
END_TEST

START_TEST(test_index_applications_skip_withargs)
{
   printf("---test_index_applications_skip_withargs\n");
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

   int run_id=2;
   mock_catalog_expect_addcommand(catalog,
                                  "run-desktop-entry",
                                  "run-desktop-entry '%f'",
                                  run_id);

   fail_unless(catalog_index_applications(catalog,
                                          TEMPDIR,
                                          1,
                                          false/*not slow*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);

   printf("---test_index_applications_skip_withargs OK\n");
}
END_TEST

/* ------------------------- index bookmarks */
static void setup_index_bookmarks()
{
   setup();

}

static void teardown_index_bookmarks()
{

   teardown();
}

START_TEST(test_index_bookmarks)
{
   printf("---test_index_bookmarks\n");
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
              "    <DT><H3 LAST_MODIFIED=\"1092503470\" PERSONAL_TOOLBAR_FOLDER=\"true\" ID=\"rdf:#$0GJ1m2\">Bookmarks Toolbar Folder</H3>\n"
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

   int run_id=2;
   mock_catalog_expect_addcommand(catalog,
                                  "gnome-moz-remote",
                                  "gnome-moz-remote '%f'",
                                  run_id);
   mock_catalog_expect_addentry(catalog,
                                "http://slashdot.org/",
                                "slashdot",
                                run_id,
                                1);
   mock_catalog_expect_addentry(catalog,
                                "http://www.groklaw.net/",
                                "GROKLAW",
                                run_id,
                                2);
   mock_catalog_expect_addentry(catalog,
                                "http://texturizer.net/firefox/extensions/",
                                "Firefox Extensions",
                                run_id,
                                3);
   mock_catalog_expect_addentry(catalog,
                                "http://texturizer.net/firefox/themes/",
                                "Firefox Themes",
                                run_id,
                                4);

   fail_unless(catalog_index_bookmarks(catalog,
                                       TEMPDIR "/bookmarks.html"),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);

   printf("---test_index_bookmarks OK\n");
}
END_TEST

START_TEST(test_index_bookmarks_escape)
{
   printf("---test_index_bookmarks_escape\n");

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

   int run_id=2;
   mock_catalog_expect_addcommand(catalog,
                                  "gnome-moz-remote",
                                  "gnome-moz-remote '%f'",
                                  run_id);
   mock_catalog_expect_addentry(catalog,
                                "http://texturizer.net/firefox/extensions/",
                                "<Firefox> add-ons & (Extensions)",
                                run_id,
                                3);

   fail_unless(catalog_index_bookmarks(catalog,
                                       TEMPDIR "/bookmarks.html"),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);


   printf("---test_index_bookmarks_escape OK\n");
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
static void createfile(const char *path, const char *content)
{
   FILE *handle=fopen(path, "w");
   fail_unless(handle!=NULL, path);
   fwrite(content, sizeof(char), strlen(content), handle);
   fclose(handle);
}

/* ------------------------- test index */

Suite *catalog_index_check_suite(void)
{
   Suite *s = suite_create("catalog_index");

   TCase *tc_directory = tcase_create("catalog_index_directory");
   suite_add_tcase(s, tc_directory);
   tcase_add_checked_fixture(tc_directory, setup_index_directory, teardown_index_directory);
   tcase_add_test(tc_directory, test_index_directory);
   tcase_add_test(tc_directory, test_index_directory_maxdepth_1);
   tcase_add_test(tc_directory, test_index_directory_maxdepth_2);
   tcase_add_test(tc_directory, test_index_directory_default_ignore);
   tcase_add_test(tc_directory, test_index_directory_ignore_others);

   TCase *tc_applications = tcase_create("catalog_index_applications");
   suite_add_tcase(s, tc_applications);
   tcase_add_checked_fixture(tc_applications, setup_index_applications, teardown_index_applications);
   tcase_add_test(tc_applications, test_index_applications);
   tcase_add_test(tc_applications, test_index_applications_depth);
   tcase_add_test(tc_applications, test_index_applications_skip_terminal);
   tcase_add_test(tc_applications, test_index_applications_skip_withargs);

   TCase *tc_bookmarks = tcase_create("catalog_index_bookmarks");
   suite_add_tcase(s, tc_bookmarks);
   tcase_add_checked_fixture(tc_bookmarks, setup_index_bookmarks, teardown_index_bookmarks);
   tcase_add_test(tc_bookmarks, test_index_bookmarks);
   tcase_add_test(tc_bookmarks, test_index_bookmarks_escape);

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

