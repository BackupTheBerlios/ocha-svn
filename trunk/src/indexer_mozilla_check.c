#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include "indexer_mozilla.h"
#include "mock_catalog.h"

#define TEMPDIR ".test"

static struct catalog *catalog;

static void deltree(const char *path);
static void touch(const char *path);
static void createfile(const char *path, const char *content);

static void setup()
{
   deltree(TEMPDIR);
   mkdir(TEMPDIR, 0700);

   catalog=mock_catalog_new();
}

static void teardown()
{
   deltree(TEMPDIR);
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

   int run_id=2;
   mock_catalog_expect_addcommand(catalog,
                                  "gnome-moz-remote",
                                  "gnome-moz-remote \"%f\"",
                                  run_id);
   mock_catalog_expect_addentry(catalog,
                                "http://slashdot.org/",
                                "slashdot",
                                "http://slashdot.org/",
                                run_id,
                                1);
   mock_catalog_expect_addentry(catalog,
                                "http://www.groklaw.net/",
                                "GROKLAW",
                                "http://www.groklaw.net/",
                                run_id,
                                2);
   mock_catalog_expect_addentry(catalog,
                                "http://texturizer.net/firefox/extensions/",
                                "Firefox Extensions",
                                "http://texturizer.net/firefox/extensions/",
                                run_id,
                                3);
   mock_catalog_expect_addentry(catalog,
                                "http://texturizer.net/firefox/themes/",
                                "Firefox Themes",
                                "http://texturizer.net/firefox/themes/",
                                run_id,
                                4);

   fail_unless(catalog_index_bookmarks(catalog,
                                       TEMPDIR "/bookmarks.html",
                                       NULL/*ignore errors*/),
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
                                  "gnome-moz-remote \"%f\"",
                                  run_id);
   mock_catalog_expect_addentry(catalog,
                                "http://texturizer.net/firefox/extensions/",
                                "<Firefox> add-ons & (Extensions)",
                                "http://texturizer.net/firefox/extensions/",
                                run_id,
                                3);

   fail_unless(catalog_index_bookmarks(catalog,
                                       TEMPDIR "/bookmarks.html",
                                       NULL/*ignore errors*/),
               "indexing failed");

   mock_catalog_assert_expectations_met(catalog);


   printf("---test_index_bookmarks_escape OK\n");
}
END_TEST


Suite *indexer_mozilla_check_suite(void)
{
   Suite *s = suite_create("indexer_mozilla");
   TCase *tc_core = tcase_create("indexer_mozilla_core");

   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_index_bookmarks);
   tcase_add_test(tc_core, test_index_bookmarks_escape);

   return s;
}

int main(void)
{
   int nf;
   Suite *s = indexer_mozilla_check_suite ();
   SRunner *sr = srunner_create (s);
   srunner_run_all (sr, CK_NORMAL);
   nf = srunner_ntests_failed (sr);
   srunner_free (sr);
   suite_free (s);
   return (nf == 0) ? 0:10;
}

