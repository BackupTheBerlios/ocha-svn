#include "mock_launchers.h"
#include <check.h>
#include <string.h>

/** \file Mock implementation of a launcher and launcher list
 *
 */

/* ------------------------- prototypes: member functions (launcher_mock) */
static gboolean launcher_mock_execute(struct launcher *, const char *, const char *, const char *, GError **);
static gboolean launcher_mock_validate(struct launcher *, const char *);

/* ------------------------- prototypes: static functions */

/* ------------------------- definitions */
static struct launcher launcher_mock = {
        TEST_LAUNCHER,
        launcher_mock_execute,
        launcher_mock_validate
};
static struct launcher *list[] = {
        &launcher_mock,
        NULL
};


/* ------------------------- public functions */
struct launcher **launchers_list(void)
{
        return list;
}

struct launcher *launchers_get(const char *id)
{
        if(strcmp(TEST_LAUNCHER, id)==0)
                return &launcher_mock;
        return NULL;
}

/* ------------------------- member functions (launcher_mock) */
static gboolean launcher_mock_execute(struct launcher *launcher,
                                      const char *name,
                                      const char *long_name,
                                      const char *uri,
                                      GError **err)
{
        fail_unless(launcher==&launcher_mock, "invalid launcher");
        fail_unless(name!=NULL, "no name set");
        fail_unless(long_name!=NULL, "no long_name set");
        fail_unless(uri!=NULL, "no uri set");
        if(*uri=='/') {
                fail(g_strdup_printf("uri is a path, not an URI: '%s'", uri));
        }
        return TRUE;
}
static gboolean launcher_mock_validate(struct launcher *launcher, const char *uri)
{
        fail_unless(launcher==&launcher_mock, "invalid launcher");
        fail_unless(uri!=NULL, "no uri set");
        if(*uri=='/') {
                fail(g_strdup_printf("uri is a path, not an URI: '%s'", uri));
        }
        return TRUE;
}

/* ------------------------- static functions */

