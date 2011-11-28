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
#include <unistd.h>
#include <string.h>


#include <msx_error.h>
#include <msx_debug.h>
#include <readproc.h>


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

START_TEST(test_basic) {
    int size = 100;
    int res;
    proc_entry_t p1;
    //msx_set_debug(READPROC_DEBUG);

    
    res = read_process_stat("./proc-test/proc1/4649", &p1);
    res = read_process_status("./proc-test/proc1/4649", &p1);

    fail_unless(res != 0, "Failed to read process 4649 stats");
    fail_unless(p1.utime == 1693, "Failed to read utime");
    fail_unless(p1.uid == 0, "Failed to read uid");

}
END_TEST



START_TEST (test_basic_mosix)
{
	proc_entry_t   *procArr;
	int            size = 100;
	int            res;

	//msx_set_debug(READPROC_DEBUG);
	
	procArr = malloc(sizeof(proc_entry_t) * 100);
	
	res = get_mosix_processes("./proc-test/proc1", MOS_PROC_ALL, procArr, &size);
	fail_unless(res != 0, "Failed to read process list");
	fail_unless(size == 1 , "Number of mosix processes is not 1");
	fail_unless(procArr[0].whereIsHere == 1, "Process should be running here");
	fail_unless(strcmp("132.65.34.136", inet_ntoa(procArr[0].fromAddr)) == 0,
		    "Process info is not correct\n");

        res = is_mosix_process_local(&procArr[0]);
        fail_unless(res == 0, "Process should not be local");
        
        res = is_mosix_process_guest(&procArr[0]);
        fail_unless(res == 1, "Process should be guest");
        
        
        size = 100;
        res =  get_mosix_processes("./proc-test/proc1", MOS_PROC_LOCAL, procArr, &size);
	fail_unless(res != 0, "Failed to read process list");
	fail_unless(size == 0 , "Number of local mosix processes is not 0");
        
        size = 100;
        res =  get_mosix_processes("./proc-test/proc1", MOS_PROC_DEPUTY, procArr, &size);
	fail_unless(res != 0, "Failed to read process list");
	fail_unless(size == 0 , "Number of local deputy mosix processes is not 0");

        size = 100;
        res =  get_mosix_processes("./proc-test/proc1", MOS_PROC_NON_DEPUTY, procArr, &size);
	fail_unless(res != 0, "Failed to read process list");
	fail_unless(size == 0 , "Number of local non deputy mosix processes is not 0");
        size = 100;
        res =  get_mosix_processes("./proc-test/proc1", MOS_PROC_FROZEN, procArr, &size);
	fail_unless(res != 0, "Failed to read process list");
	fail_unless(size == 0 , "Number of frozen mosix processes is not 0");


        // Reading the proc2 directory
        size = 100;
        res =  get_mosix_processes("./proc-test/proc2", MOS_PROC_ALL, procArr, &size);
        //print_mosix_processes(procArr, size);
	fail_unless(res != 0, "Failed to read process list");
	fail_unless(size == 3 , "Number of mosix processes is not 3");
        
}

END_TEST



/*************************************************************/
Suite *mapper_suite(void) {
    Suite *s = suite_create("Read Process /proc info");

    TCase *tc_core = tcase_create("Core");
    TCase *tc_stress = tcase_create("Stress");

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_stress);

    tcase_add_test(tc_core, test_basic);
    tcase_add_test(tc_core, test_basic_mosix);

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
