/** \file Implementation of the API defined in ocha_gconf.h */

#include "ocha_gconf.h"
#include <string.h>
#include <stdlib.h>

static GConfClient *client;

/* ------------------------- public function */

GConfClient *ocha_gconf_get_client()
{
    if(client!=NULL)
        return client;

    GConfClient *_client=gconf_client_get_default();
    gconf_client_set_error_handling(_client, GCONF_CLIENT_HANDLE_UNRETURNED);
    gconf_client_add_dir(_client, OCHA_GCONF_PREFIX, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
    client=_client;
    return client;
}

gboolean ocha_gconf_exists()
{
    ocha_gconf_get_client();
    return gconf_client_dir_exists(client, OCHA_GCONF_PREFIX, NULL/*err*/);
}

void ocha_gconf_get_sources(const char *type, int **ids_out, int *ids_len_out)
{
    g_return_if_fail(type);
    g_return_if_fail(ids_out);
    g_return_if_fail(ids_len_out);

    ocha_gconf_get_client();

    char buffer[strlen(OCHA_GCONF_INDEXERS)+1+strlen(type)+1];
    strcpy(buffer, OCHA_GCONF_INDEXERS);
    strcat(buffer, "/");
    strcat(buffer, type);


    GSList *retval = gconf_client_all_dirs(client, buffer, NULL/*err*/);
    int len = g_slist_length(retval);
    *ids_len_out = len;
    if(len==0)
        *ids_out=NULL;
    else
    {
        int *ids = g_new(int, len);
        int index=0;
        *ids_out=ids;
        for(GSList *current=retval; current; current=current->next, index++)
        {
            char *entry = (char *)current->data;
            char *lastpart = strrchr(entry, '/');
            if(lastpart)
                lastpart++;
            else
                lastpart=entry;
            ids[index]=atoi(lastpart);
            g_free(entry);
        }
    }
    g_slist_free(retval);
}

char *ocha_gconf_get_source_attribute(const char *type, int source_id, const char *attribute)
{
    g_return_val_if_fail(type, NULL);
    g_return_val_if_fail(attribute, NULL);

    ocha_gconf_get_client();

    char *key = ocha_gconf_get_source_attribute_key(type,
                                                    source_id,
                                                    attribute);

    char *retval = gconf_client_get_string(client,
                                           key,
                                           NULL/*err*/);
    g_free(key);
    return retval;
}

gboolean ocha_gconf_set_source_attribute(const char *type, int source_id, const char *attribute, const char *value)
{
    g_return_val_if_fail(type, FALSE);
    g_return_val_if_fail(attribute, FALSE);

    ocha_gconf_get_client();

    char *key = ocha_gconf_get_source_attribute_key(type,
                                                    source_id,
                                                    attribute);

    gboolean retval;
    if(value==NULL)
        retval=gconf_client_unset(client,
                                  key,
                                  NULL/*err*/);
    else
        retval=gconf_client_set_string(client,
                                       key,
                                       value,
                                       NULL/*err*/);
    g_free(key);
    return retval;
}

gchar *ocha_gconf_get_source_key(const char *type, int id)
{
    g_return_val_if_fail(type, NULL);
    return g_strdup_printf("%s/%s/%d",
                           OCHA_GCONF_INDEXERS,
                           type,
                           id);
}

gchar *ocha_gconf_get_source_attribute_key(const char *type, int id, const char *attribute)
{
   g_return_val_if_fail(type, NULL);
   return g_strdup_printf("%s/%s/%d/%s",
                          OCHA_GCONF_INDEXERS,
                          type,
                          id,
                          attribute);
}
