/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <stdio.h>
#include <check.h>
#include <stdlib.h>
#include <string.h>


#include <host_list.h>


static char *curr_msg;
void print_start(char *msg)
{
	curr_msg = msg;
	printf("\n================ %15s ===============\n", msg);
}
void print_end()
{
	printf("\n++++++++++++++++ %15s +++++++++++++++\n", curr_msg);
}



START_TEST (test_single_item_list)
{

 char str[100];
 mon_hosts_t list;
 int res;

 // Testing from set
 mh_init(&list);

 strcpy(str, "mos1");
 res = mh_set(&list, str);
 fail_unless(res == 1, "Building a simple host list");
 res = mh_size(&list);
 fail_unless(res == 1, "Checking the size of a list");

 mh_free(&list);

 // Testing from add
 mh_init(&list);

 strcpy(str, "mos1");
 res = mh_add(&list, str);
 fail_unless(res == 1, "Building a simple host list");
 res = mh_size(&list);
 fail_unless(res == 1, "Checking the size of a list");
 mh_free(&list);
  
}
END_TEST


START_TEST (test_basic_list)
{

 char str[100];
 mon_hosts_t list;
 int res;
 
 mh_init(&list);

 strcpy(str, "mos1,mos2,mos3");
 res = mh_set(&list, str);
 fail_unless(res == 1, "Building a simple host list");
 res = mh_size(&list);
 fail_unless(res == 3, "Checking the size of a list");
 
}
END_TEST


START_TEST (test_complex_list)
{

 char str[100];
 mon_hosts_t list;
 int res;
 
 mh_init(&list);

 strcpy(str, "mos1..10,mos20..25,mos35,mos45");
 res = mh_set(&list, str);
 fail_unless(res == 1, "Building a simple host list");
 res = mh_size(&list);
 fail_unless(res == 18, "Checking the size of a list");
 
}
END_TEST

START_TEST (test_add_range)
{

 char str[100];
 mon_hosts_t list;
 int res;
 
 mh_init(&list);

 strcpy(str, "mos1..10,mos20..25,mos35,mos45");
 res = mh_set(&list, str);
 fail_unless(res == 1, "Building a complex host list");
 res = mh_size(&list);
 fail_unless(res == 18, "Checking the size of a list");

 res = mh_add(&list, "mos16,mos18");
 fail_unless(res == 1, "Adding to the list");
 res = mh_size(&list);
 fail_unless(res == 20, "Checking the size of a list");

 
}
END_TEST

START_TEST (test_add_range2)
{

 char str[100];
 mon_hosts_t list;
 int res;
 
 mh_init(&list);

 strcpy(str, "mos1..10,mos20..25,mos35,mos45");
 res = mh_add(&list, str);
 fail_unless(res == 1, "Building a complex host list");
 res = mh_size(&list);
 fail_unless(res == 18, "Checking the size of a list");

}
END_TEST


/*************************************************************/
Suite *mapper_suite(void)
{
  Suite *s = suite_create("HostList");
  TCase *tc_core = tcase_create("Core");
  
  suite_add_tcase (s, tc_core);

  tcase_add_test(tc_core, test_single_item_list);
  tcase_add_test(tc_core, test_basic_list);
  tcase_add_test(tc_core, test_complex_list);
  tcase_add_test(tc_core, test_add_range);
  tcase_add_test(tc_core, test_add_range2);

  return s;
}

int main(void)
{
  int nf;
  
  Suite *s = mapper_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
