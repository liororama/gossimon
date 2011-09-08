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

#include <ProcessWatchInfo.h>

int debug=0;

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
        
        print_start("Testing basic");
        //msx_set_debug(PRIOD_DEBUG);

        print_end();
        msx_set_debug(0);
} 
END_TEST

void addProcToArr(proc_watch_entry_t *procArr, int pos,
                 char *name, int pid, proc_status_t stat)
{

        proc_watch_entry_t *ent = &procArr[pos];

        strcpy(ent->procName, name);
        ent->procPid = pid;
        ent->procStat = stat;
}

char *pw_good_1 =
"<proc-watch><proc pid=1111 name=\"priod\" stat=\"ok\" /></proc-watch>";


char *pw_good_2 =
"<proc-watch><proc pid=1111 name=\"priod\" stat=\"ok\" />"
"            <proc pid=2222 name=\"priod2\" stat=\"ok\" />"
"</proc-watch>";



START_TEST (test_good)
{
        int                res;
        int                size = 50;
        proc_watch_entry_t arr[50];
        
        print_start("Testing good external info");
        msx_set_debug(1);

        // pw_good_1
        size = 50;
        res = getProcWatchInfo(pw_good_1, arr, &size);
        fail_unless(res == 1, "Failed to parse pw_good_1");
        fail_unless(arr[0].procPid == 1111, "pw_good_1: wrong pid\n");
        fail_unless(arr[0].procStat == PS_OK, "pw_good_1: wrong status\n");
        
        // pw_good_2
        size = 50;
        res = getProcWatchInfo(pw_good_2, arr, &size);
        fail_unless(res == 1, "Failed to parse pw_good_2");
        fail_unless(size == 2, "pw_good_2: size != 2");
        fail_unless(arr[1].procPid == 2222, "pw_good_2: wrong pid\n");
        fail_unless(arr[1].procStat == PS_OK, "pw_good_2: wrong status\n");
        
        print_end();
        msx_set_debug(0);
} 
END_TEST




START_TEST (test_bad)
{
        
        print_start("Testing good external info");
        msx_set_debug(1);

        print_end();
        msx_set_debug(0);
} 
END_TEST


START_TEST (test_write)
{
        int                res;
        proc_watch_entry_t arr[50];
        int                size = 50;
        char               buff[1024];
        
        print_start("Testing good external info");
        msx_set_debug(1);

        addProcToArr(arr, 0, "infod"  , 1, PS_OK);
        addProcToArr(arr, 1, "priod"  , 2, PS_NO_PID_FILE);
        addProcToArr(arr, 2, "qmd"    , 3, PS_NO_PROC);
        addProcToArr(arr, 3, "remoted", 4, PS_OK);


        writeProcWatchInfo(arr, 4, 0, buff);
        
        size = 50;
        res = getProcWatchInfo(buff, arr, &size);
        fail_unless(res == 1, "Failed to parse written buff");
        fail_unless(size == 4, "written buff: size != 4");
        
        fail_unless(arr[0].procPid == 1 && arr[0].procStat == PS_OK,
                    "written buff: 0 wrong pid or stat \n");
        fail_unless(arr[1].procPid == 2 && arr[1].procStat == PS_NO_PID_FILE,
                    "written buff: 1 wrong pid or stat \n");
        fail_unless(arr[2].procPid == 3 && arr[2].procStat == PS_NO_PROC,
                    "written buff: 2 wrong pid or stat \n");
        fail_unless(arr[3].procPid == 4 && arr[3].procStat == PS_OK,
                    "written buff: 3 wrong pid or stat \n");
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


/***************************************************/
Suite *pw_suite(void)
{
  Suite *s = suite_create("ProcWatchInfo");

  TCase *tc_basic = tcase_create("basic");
  TCase *tc_good = tcase_create("good");
  TCase *tc_bad = tcase_create("bad");

  suite_add_tcase (s, tc_basic);
  suite_add_tcase (s, tc_good);
  suite_add_tcase (s, tc_bad);

  tcase_add_test(tc_basic, test_basic);
  tcase_add_test(tc_good, test_good);
  tcase_add_test(tc_bad, test_bad);
  tcase_add_test(tc_bad, test_write);

  return s;
}


int main(int argc, char **argv)
{
  int nf;

  msx_set_debug(0);
  if(argc > 1)
          debug = 1;
  
  
  Suite *s = pw_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
