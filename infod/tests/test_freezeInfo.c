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
#include <provider.h>

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




START_TEST (test_get_freeze_dirs)
{
        int     res;
        freeze_info_t fi;
        
        print_start("Basic pim functionality");
        msx_set_debug(0);

        bzero(&fi, sizeof(freeze_info_t));
        
        res = get_freeze_dirs(&fi, "freeze_conf/freeze.conf");
        fail_unless(res == 1, "Failed to get freeze dirs");
        fail_unless(fi.freeze_dirs_num == 1, "Got wrong number of freeze dirs (should be 1)");
        fail_unless(strcmp(fi.freeze_dirs[0], "/mopi") == 0, "Got wrong freeze dir (should be /mopi)");

        
        bzero(&fi, sizeof(freeze_info_t));
        res = get_freeze_dirs(&fi, "freeze_conf/freeze_2.conf");
        fail_unless(res == 1, "Failed to get freeze dirs");
        fail_unless(fi.freeze_dirs_num == 3, "Got wrong number of freeze dirs (should be 3)");
        fail_unless(strcmp(fi.freeze_dirs[0], "/mopi") == 0, "Got wrong freeze dir (should be /mopi)");
        fail_unless(strcmp(fi.freeze_dirs[1], "/tmp") == 0, "Got wrong freeze dir (should be /tmp)");
        fail_unless(strcmp(fi.freeze_dirs[2], "/home") == 0, "Got wrong freeze dir (should be /homw)");

        bzero(&fi, sizeof(freeze_info_t));
        res = get_freeze_dirs(&fi, "freeze_conf/freeze_3.conf");
        fail_unless(res == 1, "Failed to get freeze dirs");
        fail_unless(fi.freeze_dirs_num == 3, "Got wrong number of freeze dirs (should be 3)");
        fail_unless(strcmp(fi.freeze_dirs[0], "/mopi") == 0, "Got wrong freeze dir (should be /mopi)");
        fail_unless(strcmp(fi.freeze_dirs[1], "/tmp") == 0, "Got wrong freeze dir (should be /tmp)");
        fail_unless(strcmp(fi.freeze_dirs[2], "/home") == 0, "Got wrong freeze dir (should be /homw)");
        
        bzero(&fi, sizeof(freeze_info_t));
        res = get_freeze_dirs(&fi, "freeze_conf/freeze_4.conf");
        fail_unless(res == 1, "Failed to get freeze dirs");
        fail_unless(fi.freeze_dirs_num == 0, "Got wrong number of freeze dirs (should be 0)");
        
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


START_TEST (test_get_freeze_space)
{
        print_start("Basic pim functionality");
        msx_set_debug(0);

        // All the following is under debug since it is not a unit test but
        // a main based test
        if(debug) {
                int     res;
                freeze_info_t fi;

                bzero(&fi, sizeof(freeze_info_t));
                
                res = get_freeze_dirs(&fi, "freeze_conf/freeze.conf");
                fail_unless(res == 1, "Failed to get freeze dirs");
                res = get_freeze_space(&fi);
                fail_unless(res == 1, "Failed to get freeze space");
                printf("Got total: %d free %d\n",
                       fi.total_mb, fi.free_mb);
        }
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


/***************************************************/
Suite *freeze_suite(void)
{
        Suite *s = suite_create("freezer");
        
        TCase *tc_basic = tcase_create("basic");
        
        suite_add_tcase (s, tc_basic);
        
        tcase_add_test(tc_basic, test_get_freeze_dirs);
        tcase_add_test(tc_basic, test_get_freeze_space);
  return s;
}


int main(int argc, char **argv)
{
  int nf;

  msx_set_debug(0);
  if(argc > 1)
          debug = 1;
  
  
  Suite *s = freeze_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
