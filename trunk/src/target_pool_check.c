#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 
#include <stdio.h>
#include <string.h>
#include <check.h>
#include <glib.h>
#include "target_pool.h"

/**
 * Stub implementation of target.h API.
 *
 * This implements only what target_pool should use
 * and helps when testing target pools
 */
struct target_stub
{
   struct target target;
   bool freed;
   int refs;
   mempool_freer_f freer;
   gpointer freer_data;
   gchar id[];
};

#define target_refcount(target) (((struct target_stub *)(target))->refs)
#define target_is_freed(target) (((struct target_stub *)(target))->freed)
static struct target *target_stub_new(const gchar *id);

#define TARGETS_COUNT 12
static struct target *targets[TARGETS_COUNT];
static struct target_pool *pool;
static void setup()
{
   char name[]="idX";
   for(int i=0; i<TARGETS_COUNT; i++)
	  { 
		 name[2]='A'+i;
		 targets[i]=target_stub_new(name);
	  } 
   pool = target_pool_new();
}

static void teardown()
{
}

/**
 * After the pool is deleted, all
 * references held to registered
 * targets are released.
 */
START_TEST(test_delete_unrefs_all) 
{
   for(int i=0; i<TARGETS_COUNT; i++) {
	  target_pool_register(pool, targets[i]);
	  target_unref(targets[i]);
   } 
   target_pool_delete(pool);
   for(int i=0; i<TARGETS_COUNT; i++)
	  fail_unless(target_is_freed(targets[i]), "reference held on target");
} 
END_TEST

/**
 * Try and free a registered target
 * after the pool has been deleted.
 */
START_TEST(test_unref_after_delete)
{
   target_pool_register(pool, targets[0]);
   target_pool_delete(pool);
   fail_unless(target_refcount(targets[0])==1, "wrong target count");
   target_unref(targets[0]);
}
END_TEST

/**
 * Register a target and look it up
 */
START_TEST(test_lookup)
{
   for(int i=1; i<TARGETS_COUNT; i++) 
	  target_pool_register(pool, targets[i]);

   /* target[0] not registered */
   fail_unless(target_pool_get(pool, "idA")==NULL, "target[0]");
   
   char name[]="idX";
   for(int i=1; i<TARGETS_COUNT; i++) {
	  name[2]='A'+i;
	  fail_unless(target_pool_get(pool, name)==targets[i], "wrong target lookup");
   }
}
END_TEST

/**
 * Make sure target_pool_get increments reference counte
 */
START_TEST(test_refcount)
{
   target_pool_register(pool, targets[0]);
   fail_unless(targets[0]==target_pool_get(pool, "idA"), "target[0]");
   fail_unless(target_refcount(targets[0])==2, "wrong reference count");
}
END_TEST

/**
 * When the reference count of a target
 * reaches 0, it should be removed from the
 * pool.
 */
START_TEST(test_cleanup)
{
   target_pool_register(pool, targets[0]);
   target_unref(targets[0]);

   /* not specified: the pool is allowed
	* to keep a reference on the target if
	* it wants. 
	*/
   int refcount = target_refcount(targets[0]);
   printf("test_cleanup: refcount=%d\n", refcount);
   if(refcount==0) {
	  fail_unless(target_pool_get(pool, "idA")==NULL, "target[0] found with refcount==0");
   } else {
	  fail_unless(target_pool_get(pool, "idA")==targets[0], "target[0] not found with refcount>0");
   }
}
END_TEST

START_TEST(test_replace)
{
   strcpy((char*)targets[1]->id, "idA");
   target_pool_register(pool, targets[0]);
   target_pool_register(pool, targets[1]);
   /* id a reference to targets[0]->id is still
	* kept, this should screw up lookup
	*/
   strcpy((char*)targets[0]->id, "xxx"); 


   fail_unless(target_pool_get(pool, "idA")==targets[1], "idA not replaced");
}
END_TEST

/**
 * There could be a problem with
 * cleanup when a target has been replaced.
 * I don't really know how to test that the
 * target pool is only deleted after targets[0]
 * is deleted... I'm doing my best, hoping that
 * if the target is deleted twice , we'd get segfaults
 * here for one reason or another.
 */
START_TEST(test_replace_and_cleanup)
{
   target_pool_register(pool, targets[0]);
   target_pool_register(pool, targets[1]);
   target_pool_delete(pool);
   target_unref(targets[1]);
   target_unref(targets[0]);
}
END_TEST

Suite *target_pool_check_suite(void)
{
   Suite *s = suite_create("target_pool");

   TCase *tc_core = tcase_create("target_pool_core");
   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_delete_unrefs_all);
   tcase_add_test(tc_core, test_refcount);
   tcase_add_test(tc_core, test_lookup);
   tcase_add_test(tc_core, test_unref_after_delete);
   tcase_add_test(tc_core, test_cleanup);
   tcase_add_test(tc_core, test_replace);
   tcase_add_test(tc_core, test_replace_and_cleanup);
   return s;
}

int main(void)
{
  int nf; 
  Suite *s = target_pool_check_suite (); 
  SRunner *sr = srunner_create (s); 
  srunner_run_all (sr, CK_NORMAL); 
  nf = srunner_ntests_failed (sr); 
  srunner_free (sr); 
  suite_free (s); 
  return (nf == 0) ? 0:10;
}

/* ------------------------- target_stub */
struct target *target_stub_new(const gchar* id)
{
   fail_unless(id!=NULL, "null id");
   struct target_stub *stub = (struct target_stub *)g_malloc0(sizeof(struct target_stub)+strlen(id)+1);
   stub->refs=1;
   strcpy(stub->id, id);
   stub->target.id=stub->id;
   return (struct target *)stub;
}

struct target *target_ref(struct target* target)
{
   fail_unless(target!=NULL, "null target");
   struct target_stub *stub = (struct target_stub *)target;
   fail_unless(!stub->freed, "already freed");
   fail_unless(stub->refs>0, "refs<=0");
   stub->refs++;
   return target;
}

void target_unref(struct target* target)
{
   fail_unless(target!=NULL, "null target");
   struct target_stub *stub = (struct target_stub *)target;
   fail_unless(!stub->freed, "already freed");
   fail_unless(stub->refs>0, "refs<=0");
   stub->refs--;
   if(stub->refs==0) {
	  stub->freed=true;
	  if(stub->freer)
		 stub->freer(stub->freer_data);
   }
}


void target_mempool_enlist(struct target* target, gpointer resource, mempool_freer_f freer)
{
   fail_unless(target!=NULL, "null target");
   struct target_stub *stub = (struct target_stub *)target;
   fail_unless(!stub->freed, "already freed");
   fail_unless(stub->refs>0, "refs<=0");
   fail_unless(stub->freer==NULL, "already a freer");
   stub->freer=freer;
   stub->freer_data=resource;
}
