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

//#include <common.h>
#include <msx_error.h>
#include <msx_debug.h>

#include <debug_util.h>

#include <providerUtil.h>

//#include <distance_graph.h>


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


int lastVmSize = -1; // in k
int peakVmSize = -1;
int
checkVmSize()
{
       char filename[128];
       snprintf(filename, 128, "/proc/%d/statm", getpid());
       FILE *f = fopen(filename, "r");
       int size;
       fscanf(f, "%*d %d", &size); // Rss (see "man 5 proc")
       //fscanf(f, "%d", &size); // Vm
       size *= getpagesize()/1024;
       fclose(f);
       /*if (lastVmSize==-1)
               printf("size: %dk\n", size);
       else
               printf("size: %dk (%dk)\n", size, size-lastVmSize);*/
       lastVmSize = size;
       if (peakVmSize<lastVmSize)
               peakVmSize = lastVmSize;
       return lastVmSize;
}


START_TEST (test_read_big_file)
{
   print_start("reading big file");
   //msx_set_debug(VEC_DEBUG);
   int size = read_proc_file("cpuinfo");
   fail_unless(size == 12912 , "Read big cpuinfo file");
   
   size = read_proc_file("cpuinfo");
   fail_unless(size == 12912 , "Read big cpuinfo file (second time)");
   
   print_end();
   msx_set_debug(0);
}
END_TEST

/***************************************************/
Suite *providerUtil_suite(void)
{
  Suite *s = suite_create("providerUtil");

  TCase *tc_core = tcase_create("Core");
  
  suite_add_tcase (s, tc_core);
  

  tcase_add_test(tc_core, test_read_big_file);
  
  return s;
}


int main(int argc, char **argv)
{
  int nf;

  if(argc > 1 )
          debug = 1;

  //printf("ddddd\n");
  Suite *s = providerUtil_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


