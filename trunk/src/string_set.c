#include "string_set.h"

/** \file Implementation of the string set API defined in string_set.h
 *
 */

struct string_set
{
        GHashTable *table;
};

#define MARKER GINT_TO_POINTER(9)

/* ------------------------- prototypes: static functions */

/* ------------------------- definitions */

/* ------------------------- public functions */
struct string_set *string_set_new(void)
{
        struct string_set *retval;

        retval = g_new(struct string_set, 1);
        retval->table = g_hash_table_new_full(g_str_hash,
                                              g_str_equal,
                                              g_free/*key destroy*/,
                                              NULL/*value destroy*/);
        return retval;
}

void string_set_free(struct string_set *set)
{
        g_return_if_fail(set);
        g_return_if_fail(set->table);

        g_hash_table_destroy(set->table);
        g_free(set);
}

void string_set_add(struct string_set *set, const char *str)
{
        g_return_if_fail(set);
        g_return_if_fail(set->table);
        g_return_if_fail(str);

        if(g_hash_table_lookup(set->table, str)!=MARKER) {
                g_hash_table_insert(set->table,
                                    g_strdup(str),
                                    MARKER);
        }
}

void string_set_remove(struct string_set *set, const char *str)
{
        g_return_if_fail(set);
        g_return_if_fail(set->table);
        g_return_if_fail(str);
        g_hash_table_remove(set->table, str);
}

gboolean string_set_contains(struct string_set *set, const char *str)
{
        g_return_val_if_fail(set, FALSE);
        g_return_val_if_fail(set->table, FALSE);
        g_return_val_if_fail(str, FALSE);

        return g_hash_table_lookup(set->table, str)==MARKER;
}


/* ------------------------- static functions */

