#include "string_utils.h"

/** \file implementation of functions defined in string_utils.h */

#define IS_SPACE(c) ((c)==' ' || (c)=='\t')

/* ------------------------- prototypes */
static const char *skip_spaces(const char *str);
static const char *last_nonspace(const char *str);

/* ------------------------- public functions */
void strstrip_on_gstring(GString *gstr)
{
        const char *start;
        const char *stop;
        const char *last;
        const char *first;

        first = gstr->str;
        start = skip_spaces(first);
        if(start>gstr->str) {
                g_string_erase(gstr, 0, start-first);
        }

        last = &gstr->str[gstr->len-1];
        stop = last_nonspace(first);
        if(stop<last) {
                g_string_truncate(gstr, gstr->len-(last-stop));
        }
}

gboolean string_equals_ignore_spaces(const char *a, const char *b)
{
        const char *a_start;
        const char *a_end;
        const char *b_start;
        const char *b_end;

        g_return_val_if_fail(a, FALSE);
        g_return_val_if_fail(b, FALSE);

        a_start=skip_spaces(a);
        a_end=last_nonspace(a);
        b_start=skip_spaces(b);
        b_end=last_nonspace(b);

        if( (a_end-a_start)!=(b_end-b_start) ) {
                return FALSE;
        }
        if(*a=='\0') {
                return *b=='\0';
        } else if(*b=='\0') {
                return FALSE;
        }

        return strncmp(a_start, b_start, (a_end-a_start)+1)==0;
}

/* ------------------------- static functions */
static const char *skip_spaces(const char *str)
{
        while( IS_SPACE(*str) ) {
                str++;
        }
        return str;
}
static const char *last_nonspace(const char *str)
{
        const char *last;

        if(*str=='\0') {
                return str;
        }

        last = &str[strlen(str)-1];
        while(last>=str && IS_SPACE(*last)) {
                last--;
        }
        return last;
}
