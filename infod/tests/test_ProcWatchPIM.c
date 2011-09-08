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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <msx_error.h>
#include <msx_debug.h>

#include <ProcessWatchInfo.h>
#include <ProcessWatchPIM.h>

int debug=0;

char bigBuff[8192];

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

int create_my_pid_file(char *pidFile) {
        FILE *f;

        f = fopen(pidFile, "w");
        if(!f)
                return 0;
        pid_t myPid = getpid();
        fprintf(f, "%d", myPid);
        fclose(f);
        return 1;
}


START_TEST (test_basic)
{
        int     res;
        void   *pw_data;
        
        print_start("Basic pim functionality");
        msx_set_debug(0);

        char initStr[256];
        char pidFile[128];

        sprintf(pidFile, "/tmp/mypid.%d", getpid());
        res = create_my_pid_file(pidFile);
        fail_unless(res != 0, "Failed to create my pid file");
        
        sprintf(initStr, "%s %s\n", "priod", pidFile);
        
        
        res = procwatch_pim_init(&pw_data, initStr);
        fail_unless(res!=0, "Faild to init procwatch pim");

        res = procwatch_pim_update(pw_data);
        fail_unless(res!=0, "Failed to update procwatch pim");

        int  size = 4096;
        char *xmlBuff = malloc(size);
        
        res = procwatch_pim_get(pw_data, xmlBuff, &size);
        fail_unless(res!=0, "Faild to get procwatch pim result");
        
        if(debug) printf("Got xml:\n%s\n", xmlBuff);
        unlink(pidFile);

        
        proc_watch_entry_t arr[50];
                
        size = 50;
        res = getProcWatchInfo(xmlBuff, arr, &size);
        fail_unless(res == 1, "Failed to parse written buff");
        fail_unless(size == 1, "written buff: size != 1");
        
        fail_unless(arr[0].procStat == PS_OK,
                    "written buff: 0 wrong stat \n");
        
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


START_TEST (test_empty_pwpim)
{
        int     res;
        void   *pw_data;
        
        print_start("Basic pim functionality");
        msx_set_debug(0);

        char *initStr = NULL;
        
        res = procwatch_pim_init(&pw_data, initStr);
        fail_unless(res!=0, "Faild to init empty procwatch pim");
        
        res = procwatch_pim_update(pw_data);
        fail_unless(res!=0, "Failed to update procwatch pim");
        
        int  size = 4096;
        char *xmlBuff = malloc(size);
        
        res = procwatch_pim_get(pw_data, xmlBuff, &size);
        fail_unless(res != 0, "Faild to get procwatch pim result");
        fail_unless(size == 0, "Empty pwpim should return no data");
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


/***************************************************/
Suite *pw_pim_suite(void)
{
        Suite *s = suite_create("pw pim");
        
        TCase *tc_basic = tcase_create("basic");
        
        suite_add_tcase (s, tc_basic);
        
        tcase_add_test(tc_basic, test_basic);
        tcase_add_test(tc_basic, test_empty_pwpim);
  return s;
}


int main(int argc, char **argv)
{
  int nf;

  msx_set_debug(0);
  if(argc > 1)
          debug = 1;
  
  
  Suite *s = pw_pim_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
