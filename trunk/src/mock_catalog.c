#include "mock_catalog.h"
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
   const char *display_name;
   int command_id;
   int id;
};

static void expectation_init(struct expectation *, const char *);
static gboolean expectation_check(struct expectation *);
static void expectation_call(struct expectation *);
/* ------------------------- mock_catalog */
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

void mock_catalog_expect_addentry(struct catalog *catalog, const char *path, const char *display_name, int command_id, int id)
{
   struct addentry_args *expect = g_new(struct addentry_args, 1);
   expectation_init(&expect->expect,
                    g_strdup_printf("addentry(catalog, '%s', '%s', %d):%d",
                                    path,
                                    display_name,
                                    command_id,
                                    id));
   expect->path=g_strdup(path);
   expect->display_name=g_strdup(display_name);
   expect->command_id=command_id;
   expect->id=id;
   g_hash_table_insert(catalog->expected_addentry,
                       (gpointer)expect->path,
                       (gpointer)expect);
}

static void expectations_met_cb(gpointer key, gpointer value, gpointer userdata)
{
   gboolean *retval = (gboolean *)userdata;
   struct expectation *expect = (struct expectation *)value;
   if(!expectation_check(expect))
      *retval=FALSE;
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

/* ------------------------- catalog */
const char *catalog_error(struct catalog *catalog)
{
   return "mock error";
}
gboolean catalog_addentry(struct catalog *catalog, const char *path, const char *display_name, int command_id, int *id_out)
{
   struct addentry_args *args =
      (struct addentry_args *)g_hash_table_lookup(catalog->expected_addentry,
                                                  path);
   if(args==NULL)
      {
         fail(g_strdup_printf("unexpected call catalog_addentry(catalog, '%s', '%s')\n",
                              path,
                              display_name));
         expectation_call(NULL);
         return FALSE;
      }

   fail_unless(strcmp(args->display_name, display_name)==0,
               g_strdup_printf("wrong display name for %s, expected '%s', got '%s'",
                               args->expect.description,
                               args->display_name,
                               display_name));
   fail_unless(command_id==args->command_id,
               g_strdup_printf("wrong command id for %s, expected %d, got %d",
                               args->expect.description,
                               args->command_id,
                               command_id));
   expectation_call(&args->expect);
   if(id_out)
      *id_out=args->id;
   return TRUE;
}

gboolean catalog_addcommand(struct catalog *catalog, const char *name, const char *execute, int *id_out)
{
   struct addcommand_args *args =
      (struct addcommand_args *)g_hash_table_lookup(catalog->expected_addcommand,
                                                    name);
   if(args==NULL)
      {
         fail(g_strdup_printf("unexpected call catalog_addcommand(catalog, '%s', '%s')\n",
                              name,
                              execute));
         expectation_call(NULL);
         return FALSE;
      }

   fail_unless(strcmp(args->execute, execute)==0,
               g_strdup_printf("wrong execute for %s, expected '%s', got '%s'",
                               args->expect.description,
                               args->execute,
                               execute));

   expectation_call(&args->expect);
   if(id_out)
      *id_out=args->id;
   return TRUE;
}


/* ------------------------- static */
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
         if(firsterror)
            {
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
      }
   else
      {
         expect->found++;
         if(expect->found>expect->expected)
            {
               fail(g_strdup_printf("%s: expected %d call(s), got %d",
                                    expect->description,
                                    expect->expected,
                                    expect->found));
            }
      }
}
