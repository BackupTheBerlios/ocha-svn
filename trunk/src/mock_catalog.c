#include "mock_catalog.h"
#include "ocha_gconf.h"
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <check.h>

/** \file Implementation of the API defined in mock_catalog.h and of some functions
 * from catalog.h
 */

/** Mock catalog structure */
struct catalog
{
        /** char* x addcommand_args, name -> expected calls */
        GHashTable *expected_addcommand;
        /** char* x addentry_args, path -> expected calls */
        GHashTable *expected_addentry;
        /** char* x char * */
        GHashTable *source_attrs;
};

struct expectation
{
        int expected;
        int found;
        const char *description;
};

struct addcommand_args
{
        struct expectation expect;
        const char *name;
        const char *execute;
        int id;
};

struct addentry_args
{
        struct expectation expect;
        const char *path;
        const char *name;
        const char *long_name;
        int command_id;
        int id;
};
static GHashTable *source_attrs;

/* ------------------------- prototypes */
static void expectation_init(struct expectation *ex, const char *description);
static gboolean expectation_check(struct expectation *expect);
static void expectation_call(struct expectation *expect);
static void expectations_met_cb(gpointer key, gpointer value, gpointer userdata);
static void init_source_attrs(void);

/* ------------------------- public functions: mock_catalog */
struct catalog *mock_catalog_new(void)
{
        struct catalog *retval = g_new(struct catalog, 1);
        retval->expected_addcommand=g_hash_table_new(g_str_hash, g_str_equal);
        retval->expected_addentry=g_hash_table_new(g_str_hash, g_str_equal);
        return retval;
}

void mock_catalog_expect_addcommand(struct catalog *catalog, const char *name, const char *execute, int id)
{
        struct addcommand_args *expect = g_new(struct addcommand_args, 1);
        expectation_init(&expect->expect,
                         g_strdup_printf("addcommand(catalog, '%s', '%s'):%d",
                                         name,
                                         execute,
                                         id));
        expect->name=g_strdup(name);
        expect->execute=g_strdup(execute);
        expect->id=id;
        g_hash_table_insert(catalog->expected_addcommand,
                            (gpointer)expect->name,
                            (gpointer)expect);
}

void mock_catalog_expect_addentry(struct catalog *catalog,
                                  const char *path,
                                  const char *name,
                                  const char *long_name,
                                  int command_id,
                                  int id)
{
        struct addentry_args *expect;

        fail_unless(path!=NULL, "no path");
        fail_unless(name!=NULL, "no name");
        fail_unless(long_name!=NULL, "no long_name");

        expect =  g_new(struct addentry_args, 1);
        expectation_init(&expect->expect,
                         g_strdup_printf("addentry(catalog, '%s', '%s', '%s', %d):%d",
                                         path,
                                         name,
                                         long_name,
                                         command_id,
                                         id));
        expect->path=g_strdup(path);
        expect->name=g_strdup(name);
        expect->long_name=g_strdup(long_name);
        expect->command_id=command_id;
        expect->id=id;
        g_hash_table_insert(catalog->expected_addentry,
                            (gpointer)expect->path,
                            (gpointer)expect);
}

void mock_catalog_assert_expectations_met(struct catalog *catalog)
{
        gboolean retval = TRUE;
        g_hash_table_foreach(catalog->expected_addcommand,
                             expectations_met_cb,
                             &retval);
        g_hash_table_foreach(catalog->expected_addentry,
                             expectations_met_cb,
                             &retval);
        fail_unless(retval,
                    "not all expectations were met (see stderr for details)");
}

