#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include <locale.h>
#include <string.h>
#include <glib.h>
#include "query.h"

/** I've used junit for too long... */
#define assertTrue(msg, expr) fail_unless(expr, msg)

static const char *utf8_ayto = "\xce\xb1\xcf\x85\xcf\x84\xcf\x8c";
static const char *utf8_AYTO = "\xce\x91\xce\xa5\xce\xa4\xce\x8c";
static const char *utf8_AYTO_EINAI = "\xce\x91\xce\xa5\xce\xa4\xce\x8c \xce\x95\xce\x8a\xce\x9d\xce\x91\xce\x99";
static const char *utf8_Ti_einai_ayto = "\xce\xa4\xce\xb9 \xce\xb5\xce\xaf\xce\xbd\xce\xb1\xce\xb9 \xce\xb1\xcf\x85\xcf\x84\xcf\x8c";
static const char *utf8_TI_EINAI_AYTO = "\xce\xa4\xce\x99 \xce\x95\xce\x8a\xce\x9d\xce\x91\xce\x99 \xce\x91\xce\xa5\xce\xa4\xce\x8c";
static const char *utf8_ete = "\xc3\xa9t\xc3\xa9";
static const char *utf8_ETE_accents = "\xc3\x89T\xc3\x89";
static const char *utf8_ETE = "ETE";
static const char *utf8_Cet_ete = "Cet \xc3\xa9t\xc3\xa9";
static const char *utf8_CET_ETE_accents = "CET \xc3\x89T\xc3\x89";
static const char *utf8_CET_ETE_noaccents = "CET ETE";

/* ------------------------- prototypes */
/**
 * query_highlight, defined in query.c,
 * which should not be used anywhere else.
 */
extern gboolean query_highlight(const char *query, const char *name, char *highlight);
static void displayable_highlight(char *highlight, int len);
static void assertHighlightIs(const char *query, const char *name, const char *pattern);

/* ------------------------- test cases */
static void setup()
{
        /* make sure the locale is always the same,
         * even if it's set to something more meaningful
         * in the system, I just care about consistency
         */
        setlocale(LC_ALL, "C");
}

static void teardown()
{}

START_TEST(test_ismatch_exact)
{
        printf("--test_ismatch_exact\n");
        assertTrue("baaaa==baaaa",
                   query_ismatch("baaaa", "baaaa"));
        assertTrue("bobo!=baaaa",
                   !query_ismatch("bobo", "baaaa"));
        assertTrue("a b c==a b c",
                   query_ismatch("a b c", "a b c"));
}
END_TEST

START_TEST(test_ismatch_empty_match_nothing)
{
        printf("--test_ismatch_empty_match_nothing\n");
        assertTrue("ba ba and ''",
                   !query_ismatch("", "ba ba"));
        assertTrue("'' and ''",
                   !query_ismatch("", ""));
}
END_TEST

START_TEST(test_ismatch_substring)
{
        printf("--test_ismatch_substring\n");
        assertTrue("baaaa match 'aa'",
                   query_ismatch("aa", "baaaa"));
        assertTrue("aa does not match 'baaaa'",
                   !query_ismatch("baaaa", "aa"));
        assertTrue("baaaa match 'ba'",
                   query_ismatch("ba", "baaaa"));
        assertTrue("baaaax match 'aaax'",
                   query_ismatch("aaax", "baaaax"));
}
END_TEST

START_TEST(test_ismatch_multiple_substring)
{
        printf("--test_ismatch_multiple_substring\n");
        assertTrue("bottom match 'bo tom'",
                   query_ismatch("bo tom", "bottom"));
        assertTrue("bottom match 'tom bo'",
                   query_ismatch("tom bo", "bottom"));
        assertTrue("bottom match 'b t m'",
                   query_ismatch("b t m", "bottom"));
        assertTrue("botto does not match 'b t m'",
                   !query_ismatch("b t m", "botto"));
        assertTrue("botto match 'b t'",
                   query_ismatch("b t", "botto"));
        assertTrue("bobo does not match 'bo t'",
                   !query_ismatch("b t", "bobo"));
        assertTrue("bobo match 'bo'",
                   query_ismatch("bo", "bobo"));
}
END_TEST

