#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>

/* \file file that's run instead of real test cases
 * to warn caller about the test cases having
 * been deactivated.
 */

int main(void)
{
        printf("** ERROR: Test cases have been disabled\n"
               "**\n"
               "** Test cases require the library libcheck\n"
               "** available from http://check.sourceforge.net/\n"
               "** Since configure could not find this library\n"
               "** it decided to de-activate the test cases.\n"
               "**\n"
               "** Download libcheck, install it and re-run\n"
               "** configure to enable the test cases.\n");
        return 99;
}


