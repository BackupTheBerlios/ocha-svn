#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include <string.h>
#include "parse_uri_list_next.h"

static GString *dest;

/* ------------------------- prototypes: static functions */

/* ------------------------- tests */

static void setup()
{
        dest = g_string_new("");
}

static void teardown()
{
        g_string_free(dest, TRUE/*free content*/);
}

START_TEST(test_empty_data)
{
        gchar *data = "";
        int len = 0;
        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        fail_unless(!parse_uri_list_next(&data, &len, dest),
                    "empty data");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_one_uri)
{
        gchar *data = "http://www.example.com/";
        int len = strlen(data);
        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "one uri");
        fail_unless(strcmp(dest->str, "http://www.example.com/")==0,
                    "expected http://www.example.com/");
        fail_unless(!parse_uri_list_next(&data, &len, dest),
                    "should be finished");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_two_uris)
{
        gchar *data = "http://www.example.com/\r\nhttp://www.example.com/toto?boo";
        gint len = strlen(data);
        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "1st uri");
        fail_unless(strcmp(dest->str, "http://www.example.com/")==0,
                    "expected http://www.example.com/");
        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "2nd uri");
        fail_unless(strcmp(dest->str, "http://www.example.com/toto?boo")==0,
                    "expected http://www.example.com/toto?boo");
        fail_unless(!parse_uri_list_next(&data, &len, dest),
                    "should be finished");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_separators)
{
        gchar *data = "http://a\nhttp://b\rhttp://c\r\n\n\rhttp://d\r\n\r\n";
        gint len = strlen(data);

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "1st uri");
        fail_unless(strcmp(dest->str, "http://a")==0,
                    "expected http://a");

        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "2nd uri");
        fail_unless(strcmp(dest->str, "http://b")==0,
                    "expected http://b");

        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "3rd uri");
        fail_unless(strcmp(dest->str, "http://c")==0,
                    "expected http://c");

        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "4th uri");
        fail_unless(strcmp(dest->str, "http://d")==0,
                    "expected http://d");

        fail_unless(!parse_uri_list_next(&data, &len, dest),
                    "should be finished");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_convert_path_to_uri)
{
        gchar *data = "/etc/init.d";
        int len = strlen(data);
        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "one uri");
        fail_unless(strcmp(dest->str, "file:///etc/init.d")==0,
                    "expected file:///etc/init.d");

        fail_unless(!parse_uri_list_next(&data, &len, dest),
                    "should be finished");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_fix_file_uri)
{
        int i;
        gchar *data = "file:/hello\r\nfile://hello\r\nfile:///hello";
        int len = strlen(data);

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        for(i=0; i<3; i++) {
                fail_unless(parse_uri_list_next(&data, &len, dest),
                            "expected uri");
                fail_unless(strcmp(dest->str, "file:///hello")==0,
                            "expected file:///hello");
        }

        fail_unless(!parse_uri_list_next(&data, &len, dest),
                    "should be finished");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_escape_file_uri)
{
        gchar *data = "/lost+found";
        int len = strlen(data);

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "expected uri");
        fail_unless(strcmp(dest->str, "file:///lost%2Bfound")==0,
                    "expected file:///lost%2Bfound");

        fail_unless(!parse_uri_list_next(&data, &len, dest),
                    "should be finished");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_ends_with_zero)
{
        gchar *data = "http://www.example.com/\r\n";
        int len = strlen(data)+1;
        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        fail_unless(parse_uri_list_next(&data, &len, dest),
                    "one uri");
        fail_unless(strcmp(dest->str, "http://www.example.com/")==0,
                    "expected http://www.example.com/");
        fail_unless(!parse_uri_list_next(&data, &len, dest),
                    "should be finished");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

/* ------------------------- test suite */

static Suite *parse_uri_list_next_check_suite(void)
{
        Suite *s = suite_create("parse_uri_list_next");
        TCase *tc_core = tcase_create("parse_uri_list_next_core");

        suite_add_tcase(s, tc_core);
        tcase_add_checked_fixture(tc_core, setup, teardown);
        tcase_add_test(tc_core, test_empty_data);
        tcase_add_test(tc_core, test_one_uri);
        tcase_add_test(tc_core, test_two_uris);
        tcase_add_test(tc_core, test_separators);
        tcase_add_test(tc_core, test_convert_path_to_uri);
        tcase_add_test(tc_core, test_fix_file_uri);
        tcase_add_test(tc_core, test_escape_file_uri);
        tcase_add_test(tc_core, test_ends_with_zero);

        return s;
}

int main(void)
{
        int nf;
        Suite *s = parse_uri_list_next_check_suite ();
        SRunner *sr = srunner_create (s);
        srunner_run_all (sr, CK_NORMAL);
        nf = srunner_ntests_failed (sr);
        srunner_free (sr);
        suite_free (s);
        return (nf == 0) ? 0:10;
}

/* ------------------------- static functions */

