#include "query.h"
#include <glib.h>
#include <string.h>

/* ------------------------- prototypes */
static char *prepare(const char *str);

/* query_highlight: not static, because used from test case, but not public either... */
gboolean query_highlight(const char *query, const char *name, char *highlight);

/* ------------------------- public functions */
gboolean query_ismatch(const char *query, const char *name)
{
        char *query_prepared;
        const char *name_prepared;
        gboolean retval;

        g_return_val_if_fail(query!=NULL, FALSE);
        g_return_val_if_fail(name!=NULL, FALSE);

        if(query[0]=='\0' || name[0]=='\0')
                return FALSE;

        query_prepared =  prepare(query);
        name_prepared =  prepare(name);

        if(strcmp(name_prepared, query_prepared)==0)
                retval=TRUE;
        else {
                char *current=query_prepared;
                retval=TRUE;
                do {
                        const char *found;
                        char *space;

                        space =  strchr(current, ' ');
                        if(space!=NULL)
                                *space='\0';

                        found =  strstr(name_prepared, current);
                        if(found==NULL) {
                                retval=FALSE;
                                break;
                        }
                        if(space)
                                current=space+1;
                        else
                                break;
                } while(TRUE);
        }

        g_free((void *)query_prepared);
        g_free((void *)name_prepared);
        return retval;
}

gboolean query_result_ismatch(const char *query, const struct result *result)
{
        g_return_val_if_fail(query!=NULL, FALSE);
        g_return_val_if_fail(result!=NULL, FALSE);

        return query_ismatch(query, result->name);
}

/**
 * Highlight the parts of a name/path that match
 * the query.
 *
 * The function is given two string of equal size. For
 * each byte where the first string (name) matches the
 * query, the second string (highlight) will be equal
 * to name. Everywhere else, it'll be 0.
 *
 * Even though this function provides a byte-matching
 * result, it actually works in UTF-8. You should only
 * feed it UTF-8 strings.
 *
 * @param query (normalized UTF8 with G_NORMALIZE_ALL_COMPOSE)
 * @param name name or path (normalized UTF8 with G_NORMALIZE_ALL_COMPOSE)
 * @param highlight a string of at least the same length as name
 * @return TRUE if there was any match
 */
gboolean query_highlight(const char *query, const char *name, char *highlight)
{
        char *query_prepared;
        const char *name_prepared;
        gboolean retval;

        g_return_val_if_fail(query!=NULL, FALSE);
        g_return_val_if_fail(name!=NULL, FALSE);
        g_return_val_if_fail(highlight!=NULL, FALSE);

        g_return_val_if_fail(query!=NULL, FALSE);
        g_return_val_if_fail(name!=NULL, FALSE);

        query_prepared =  g_utf8_casefold(query, -1);
        name_prepared =  g_utf8_casefold(name, -1);

        memset(highlight, '\0', strlen(name)+1);

        if(strcmp(name_prepared, query_prepared)==0) {
                strcpy(highlight, name);
                retval=TRUE;
        } else {
                char *current=query_prepared;
                retval=TRUE;

                do {
                        const char *found;
                        char *space;
                        space =  strchr(current, ' ');
                        if(space!=NULL)
                                *space='\0';

                        found =  strstr(name_prepared, current);
                        if(found==NULL) {
                                retval=FALSE;
                                break;
                        } else {
                                const char *cur;
                                int start_index;
                                int end_index;
                                const char *start_ptr;
                                const char *end_ptr;
                                char *highlight_ptr;

                                start_index =  g_utf8_pointer_to_offset(name_prepared,
                                                                        found);
                                end_index =  start_index+g_utf8_strlen(current,
                                                                       -1);
                                start_ptr =  g_utf8_offset_to_pointer(name,
                                                                      start_index);
                                end_ptr =  g_utf8_offset_to_pointer(name,
                                                                    end_index);
                                highlight_ptr = highlight+(start_ptr-name);

                                for(cur=start_ptr; cur<end_ptr; cur++, highlight_ptr++) {
                                        *highlight_ptr=*cur;
                                }
                        }
                        if(space) {
                                current=space+1;
                        } else {
                                break;
                        }
                } while(TRUE);
        }

        g_free((void *)query_prepared);
        g_free((void *)name_prepared);
        return retval;

        return FALSE;
}


const char *query_pango_highlight(const char *query,
                                  const char *str,
                                  const char *highlight_on,
                                  const char *highlight_off)
{
        char *norm_query;
        char *norm_str;
        int norm_str_len;
        char *highlight;
        char *retval;

        norm_query =  g_utf8_normalize(query, -1, G_NORMALIZE_ALL_COMPOSE);
        norm_str =  g_utf8_normalize(str, -1, G_NORMALIZE_ALL_COMPOSE);
        norm_str_len =  strlen(norm_str);
        highlight = g_malloc(norm_str_len+1);

        if(query_highlight(query, str, highlight)) {
                GString *gstr = g_string_new("");
                gboolean last_highlighted=FALSE;
                char *buffer = g_malloc(norm_str_len+1);
                int bufpos=0;
                int i;

                for(i=0; i<norm_str_len; i++) {
                        gboolean current_highlighted = highlight[i]!='\0';
                        if(current_highlighted!=last_highlighted) {
                                if(bufpos>0) {
                                        const char *escaped;
                                        escaped = g_markup_escape_text(buffer, bufpos);
                                        g_string_append(gstr, escaped);
                                        g_free((void*)escaped);
                                        bufpos=0;
                                }

                                g_string_append(gstr,
                                                current_highlighted
                                                ? highlight_on
                                                : highlight_off);
                                last_highlighted=current_highlighted;
                        }
                        buffer[bufpos]=norm_str[i];
                        bufpos++;
                }
                if(bufpos>0) {
                        const char *escaped = g_markup_escape_text(buffer, bufpos);
                        g_string_append(gstr, escaped);
                        g_free((void *)escaped);
                }
                if(last_highlighted) {
                        g_string_append(gstr, highlight_off);
                }

                g_free(buffer);
                g_free(norm_str);

                retval = gstr->str;
                g_string_free(gstr, FALSE/*don't free content*/);
        } else {
                retval=norm_str;
        }
        g_free(norm_query);
        g_free(highlight);
        return retval;
}

/* ------------------------- static functions */

static char *prepare(const char *str)
{
        const char *str_norm = g_utf8_normalize(str,
                                                -1,
                                                G_NORMALIZE_ALL_COMPOSE);
        char *retval = g_utf8_casefold(str_norm, -1);
        g_free((void *)str_norm);
        return retval;
}
