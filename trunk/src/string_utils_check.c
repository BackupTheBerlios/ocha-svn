#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include "string_utils.h"

/* ------------------------- prototypes */
static Suite *string_utils_check_suite(void);
static void assertStrippedGStringIs(const char *str, const char *goal);
static void assertEQ(const char *a, const char *b);
static void assertNEQ(const char *a, const char *b);

/* ------------------------- test case */
static void setup()
{
}

static void teardown()
{
}

START_TEST(test_strstrip_on_gstring)
{
        assertStrippedGStringIs("abc", "abc");
        assertStrippedGStringIs(" abc", "abc");
        assertStrippedGStringIs("  \t abc", "abc");
        assertStrippedGStringIs("abc ", "abc");
        assertStrippedGStringIs("abc \t ", "abc");
        assertStrippedGStringIs(" \t abc \t ", "abc");

        assertStrippedGStringIs("", "");
        assertStrippedGStringIs("a b c", "a b c");
}
END_TEST

START_TEST(test_string_equals_ignore_space)
{
        assertEQ("abc", "abc");
        assertNEQ("", "h");
        assertNEQ("abd", "abc");
        assertNEQ("abc", "");
        assertNEQ("", "abc");

        assertEQ(" abc", "abc");
        assertEQ("  \t  abc", "abc");
        assertEQ("abc ", "abc");
        assertEQ("abc  \t ", "abc");
        assertEQ("  \t abc  \t ", "abc");
        assertEQ("abc", "  \t abc  \t ");
}
END_TEST


/* ------------------------- test suite */
static Suite *string_utils_check_suite(void)
{
        Suite *s = suite_create("string_utils");
        TCase *tc_core = tcase_create("string_utils_core");

        suite_add_tcase(s, tc_core);
        tcase_add_checked_fixture(tc_core, setup, teardown);
        tcase_add_test(tc_core, test_strstrip_on_gstring);
        tcase_add_test(tc_core, test_string_equals_ignore_space);

        return s;
}

int main(void)
{
        int nf;
        Suite *s = string_utils_check_suite ();
        SRunner *sr = srunner_create (s);
        srunner_run_all (sr, CK_NORMAL);
        nf = srunner_ntests_failed (sr);
        srunner_free (sr);
        suite_free (s);
        return (nf == 0) ? 0:10;
}

/* ------------------------- static functions */

static void assertStrippedGStringIs(const char *str, const char *goal)
{
        GString *gstr = g_string_new(str);
        strstrip_on_gstring(gstr);
        if(strcmp(gstr->str, goal)!=0) {
                fail(g_strdup_printf("stripped '%s' expected '%s', but got '%s'",
                                     str,
                                     goal,
                                     gstr->str));
        }
        g_string_free(gstr, TRUE/*with content*/);
}

static void assertEQ(const char *a, const char *b)
{
        if(!string_equals_ignore_spaces(a, b))
        {
                fail(g_strdup_printf("should be equal: '%s', '%s'",
                                     a, b));
        }
}

static void assertNEQ(const char *a, const char *b)
{
        if(string_equals_ignore_spaces(a, b))
        {
                fail(g_strdup_printf("should be different: '%s', '%s'",
                                     a, b));
        }
}
