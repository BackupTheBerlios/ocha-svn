#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 
#include <stdio.h>
#include <check.h>
#include "mempool.h"

static void setup()
{
}

static void teardown()
{
}

START_TEST(test_newpool) 
{
   struct mempool* pool = mempool_new();
   fail_unless(pool!=NULL, "null pool");

   char *str1 = mempool_alloc(pool, 12);
   fail_unless(str1!=NULL, "null str1");

   mempool_delete(pool);
} 
END_TEST

START_TEST(test_zeroed)
{
   struct mempool* pool = mempool_new();
   char *str1 = mempool_alloc(pool, 121);
   for(int i=0; i<121; i++)
	  fail_unless(str1[i]==0, "not zeroed");
   mempool_delete(pool);
}
END_TEST

START_TEST(test_largeallocations)
{
   struct mempool* pool = mempool_new();
   int total = 0;
   fail_unless(mempool_size(pool)>=total, "no allocation");

   mempool_alloc(pool, 256);
   total+=256;
   fail_unless(mempool_size(pool)>=total, "256 bytes");


   mempool_alloc(pool, 4098);
   total+=4098;
   fail_unless(mempool_size(pool)>=total, "4k");
   
   mempool_alloc(pool, 1024*1024);
   total+=1024*1024;
   fail_unless(mempool_size(pool)>=total, "1M");

   mempool_delete(pool);
}
END_TEST

static int callback_calls;
static void test_enlist_callback(void *ptr)
{
   fail_unless(ptr==(void*)0xf001f001, "wrong pointer");
   callback_calls++;
}
START_TEST(test_enlist)
{
   struct mempool* pool = mempool_new();
   callback_calls=0;
   mempool_enlist(pool, (void*)0xf001f001, test_enlist_callback);
   fail_unless(callback_calls==0, "enlist should not free");
   mempool_delete(pool);
   fail_unless(callback_calls==1, "resource not freed");
}
END_TEST

static int toto(int val)
{
   return 1+val;
}
typedef int (*toto_f)(int);

/**
 * Make sure the pointer that's returned is 
 * correctly aligned to store pointer. Try
 * that with a function pointer
 */
START_TEST(test_align)
{
   struct mempool* pool = mempool_new();
   toto_f *storage = mempool_alloc(pool, sizeof(toto_f));
   *storage=toto;
   fail_unless((*storage)(14)==15, "call to toto() failed");
   mempool_delete(pool);
}
END_TEST

Suite *mempool_check_suite(void)
{
   Suite *s = suite_create("mempool");
   TCase *tc_core = tcase_create("mempool_core");
   
   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_newpool);
   tcase_add_test(tc_core, test_zeroed);
   tcase_add_test(tc_core, test_largeallocations);
   tcase_add_test(tc_core, test_enlist);
   tcase_add_test(tc_core, test_align);
   
   return s;
}

int main(void)
{
  int nf; 
  Suite *s = mempool_check_suite (); 
  SRunner *sr = srunner_create (s); 
  srunner_run_all (sr, CK_NORMAL); 
  nf = srunner_ntests_failed (sr); 
  srunner_free (sr); 
  suite_free (s); 
  return (nf == 0) ? 0:10;
}