START_TEST(test_ismatch_case_insensitive)
{
        printf("--test_ismatch_case_insensitive\n");
        assertTrue("heLLo and HELLo",
                   query_ismatch("heLLo", "HELLo"));
        assertTrue("HELLO and hello",
                   query_ismatch("HELLO", "hello"));
        assertTrue("hello and HELLO",
                   query_ismatch("hello", "HELLO"));
}
END_TEST


START_TEST(test_ismatch_utf8)
{
        printf("--test_ismatch_utf8\n");
        assertTrue("Cet 'et'e and Cet 'et'e (fr)",
                   query_ismatch(utf8_Cet_ete, utf8_Cet_ete));
        assertTrue("'et'e and Cet 'et'e (fr)",
                   query_ismatch(utf8_ete, utf8_Cet_ete));

        assertTrue("Ti e'inai ayto and Ti e'inai ayt'o (gr)",
                   query_ismatch(utf8_Ti_einai_ayto, utf8_Ti_einai_ayto));
        assertTrue("Ti e'inai ayt'o and TI E'INAI AYT'O (gr)",
                   query_ismatch(utf8_Ti_einai_ayto, utf8_TI_EINAI_AYTO));
        assertTrue("AYTO E'INAI and Ti e'inai ayt'o (gr)",
                   query_ismatch(utf8_AYTO_EINAI, utf8_Ti_einai_ayto));

        assertTrue("Cet 'et'e and CET 'ET'E (fr)",
                   query_ismatch(utf8_Cet_ete, utf8_CET_ETE_accents));

        /* There's no way it's going to work, yet I think it should:
         * no matter what the Academie Francaise says, nobody writes
         * capital letters with accents in french.
         *
         * assertTrue("Cet 'et'e and CET ETE (fr)",
         *          query_ismatch(utf8_Cet_ete, utf8_CET_ETE_noaccents));
         */
}
END_TEST


START_TEST(test_result_ismatch)
{
        printf("--test_result_ismatch\n");
        struct result result;
        memset(&result, 0, sizeof(result));
        result.name="bottom";
        /* I don't set anything else, because it must not
         * be accessed.
         */
        assertTrue("'bobo' and result(bottom)",
                   !query_result_ismatch("bobo", &result));
        assertTrue("'bottom' and result(bottom)",
                   query_result_ismatch("bottom", &result));
        assertTrue("'to bo' and result(bottom)",
                   query_result_ismatch("to bo", &result));
}
END_TEST

START_TEST(test_highlight_exact)
{
        printf("--test_highlight_exact\n");
        assertHighlightIs("bottom",
                          "bobo",
                          "0000");
        assertHighlightIs("bottom",
                          "bottom",
                          "111111");
}
END_TEST

START_TEST(test_highlight_substring)
{
        printf("--test_highlight_substring\n");
        assertHighlightIs("bottom",
                          "soggy bottom boys",
                          "00000 111111 0000");
        assertHighlightIs("sogg",
                          "soggy bottom boys",
                          "11110 000000 0000");
}
END_TEST

START_TEST(test_highlight_multiple_string)
{
        printf("--test_highlight_multiple_string\n");
        assertHighlightIs("bottom soggy",
                          "soggy bottom boys",
                          "11111 111111 0000");
        assertHighlightIs("ogg boy",
                          "soggy bottom boys",
                          "01110 000000 1110");
}
END_TEST

START_TEST(test_highlight_overlapping)
{
        printf("--test_highlight_overlapping\n");
        assertHighlightIs("ogg sog",
                          "soggy bottom boys",
                          "11110 000000 0000");
}
END_TEST


START_TEST(test_highlight_case_insensitive)
{
        printf("--test_highlight_case_insensitive\n");
        assertHighlightIs("soggy",
                          "Soggy Bottom Boys",
                          "11111 000000 0000");
        assertHighlightIs("BOTTOM",
                          "Soggy Bottom Boys",
                          "00000 111111 0000");
}
END_TEST

START_TEST(test_highlight_utf8)
{
        printf("--test_highlight_utf8\n");
        assertHighlightIs(utf8_AYTO,
                          utf8_TI_EINAI_AYTO,
                          "0000 0000000000 11111111");
        assertHighlightIs(utf8_ayto,
                          utf8_TI_EINAI_AYTO,
                          "0000 0000000000 11111111");
        assertHighlightIs("t",
                          utf8_CET_ETE_accents,
                          "001 00000");
        assertHighlightIs(utf8_ete,
                          utf8_CET_ETE_accents,
                          "000 11111");
}
END_TEST

