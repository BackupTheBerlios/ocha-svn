#include "parse_uri_list_next.h"
#include <stdio.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs-utils.h>

/** \file implement the API defined in parse_uri_list_next.h
 *
 */

/* ------------------------- prototypes: static functions */
static gchar *skip_crlf(gchar *ptr, gchar *end);
static inline gboolean is_crlf(gchar c) {
        return c=='\n' || c=='\r';
}
static void fix_file_uri(GString *gstr);

/* ------------------------- public functions */

gboolean parse_uri_list_next(gchar **data_ptr, gint *length_ptr, GString *dest)
{
        gchar *data;
        gchar *ptr;
        gchar *data_end;
        gint length;

        g_return_val_if_fail(data_ptr, FALSE);
        g_return_val_if_fail(*data_ptr, FALSE);
        g_return_val_if_fail(length_ptr, FALSE);
        g_return_val_if_fail(*length_ptr>=0, FALSE);
        g_return_val_if_fail(dest, FALSE);

        length = *length_ptr;
        data = *data_ptr;

        if(length<=0) {
                return FALSE;
        }

        data_end = data+length;

        data=skip_crlf(data, data_end);

        for(ptr=data; ptr<data_end; ptr++) {
                if(is_crlf(*ptr)) {
                        break;
                }
        }

        if(ptr==data) {
                *length_ptr=0;
                *data_ptr=data_end;
                return FALSE;
        }
        g_string_set_size(dest, 0);
        g_string_append_len(dest, data, ptr-data);
        fix_file_uri(dest);

        ptr=skip_crlf(ptr, data_end);
        while(ptr<data_end && *ptr=='\0') {
                ptr++;
        }

        *data_ptr=ptr;
        *length_ptr=data_end-ptr;

        return TRUE;
}


/* ------------------------- static functions */
static gchar *skip_crlf(gchar *ptr, gchar *end)
{
        while(ptr<end && is_crlf(*ptr)) {
                ptr++;
        }
        return ptr;
}

static void fix_file_uri(GString *gstr)
{
        char *tmp;
        g_return_if_fail(gstr);
        g_return_if_fail(gstr->len>0);


        if(*gstr->str=='/') {
                tmp = gnome_vfs_get_uri_from_local_path(gstr->str);
                g_string_assign(gstr, tmp);
                g_free(tmp);
                return;
        }

        if(g_str_has_prefix(gstr->str, "file:")) {
                gchar *str = gstr->str;
                gchar *ptr;
                int slashes=0;
                int i;
                int prefix_len = strlen("file:");

                str+=prefix_len;
                for(ptr=str; *ptr=='/'; ptr++, slashes++);
                for(i=slashes; i<3; i++) {
                        g_string_insert_c(gstr, prefix_len, '/');
                }
        }
}
