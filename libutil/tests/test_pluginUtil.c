/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2010 Cluster Logic Ltd

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright-ClusterLogic.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

/******************************************************************************
 *
 * Author(s): Amar Lior
 *
 *****************************************************************************/


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


#include <pluginUtil.h>
#include <glib-2.0/glib.h>

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

START_TEST (test_getDirFiles)
{
    print_start("Testing basic");
    if (debug) msx_set_debug(MAP_DEBUG);

    GPtrArray *arr;

    if (!getFilesInDir("plugins-dir/avail-plugins", &arr, ".conf")) {
        printf("Error getting files\n");
    }
    printf("Got %d elements\n", arr->len);
    //for(int i=0 ; i< arr->len ; i++) {
      //  printf("[%s]\n", (char *) g_ptr_array_index (arr, i));
    //}
    

    fail_unless(arr->len == 3, "Size of array is not 3");

    fail_unless(strcmp((g_ptr_array_index (arr, 0)), "plugin3.conf") == 0, "First element is not plugin3.conf [%s]",
            g_ptr_array_index (arr, 0));

    print_end();
    msx_set_debug(0);
}
END_TEST


/***************************************************/
Suite *mapper_suite(void)
{
    Suite *s = suite_create("Plugin Util");
    TCase *tc_basic = tcase_create("basic");
  
    suite_add_tcase (s, tc_basic);
  

    tcase_add_test(tc_basic, test_getDirFiles);
  
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