START_TEST(test_pango_highlight)
{
        printf("--test_pango_highlight\n");
        assertTrue("exact",
                   strcmp("<toto>hello</toto>",
                          query_pango_highlight("hello", "hello", "<toto>", "</toto>"))==0);
        assertTrue("substring",
                   strcmp("he<toto>ll</toto>o",
                          query_pango_highlight("ll", "hello", "<toto>", "</toto>"))==0);
        assertTrue("multiple substring",
                   strcmp("hel[lo], [wor]ld",
                          query_pango_highlight("lo wor", "hello, world", "[", "]"))==0);
        assertTrue("multiple overlapping substring",
                   strcmp("hel[lowor]ld",
                          query_pango_highlight("low wor", "helloworld", "[", "]"))==0);
        assertTrue("escaped substring",
                   strcmp("&lt;he[&lt;a&gt;]toto&gt;",
                          query_pango_highlight("<a>", "<he<a>toto>", "[", "]"))==0);

}
END_TEST

/* ------------------------- test suite */

Suite *query_check_suite(void)
{
        Suite *s = suite_create("query");
        TCase *tc_core = tcase_create("query_core");

        suite_add_tcase(s, tc_core);
        tcase_add_checked_fixture(tc_core, setup, teardown);

        tcase_add_test(tc_core, test_ismatch_exact);
        tcase_add_test(tc_core, test_ismatch_empty_match_nothing);
        tcase_add_test(tc_core, test_ismatch_substring);
        tcase_add_test(tc_core, test_ismatch_multiple_substring);
        tcase_add_test(tc_core, test_ismatch_case_insensitive);
        tcase_add_test(tc_core, test_ismatch_utf8);

        tcase_add_test(tc_core, test_result_ismatch);

        tcase_add_test(tc_core, test_highlight_exact);
        tcase_add_test(tc_core, test_highlight_substring);
        tcase_add_test(tc_core, test_highlight_multiple_string);
        tcase_add_test(tc_core, test_highlight_overlapping);
        tcase_add_test(tc_core, test_highlight_case_insensitive);
        tcase_add_test(tc_core, test_highlight_utf8);

        tcase_add_test(tc_core, test_pango_highlight);

        return s;
}

int main(void)
{
        int nf;
        Suite *s = query_check_suite ();
        SRunner *sr = srunner_create (s);
        srunner_run_all (sr, CK_NORMAL);
        nf = srunner_ntests_failed (sr);
        srunner_free (sr);
        suite_free (s);
        return (nf == 0) ? 0:10;
}

/* ------------------------- private functions */
static void displayable_highlight(char *highlight, int len)
{
        for(int i=0; i<len; i++) {
                if(highlight[i]=='\0')
                        highlight[i]=' ';
        }
}
static void assertHighlightIs(const char *query, const char *name, const char *pattern)
{
        const int namelen=strlen(name);
        char goal[namelen+1];
        strcmp(name, goal);
        gboolean expected_retval=FALSE;
        for(int i=0; i<namelen; i++) {
                if(pattern[i]=='1') {
                        expected_retval=TRUE;
                        goal[i]=name[i];
                } else
                        goal[i]='\0';
        }
        goal[namelen]='\0';
        char dest[namelen+2];
        memset(dest, 'w', namelen+1);
        dest[namelen+1]='\0';
        gboolean retval = query_highlight(query, name, dest);
        fail_unless(expected_retval==retval,
                    g_strdup_printf("assertHighlightIs(%s, %s, ...): returned %s when it should have returned %s\n",
                                    query,
                                    name,
                                    retval ? "TRUE":"FALSE",
                                    expected_retval ? "TRUE":"FALSE"));
        if(retval) {
                if(memcmp(dest, goal, namelen+1)==0)
                        return;

                displayable_highlight(goal, namelen);
                displayable_highlight(dest, namelen);
                fail(g_strdup_printf("assertHighlightIs(%s, %s, result): result was:\n '%s'\nwhen it should have been:\n '%s'\n",
                                     query, name, dest, goal));
        }
}

