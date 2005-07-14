#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <check.h>
#include <string.h>
#include "contentlist.h"

/* ------------------------- prototypes: static functions */
static void loop_as_long_as_necessary(void);
static void row_changed_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer userdata);
static ContentList *new_full_list(guint size);
static void assert_iter_index_is(int i, ContentList *list, GtkTreeIter *iter);

/* ------------------------- tests */

static void setup()
{
        gtk_init(0, NULL);
}

static void teardown()
{
}

START_TEST(test_new_get_set)
{
        guint i;
        ContentList *list;
        char name[strlen("name0")+1];
        char long_name[strlen("long_name0")+1];

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        mark_point();
        list = contentlist_new(10);
        mark_point();
        for(i=0; i<10; i++) {
                int id_out = -1;
                char *name_out = "unmodified";
                char *long_name_out = "unmodified";
                gboolean enabled;
                gboolean enabled_out;
                sprintf(&name[0], "name%d", i);
                sprintf(&long_name[0], "long_name%d", i);

                mark_point();
                fail_unless(!contentlist_get(list, i, NULL/*id_out*/, NULL/*name_out*/, NULL/*long_name_out*/, NULL/*enabled_out*/),
                            "could get content not initialized");
                mark_point();
                enabled = (i%2)==0;
                enabled_out = !enabled;

                contentlist_set(list,
                                i,
                                100+i/*id*/,
                                name,
                                long_name,
                                enabled);
                mark_point();
                fail_unless(contentlist_get(list, i, &id_out, &name_out, &long_name_out, &enabled_out),
                            "could not get initialized content");

                mark_point();
                printf("[%d] id=%d, name='%s', long_name='%s'\n",
                       i, id_out, name_out, long_name_out);

                fail_unless(id_out==(100+i),
                            "wrong id");
                fail_unless(strcmp(name, name_out)==0,
                            "wrong name");
                fail_unless(name!=name_out,
                            "name not strdup'ed");
                fail_unless(strcmp(long_name, long_name_out)==0,
                            "wrong long_name");
                fail_unless(long_name!=long_name_out,
                            "long_name not strdup'ed");
                fail_unless((enabled_out && enabled) || (!enabled_out && !enabled),
                            "wrong value for enabled");
        }
        g_object_unref(list);

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_get_with_nulls)
{
        ContentList *list;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = contentlist_new(1);
        contentlist_set(list, 0, 29/*id*/, "My Name", "My Description", TRUE);
        fail_unless(contentlist_get(list, 0, NULL, NULL, NULL, NULL),
                    "not set ?");

        g_object_unref(list);

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_send_changed)
{
        ContentList *list;
        gint run = -1;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = contentlist_new(1);
        g_signal_connect(list,
                         "row-changed",
                         G_CALLBACK(row_changed_cb),
                         &run/*userdata*/);
        contentlist_set(list, 0, 29/*id*/, "My Name", "My Description", TRUE);
        loop_as_long_as_necessary();
        fail_unless(run==0, "callback not called");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_get_n_columns)
{
        ContentList *list;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = contentlist_new(1);
        fail_unless(gtk_tree_model_get_n_columns(GTK_TREE_MODEL(list))==CONTENTLIST_N_COLUMNS,
                    "wrong column #");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_get_column_type)
{
        ContentList *list;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = contentlist_new(1);

        fail_unless(gtk_tree_model_get_column_type(GTK_TREE_MODEL(list), CONTENTLIST_COL_DEFINED)==G_TYPE_BOOLEAN,
                    "wrong type for 'defined' column");
        fail_unless(gtk_tree_model_get_column_type(GTK_TREE_MODEL(list), CONTENTLIST_COL_NAME)==G_TYPE_STRING,
                    "wrong type for 'name' column");
        fail_unless(gtk_tree_model_get_column_type(GTK_TREE_MODEL(list), CONTENTLIST_COL_LONG_NAME)==G_TYPE_STRING,
                    "wrong type for 'long_name' column");
        fail_unless(gtk_tree_model_get_column_type(GTK_TREE_MODEL(list), CONTENTLIST_COL_ENABLED)==G_TYPE_BOOLEAN,
                    "wrong type for 'enabled' column");
        fail_unless(CONTENTLIST_N_COLUMNS==4,
                    "column list changed, but test_get_column_type not updated");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_get_iter)
{
        int i;
        ContentList *list;
        GtkTreeIter iter;
        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = new_full_list(12);

        for(i=0; i<10; i++) {
                GtkTreePath *path=gtk_tree_path_new_from_indices(i, -1);
                fail_unless(gtk_tree_model_get_iter(GTK_TREE_MODEL(list), &iter, path),
                            "could not get iter");
                assert_iter_index_is(i, list, &iter);
                gtk_tree_path_free(path);
        }

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_get_path)
{
        int i;
        ContentList *list;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = new_full_list(12);

        for(i=0; i<10; i++) {
                GtkTreePath *path;
                GtkTreePath *orig;
                GtkTreeIter iter;

                orig =gtk_tree_path_new_from_indices(i, -1);
                fail_unless(gtk_tree_model_get_iter(GTK_TREE_MODEL(list), &iter, orig),
                            "could not get iter");
                path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &iter);
                fail_unless(1==gtk_tree_path_get_depth(path), "wrong depth");
                fail_unless(i==gtk_tree_path_get_indices(path)[0], "wrong indice");
                gtk_tree_path_free(path);
                gtk_tree_path_free(orig);
        }

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_get_value)
{
        int i;
        ContentList *list;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = new_full_list(12);

        for(i=0; i<10; i++) {
                GtkTreePath *path;
                GtkTreeIter iter;
                gboolean defined;
                char *name = NULL;
                char *long_name = NULL;

                path = gtk_tree_path_new_from_indices(i, -1);
                fail_unless(gtk_tree_model_get_iter(GTK_TREE_MODEL(list), &iter, path),
                            "could not get iter");

                gtk_tree_model_get(GTK_TREE_MODEL(list),
                                   &iter,
                                   CONTENTLIST_COL_DEFINED, &defined,
                                   CONTENTLIST_COL_NAME, &name,
                                   CONTENTLIST_COL_LONG_NAME, &long_name,
                                   -1);
                fail_unless(defined, "not defined ?");
                fail_unless(strcmp(name, g_strdup_printf("name %d", i))==0, "wrong name");
                fail_unless(strcmp(long_name, g_strdup_printf("long name %d", i))==0, "wrong long name");
        }

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST


START_TEST(test_iter_next)
{
        int i;
        ContentList *list;
        GtkTreePath *path;
        GtkTreeIter iter;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = new_full_list(12);
        path = gtk_tree_path_new_from_indices(0, -1);
        fail_unless(gtk_tree_model_get_iter(GTK_TREE_MODEL(list), &iter, path),
                    "could not get iter");

        for(i=0; i<12; i++) {
                assert_iter_index_is(i, list, &iter);

                if(i<11) {
                        fail_unless(gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter),
                                    "no next ?");
                } else {
                        fail_unless(!gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter),
                                    "more than 12 ?");
                }
        }

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_iter_children)
{
        ContentList *list;
        GtkTreeIter iter;
        GtkTreeIter iter2;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = new_full_list(12);
        fail_unless(gtk_tree_model_iter_children(GTK_TREE_MODEL(list), &iter, NULL/*parent=root*/),
                    "could not get iter on 1st child");
        assert_iter_index_is(0, list, &iter);
        fail_unless(!gtk_tree_model_iter_children(GTK_TREE_MODEL(list), &iter2, &iter/*parent*/),
                    "entry 0 has children ?");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_iter_has_child)
{
        ContentList *list;
        GtkTreeIter iter;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = new_full_list(12);
        gtk_tree_model_iter_children(GTK_TREE_MODEL(list), &iter, NULL/*parent=root*/);

        fail_unless(!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(list), &iter),
                    "entry 0 has children ?");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_iter_n_children)
{
        ContentList *list;
        GtkTreeIter iter;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = new_full_list(12);
        gtk_tree_model_iter_children(GTK_TREE_MODEL(list), &iter, NULL/*parent=root*/);

        fail_unless(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list), NULL/*parent=root*/)==12,
                    "wrong entry count");
        fail_unless(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list), &iter/*parent*/)==0,
                    "entry 0 has children ?");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_iter_nth_child)
{
        int i;
        ContentList *list;
        GtkTreeIter iter;
        GtkTreeIter iter2;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = new_full_list(12);

        for(i=0; i<12; i++) {
                fail_unless(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list), &iter, NULL/*parent=root*/, i),
                            "could not get iter on nth child");
                assert_iter_index_is(i, list, &iter);
        }

        gtk_tree_model_iter_children(GTK_TREE_MODEL(list), &iter, NULL/*parent=root*/);
        fail_unless(!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list), &iter2, &iter, 0),
                    "could get iter on 1st child of entry 0");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

