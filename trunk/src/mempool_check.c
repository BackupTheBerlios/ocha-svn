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

Suite *mempool_check_suite(void)
{
   Suite *s = suite_create("mempool");
   TCase *tc_core = tcase_create("mempool_core");
   
   suite_add_tcase(s, tc_core);
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_newpool);
   tcase_add_test(tc_core, test_zeroed);
   tcase_add_test(tc_core, test_largeallocations);
   
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
