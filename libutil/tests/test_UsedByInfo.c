/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <unistd.h>
#include <stdio.h>
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

//#include <common.h>
#include <msx_error.h>
#include <msx_debug.h>

#include <UsedByInfo.h>

int debug=0;

char bigBuff[4096];
static char *curr_msg;
void print_start(char *msg)
{
	curr_msg = msg;
	if(debug)
		printf("\n================ %15s ===============\n", msg);
}

void print_end()
{
	if(debug)
		printf("\n++++++++++++++++ %15s +++++++++++++++\n", curr_msg);
}

START_TEST (test_basic)
{
        node_usage_status_t stat;
        
        print_start("Testing basic");
        //msx_set_debug(PRIOD_DEBUG);

        stat = strToUsageStatus(usageStatusStr(UB_STAT_INIT));
        fail_unless(stat == UB_STAT_INIT, "Translation of UB_STAT_INIT failed");

        stat = strToUsageStatus(usageStatusStr(UB_STAT_LOCAL_USE));
        fail_unless(stat == UB_STAT_LOCAL_USE, "Translation of UB_STAT_LOCAL_USE failed");

        stat = strToUsageStatus(usageStatusStr(UB_STAT_GRID_USE));
        fail_unless(stat == UB_STAT_GRID_USE, "Translation of UB_STAT_GRID_USE failed");

        stat = strToUsageStatus(usageStatusStr(UB_STAT_FREE));
        fail_unless(stat == UB_STAT_FREE, "Translation of UB_STAT_FREE failed");

        stat = strToUsageStatus(usageStatusStr(UB_STAT_ERROR));
        fail_unless(stat == UB_STAT_ERROR, "Translation of UB_STAT_ERROR failed");

        stat = strToUsageStatus(usageStatusStr(17));
        fail_unless(stat == UB_STAT_ERROR, "Translation of 17 succeded");
        
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


void init_entry(used_by_entry_t *ent, char *name) {
        strcpy(ent->clusterName, name);
        ent->uidNum = 0;
}
void add_using_user(used_by_entry_t *ent, int uid) {
        ent->uid[ent->uidNum++] = uid;
}
void add_using_cluster(used_by_entry_t *arr, int *size, used_by_entry_t *ent)
{
        arr[*size] = *ent;
        (*size)++;
}

START_TEST (test_ub_grid_use)
{
        node_usage_status_t stat;
        int             res;
        int             size = 50;
        used_by_entry_t ent;
        used_by_entry_t arr[50];
        int             size2 = 50;
        used_by_entry_t arr2[50];
        
        
        print_start("Testing good external info");
        msx_set_debug(PRIOD_DEBUG);

        // Str 1
        
        init_entry(&ent, "c1");
        add_using_user(&ent, 1297);
        add_using_user(&ent, 1111);
        size = 0;
        add_using_cluster(arr, &size, &ent);
        
        res = writeUsageInfo(UB_STAT_GRID_USE, arr, size, bigBuff);
        fail_unless(res != 0, "Failed to write used by info");

        if(debug)
                printf("Got xml string:\n%s\n", bigBuff);
        size2 = 50;
        stat = getUsageInfo(bigBuff, arr2, &size2);
        fail_unless(stat == UB_STAT_GRID_USE, "Faild to parse good external info");
        fail_unless(size2 == 1, "There should be 1 using clusters");
        
        
        fail_unless(strcmp(arr2[0].clusterName, "c1") == 0,
                    "Cluster name is wrong (shoult be c1");
        fail_unless(arr2[0].uidNum == 2, "There should be 2 users");
        fail_unless(arr2[0].uid[0] == 1297, "Uid 1 is not 1297");
        fail_unless(arr2[0].uid[1] == 1111, "Uid 2 is not 1111");

        

        // Case 2
        // Str 1

        size = 0;
        init_entry(&ent, "c1");
        add_using_user(&ent, 1111);
        add_using_cluster(arr, &size, &ent);

        init_entry(&ent, "c2");
        add_using_user(&ent, 0);
        add_using_cluster(arr, &size, &ent);

        
        res = writeUsageInfo(UB_STAT_GRID_USE, arr, size, bigBuff);
        fail_unless(res != 0, "Failed to write used by info");

        if(debug)
                printf("Got xml string:\n%s\n", bigBuff);
        size2 = 50;
        stat = getUsageInfo(bigBuff, arr2, &size2);
        fail_unless(stat == UB_STAT_GRID_USE, "Faild to parse good external info");
        fail_unless(size2 == 2, "There should be 2 using clusters");
        fail_unless(strcmp(arr2[0].clusterName, "c1") == 0,
                    "Cluster name is wrong (should be c1");
        fail_unless(arr2[0].uidNum == 1, "There should be 1 user in c1");
        fail_unless(arr2[0].uid[0] == 1111, "Uid 1 is not 1111");
        
        fail_unless(strcmp(arr2[1].clusterName, "c2") == 0,
                    "Cluster name is wrong (should be c2");
        fail_unless(arr2[1].uidNum == 1, "There should be 1 user in c2");
        fail_unless(arr2[1].uid[0] == 0, "Uid 1 is not 0");
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


START_TEST (test_ub_local_use)
{
        node_usage_status_t stat;
        int             res;
        int             size = 50;
        used_by_entry_t ent;
        used_by_entry_t arr[50];
        int             size2 = 50;
        used_by_entry_t arr2[50];
        
        
        print_start("Testing good external info");
        msx_set_debug(PRIOD_DEBUG);

        // Str 1
        
        init_entry(&ent, "local");
        add_using_user(&ent, 1);
        add_using_user(&ent, 2);
        add_using_user(&ent, 3);
        add_using_user(&ent, 4);
        size = 0;
        add_using_cluster(arr, &size, &ent);
        
        res = writeUsageInfo(UB_STAT_LOCAL_USE, arr, size, bigBuff);
        fail_unless(res != 0, "Failed to write used by info");

        if(debug)
                printf("Got xml string:\n%s\n", bigBuff);
        size2 = 50;
        stat = getUsageInfo(bigBuff, arr2, &size2);
        fail_unless(stat == UB_STAT_LOCAL_USE, "Faild to parse good external info");
        fail_unless(size2 == 1, "There should be 1 using clusters");
        
        
        fail_unless(strcmp(arr2[0].clusterName, "local") == 0,
                    "Cluster name is wrong (shoult be c1");
        fail_unless(arr2[0].uidNum == 4, "There should be 2 users");
        fail_unless(arr2[0].uid[0] == 1, "Uid 1 is not 1");
        fail_unless(arr2[0].uid[1] == 2, "Uid 2 is not 2");
        fail_unless(arr2[0].uid[2] == 3, "Uid 3 is not 3");
        fail_unless(arr2[0].uid[3] == 4, "Uid 4 is not 4");

        print_end();
        msx_set_debug(0);
} 
END_TEST

START_TEST (test_ub_other)
{
        node_usage_status_t stat;
        int             res;
        used_by_entry_t arr[50];
        int             size2 = 50;
        used_by_entry_t arr2[50];
        
        
        print_start("Testing good external info");
        msx_set_debug(PRIOD_DEBUG);

        // FREE  
        res = writeUsageInfo(UB_STAT_FREE, arr, 0, bigBuff);
        fail_unless(res != 0, "Failed to write used by info");

        if(debug)
                printf("Got xml string:\n%s\n", bigBuff);
        size2 = 50;
        stat = getUsageInfo(bigBuff, arr2, &size2);
        fail_unless(stat == UB_STAT_FREE, "Faild Free used by info");
        fail_unless(size2 == 0, "There should be 0 using clusters");

        // ERROR  
        res = writeUsageInfo(UB_STAT_ERROR, arr, 0, bigBuff);
        fail_unless(res != 0, "Failed to write used by info");

        if(debug)
                printf("Got xml string:\n%s\n", bigBuff);
        size2 = 50;
        stat = getUsageInfo(bigBuff, arr2, &size2);
        fail_unless(stat == UB_STAT_ERROR, "Faild ERROR used by info");
        fail_unless(size2 == 0, "There should be 0 using clusters");
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


/* char *good_ei_str_1 = "GRID USE\nmos-lab1 0"; */
/* char *good_ei_str_2 = "GRID USE\nmos-lab1 0 1297 2333"; */
/* char *good_ei_str_3 = "GRID USE\n  mos-lab1 0 1297\n"; */
/* char *good_ei_str_3_1 = "GRID USE\n  mos-lab1 0 1297 \n"; */
/* char *good_ei_str_4 = "GRID USE\n mos-lab1 0 1297\nbmos 5555 6666\n"; */
/* char *good_ei_str_4_1 = "GRID USE\n mos-lab1 0 1297\nbmos 5555 6666\n\n"; */
/* char *good_ei_str_5 = "LOCAL USE\n local_use 1111";  */
/* char *good_ei_str_6 = "FREE"; */


/* START_TEST (test_good_old) */
/* { */
/*         node_usage_status_t res; */
/*         int             size = 50; */
/*         used_by_entry_t arr[50]; */

        
/*         print_start("Testing good external info"); */
/*         msx_set_debug(PRIOD_DEBUG); */

/*         // Str 1 */
/*         res = getUsageInfo(good_ei_str_1, arr, &size); */
/*         printf("res =%d\n", res); */
/*         fail_unless(res == UB_STAT_GRID_USE, "Faild to parse good external info"); */

        
/*         fail_unless(strcmp(arr[0].clusterName, "mos-lab1") == 0, */
/*                     "Cluster name is wrong"); */
/*         fail_unless(arr[0].uid[0] == 0, "Uid is not 0"); */
/*         fail_unless(arr[0].uidNum == 1, "There are more than 1 uid"); */

/*         // Str 2 */
/*         size = 50; */
/*         res = getUsageInfo(good_ei_str_2, arr, &size); */
/*         fail_unless(res == UB_STAT_GRID_USE, "Faild to parse good external info"); */
        
/*         fail_unless(strcmp(arr[0].clusterName, "mos-lab1") == 0, */
/*                     "Cluster name is wrong"); */
/*         fail_unless(arr[0].uid[1] == 1297, "Second Uid is not 1297"); */
/*         fail_unless(arr[0].uid[2] == 2333, "Uid is not 2333"); */
/*         fail_unless(arr[0].uidNum == 3, "Should be 3 uids"); */
        
/*         // Str 3 */
/*         size = 50; */
/*         res = getUsageInfo(good_ei_str_3, arr, &size); */
/*         fail_unless(res == UB_STAT_GRID_USE, "Faild to parse good external info"); */
/*         fail_unless(strcmp(arr[0].clusterName, "mos-lab1") == 0, */
/*                     "Cluster name is wrong"); */
/*         fail_unless(arr[0].uidNum == 2, "Should be 2 uids"); */

/*         // Str 3 */
/*         size = 50; */
/*         res = getUsageInfo(good_ei_str_3_1, arr, &size); */
/*         fail_unless(res == UB_STAT_GRID_USE, "Faild to parse good external info"); */
/*         fail_unless(strcmp(arr[0].clusterName, "mos-lab1") == 0, */
/*                     "Cluster name is wrong"); */
/*         fail_unless(arr[0].uidNum == 2, "Should be 2 uids"); */
        

        
/*         // Str 4 */
/*         size = 50; */
/*         res = getUsageInfo(good_ei_str_4, arr, &size); */
/*         fail_unless(res == UB_STAT_GRID_USE, "Faild to parse good external info"); */
/*         fail_unless(size == 2, "There are 2 clusters using"); */
/*         fail_unless(strcmp(arr[1].clusterName, "bmos") == 0, */
/*                     "Cluster name is wrong (should be bmos)"); */


/*         // Str 4_1 */
/*         size = 50; */
/*         res = getUsageInfo(good_ei_str_4_1, arr, &size); */
/*         fail_unless(res == UB_STAT_GRID_USE, "Faild to parse good external info"); */
/*         fail_unless(size == 2, "There are 2 clusters using"); */
/*         fail_unless(strcmp(arr[1].clusterName, "bmos") == 0, */
/*                     "Cluster name is wrong (should be bmos)"); */
        
/*         // Str 5 */
/*         size = 50; */
/*         res = getUsageInfo(good_ei_str_5, arr, &size); */
/*         fail_unless(res == UB_STAT_LOCAL_USE , "Faild to parse good local use external info"); */
/*         fail_unless(arr[0].uid[0] == 1111, "Uid is not 0"); */
/*         fail_unless(arr[0].uidNum == 1, "There are more than 1 uid"); */

/*         // Str 6 */
/*         size = 50; */
/*         res = getUsageInfo(good_ei_str_6, arr, &size); */
/*         fail_unless(res == UB_STAT_FREE, "Faild to parse good free external info"); */

        
/*         print_end(); */
/*         msx_set_debug(0); */
/* }  */
/* END_TEST */


/* char *bad_ei_str_1 = "11 mos-lab1 0"; */
/* char *bad_ei_str_2 = "0 1297 2333 ddd"; */

/* START_TEST (test_bad) */
/* { */
/*         node_usage_status_t   res; */
/*         int             size = 50; */
/*         used_by_entry_t arr[50]; */
        
/*         print_start("Testing good external info"); */
/*         msx_set_debug(1); */

/*         // Str 1 */
/*         res = getUsageInfo(bad_ei_str_1, arr, &size); */
/*         fail_unless(res == UB_STAT_ERROR, "Bad str is good"); */
/*         // Str 2 */
/*         res = getUsageInfo(bad_ei_str_2, arr, &size); */
/*         fail_unless(res == UB_STAT_ERROR, "Bad str is good"); */
        
/*         print_end(); */
/*         msx_set_debug(0); */
/* }  */
/* END_TEST */


/***************************************************/
Suite *mapper_suite(void)
{
  Suite *s = suite_create("Parse External Info");

  TCase *tc_basic = tcase_create("basic");
  TCase *tc_good = tcase_create("good");
  TCase *tc_bad = tcase_create("bad");

  suite_add_tcase (s, tc_basic);
  suite_add_tcase (s, tc_good);
  suite_add_tcase (s, tc_bad);

  tcase_add_test(tc_basic, test_basic);
  
  tcase_add_test(tc_good, test_ub_grid_use);
  tcase_add_test(tc_good, test_ub_local_use);
  tcase_add_test(tc_good, test_ub_other);
  
//tcase_add_test(tc_bad, test_bad);

  return s;
}


int main(int argc, char **argv)
{
  int nf;

  msx_set_debug(0);
  if(argc > 1)
          debug = 1;
  
  
  Suite *s = mapper_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