START_TEST(test_iter_parent)
{
        ContentList *list;
        GtkTreeIter iter;
        GtkTreeIter parent;

        printf("\n%s:%d: test start ---\n", __FILE__, __LINE__);

        list = new_full_list(12);
        gtk_tree_model_iter_children(GTK_TREE_MODEL(list), &iter, NULL/*parent=root*/);
        fail_unless(!gtk_tree_model_iter_parent(GTK_TREE_MODEL(list), &parent, &iter),
                    "could get parent of entry 0 ?");

        printf("%s:%d: test end ---\n", __FILE__, __LINE__);
}
END_TEST

/* ------------------------- tests suite */

static Suite *contentlist_check_suite(void)
{
        Suite *s = suite_create("contentlist");
        TCase *tc_core = tcase_create("contentlist_core");

        suite_add_tcase(s, tc_core);
        tcase_add_checked_fixture(tc_core, setup, teardown);
        tcase_add_test(tc_core, test_new_get_set);
        tcase_add_test(tc_core, test_get_with_nulls);
        tcase_add_test(tc_core, test_send_changed);
        tcase_add_test(tc_core, test_get_n_columns);
        tcase_add_test(tc_core, test_get_column_type);
        tcase_add_test(tc_core, test_get_iter);
        tcase_add_test(tc_core, test_get_path);
        tcase_add_test(tc_core, test_get_value);
        tcase_add_test(tc_core, test_iter_next);
        tcase_add_test(tc_core, test_iter_children);
        tcase_add_test(tc_core, test_iter_has_child);
        tcase_add_test(tc_core, test_iter_n_children);
        tcase_add_test(tc_core, test_iter_nth_child);
        tcase_add_test(tc_core, test_iter_parent);


        return s;
}

