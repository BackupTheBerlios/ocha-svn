#include "query.h"
#include <glib.h>
#include <string.h>

static char *prepare(const char *str)
{
   const char *str_norm = g_utf8_normalize(str, -1, G_NORMALIZE_ALL_COMPOSE);
   char *retval = g_utf8_casefold(str_norm, -1);
   g_free((void *)str_norm);
   return retval;
}
bool query_ismatch(const char *query, const char *name)
{
   g_return_val_if_fail(query!=NULL, false);
   g_return_val_if_fail(name!=NULL, false);

   if(query[0]=='\0' || name[0]=='\0')
      return false;

   char *query_prepared = prepare(query);
   const char *name_prepared = prepare(name);

   bool retval;
   if(strcmp(name_prepared, query_prepared)==0)
      retval=true;
   else
      {
         retval=true;
         char *current=query_prepared;
         do
            {
               char *space = strchr(current, ' ');
               if(space!=NULL)
                  *space='\0';

               const char *found = strstr(name_prepared, current);
               if(found==NULL)
                  {
                     retval=false;
                     break;
                  }
               if(space)
                  current=space+1;
               else
                  break;
            }
         while(true);
      }

   g_free((void *)query_prepared);
   g_free((void *)name_prepared);
   return retval;
}

bool query_result_ismatch(const char *query, const struct result *result)
{
   g_return_val_if_fail(query!=NULL, false);
   g_return_val_if_fail(result!=NULL, false);

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
 * @return true if there was any match
 */
bool query_highlight(const char *query, const char *name, char *highlight)
{
   g_return_val_if_fail(query!=NULL, false);
   g_return_val_if_fail(name!=NULL, false);
   g_return_val_if_fail(highlight!=NULL, false);

   g_return_val_if_fail(query!=NULL, false);
   g_return_val_if_fail(name!=NULL, false);
   char *query_prepared = g_utf8_casefold(query, -1);
   const char *name_prepared = g_utf8_casefold(name, -1);

   memset(highlight, '\0', strlen(name)+1);
   bool retval;
   if(strcmp(name_prepared, query_prepared)==0)
      {
         strcpy(highlight, name);
         retval=true;
      }
   else
      {
         retval=true;
         char *current=query_prepared;


         do
            {
               char *space = strchr(current, ' ');
               if(space!=NULL)
                  *space='\0';

               const char *found = strstr(name_prepared, current);
               if(found==NULL)
                  {
                     retval=false;
                     break;
                  }
               else
                  {
                     int start_index = g_utf8_pointer_to_offset(name_prepared, found);
                     int end_index = start_index+g_utf8_strlen(current, -1);
                     const char *start_ptr = g_utf8_offset_to_pointer(name, start_index);
                     const char *end_ptr = g_utf8_offset_to_pointer(name, end_index);

                     char *highlight_ptr=highlight+(start_ptr-name);
                     for(const char *cur=start_ptr; cur<end_ptr; cur++, highlight_ptr++)
                           *highlight_ptr=*cur;
                  }
               if(space)
                  current=space+1;
               else
                  break;
            }
         while(true);
      }

   g_free((void *)query_prepared);
   g_free((void *)name_prepared);
   return retval;

   return false;
}


const char *query_pango_highlight(const char *query, const char *str, const char *highlight_on, const char *highlight_off)
{
   const char *norm_query = g_utf8_normalize(query, -1, G_NORMALIZE_ALL_COMPOSE);
   char *norm_str = g_utf8_normalize(str, -1, G_NORMALIZE_ALL_COMPOSE);
   int norm_str_len = strlen(norm_str);
   char highlight[norm_str_len+1];

   if(query_highlight(query, str, highlight))
      {
         GString *retval = g_string_new("");
         bool last_highlighted=false;
         char buffer[norm_str_len+1];
         int bufpos=0;
         for(int i=0; i<norm_str_len; i++)
            {
               bool current_highlighted = highlight[i]!='\0';
               if(current_highlighted!=last_highlighted)
                  {
                     if(bufpos>0)
                        {
                           const char *escaped = g_markup_escape_text(buffer, bufpos);
                           g_string_append(retval, escaped);
                           g_free((void*)escaped);
                           bufpos=0;
                        }

                     g_string_append(retval, current_highlighted ? highlight_on:highlight_off);
                     last_highlighted=current_highlighted;
                  }
               buffer[bufpos]=norm_str[i];
               bufpos++;
            }
         if(bufpos>0)
            {
               const char *escaped = g_markup_escape_text(buffer, bufpos);
               g_string_append(retval, escaped);
               g_free((void *)escaped);
            }
         if(last_highlighted)
            g_string_append(retval, highlight_off);
         g_free((void *)norm_query);
         g_free((void *)norm_str);
         char *retval_content = retval->str;
         g_string_free(retval, false/*don't free content*/);
         return retval_content;
      }
   else
      {
         g_free((void *)norm_query);
         return norm_str;
      }
}