/* ------------------------- public functions: catalog */
const char *catalog_error(struct catalog *catalog)
{
        return "mock error";
}
gboolean catalog_add_entry(struct catalog *catalog, int source_id, const char *launcher, const char *path, const char *name, const char *long_name, int *id_out)
{
        struct addentry_args *args =
                                        (struct addentry_args *)g_hash_table_lookup(catalog->expected_addentry,
                                                        path);

        fail_unless(long_name!=NULL, "no long_name");
        fail_unless(name!=NULL, "no display_name");
        fail_unless(path!=NULL, "no path");
        if(args==NULL)
{

                fail(g_strdup_printf("unexpected call catalog_addentry(catalog, '%s', '%s')\n",
                                     path,
                                     name));
                expectation_call(NULL);
                return FALSE;
        }
        fail_unless(strcmp(args->name, name)==0,
                    g_strdup_printf("wrong name for %s, expected '%s', got '%s'",
                                    args->expect.description,
                                    args->name,
                                    name));
        fail_unless(strcmp(args->long_name, long_name)==0,
                    g_strdup_printf("wrong long_name for %s, expected '%s', got '%s'",
                                    args->expect.description,
                                    args->long_name,
                                    long_name));
        fail_unless(source_id==args->command_id,
                    g_strdup_printf("wrong command id for %s, expected %d, got %d",
                                    args->expect.description,
                                    args->command_id,
                                    source_id));
        expectation_call(&args->expect);
        if(id_out)
                *id_out=args->id;
        return TRUE;
}


char *ocha_gconf_get_source_attribute(const char *type, int source_id, const char *attribute)
{
        char *retval;

        init_source_attrs();
        retval = g_hash_table_lookup(source_attrs, attribute);
        return retval ? g_strdup(retval):NULL;
}
gboolean ocha_gconf_set_source_attribute(const char *type, int source_id, const char *attribute, const char *value)
{
        init_source_attrs();
        g_hash_table_insert(source_attrs, g_strdup(attribute), g_strdup(value));
        return TRUE;
}

gchar *ocha_gconf_get_source_attribute_key(const char *type, int source_id, const char *attribute)
{
        return "fake_key";
}
GConfClient *ocha_gconf_get_client()
{
        return NULL;
}

guint        gconf_client_notify_add(GConfClient* client,
                                     const gchar* namespace_section, /* dir or key to listen to */
                                     GConfClientNotifyFunc func,
                                     gpointer user_data,
                                     GFreeFunc destroy_notify,
                                     GError** err)
{
        return 9;
}

void gconf_client_notify_remove(GConfClient *client, guint num)
{}
gboolean catalog_add_source(struct catalog *catalog, const char *type, int *id)
{
        fail("unexpected call");
        return FALSE;
}

gboolean catalog_remove_entry(struct catalog *catalog, int source_id, const char *path)
{
        return TRUE;
}
gboolean catalog_get_source_content(struct catalog *catalog, int source_id, catalog_callback_f callback, void *userdata)
{
        return TRUE;
}

/* ------------------------- static functions*/
static void expectation_init(struct expectation *ex, const char *description)
{
        ex->expected=1;
        ex->found=0;
        ex->description=description;
}

static gboolean expectation_check(struct expectation *expect)
{
        if(expect->expected!=expect->found)
        {
                static gboolean firsterror=TRUE;
                if(firsterror) {
                        fprintf(stderr, "\n---\n");
                        firsterror=FALSE;
                }
                fprintf(stderr,
                        "%s: expected %d call(s), got %d\n",
                        expect->description,
                        expect->expected,
                        expect->found);
                return FALSE;
        }
        return TRUE;
}

static void expectation_call(struct expectation *expect)
{
        if(expect==NULL)
        {
                fail("unexpected call");
        } else
        {
                expect->found++;
                if(expect->found>expect->expected) {
                        fail(g_strdup_printf("%s: expected %d call(s), got %d",
                                             expect->description,
                                             expect->expected,
                                             expect->found));
                }
        }
}

static void expectations_met_cb(gpointer key, gpointer value, gpointer userdata)
{
        gboolean *retval = (gboolean *)userdata;
        struct expectation *expect = (struct expectation *)value;
        if(!expectation_check(expect))
                *retval=FALSE;
}

static void init_source_attrs()
{
        if(!source_attrs)
                source_attrs=g_hash_table_new(g_str_hash, g_str_equal);
}