int main(void)
{
        int nf;
        Suite *s = contentlist_check_suite ();
        SRunner *sr = srunner_create (s);
        srunner_run_all (sr, CK_NORMAL);
        nf = srunner_ntests_failed (sr);
        srunner_free (sr);
        suite_free (s);
        return (nf == 0) ? 0:10;
}

/* ------------------------- static functions */
static void loop_as_long_as_necessary()
{
        while(g_main_context_pending(NULL/*default context*/))
                g_main_context_iteration(NULL/*default context*/,
                                         TRUE/*may block*/);
}

static void row_changed_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer userdata)
{
        ContentList *contentlist;
        gint *indices;
        gint *found;
        GtkTreePath *path_from_iter;

        fail_unless(IS_CONTENTLIST(model), "model not a contentlist");
        fail_unless(path!=NULL, "no path");
        fail_unless(iter!=NULL, "no iter");
        fail_unless(userdata!=NULL, "no userdata");

        contentlist = CONTENTLIST(model);
        found=(gint *)userdata;

        indices=gtk_tree_path_get_indices(path);
        fail_unless(gtk_tree_path_get_depth(path)==1, "wrong path depth");
        fail_unless(indices[0]>=0 && indices[0]<contentlist_get_size(contentlist), "wrong indice");

        *found=indices[0];

        path_from_iter=gtk_tree_model_get_path(model, iter);
        indices=gtk_tree_path_get_indices(path_from_iter);
        fail_unless(gtk_tree_path_get_depth(path_from_iter)==1, "wrong path_from_iter depth");
        fail_unless(indices[0]>=0 && indices[0]<contentlist_get_size(contentlist), "wrong indice in path_from_iter");

        fail_unless(contentlist_get_at_iter(contentlist, iter, NULL, NULL, NULL, NULL/*enabled_out*/),
                    "cannot get data at iter");
}

/**
 * New, initialized list.
 *
 * Each entry of the list is initialized with:
 * (id=index+100, name="name "+index, long_name="long name "+index)
 * @param list
 * @param size
 */
static ContentList *new_full_list(guint size)
{
        guint i;
        ContentList *retval;

        retval = contentlist_new(size);
        for(i=0; i<size; i++) {
                contentlist_set(retval,
                                i,
                                i+100,
                                g_strdup_printf("name %d", i),
                                g_strdup_printf("long name %d", i),
                                TRUE);
        }
        return retval;
}

static void assert_iter_index_is(int i, ContentList *list, GtkTreeIter *iter)
{
        int id = -1;
        fail_unless(contentlist_get_at_iter(list, iter, &id, NULL, NULL, NULL/*enabled_out*/),
                    "could not get id");
        fail_unless(id==(i+100), "wrong id");
}

