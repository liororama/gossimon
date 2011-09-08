/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>


//#include <common.h>
#include <msx_error.h>
#include <msx_debug.h>

#include <pe.h>
#include <Mapper.h>
#include <MapperBuilder.h>
#include <debug_util.h>
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

START_TEST (test_create)
{
   mapper_t  map;
   char     *dirStr = "./partners-test/p1/";
   int       cNum;
   
   //msx_set_debug(MAP_DEBUG);
   print_start("creating a partners map");
   
   map = BuildPartnersMap(dirStr, PARTNER_SRC_TEXT);
   fail_unless(map != NULL, "Failed to create map object");

   cNum = mapperClusterNum(map);
   fail_unless(cNum == 3, "Cluster number is not 3");
   
   //mapperPrint(map, PRINT_TXT_PRETTY);
   mapperDone(map);
   msx_set_debug(0);
}
END_TEST

START_TEST (test_create_bin)
{
   mapper_t map;
   char *dirStr = "./partners-test/p1-bin/";
   
   msx_set_debug(MAP_DEBUG);
   print_start("creating a partners map");
   
   map = BuildPartnersMap(dirStr, PARTNER_SRC_BIN);
   fail_unless(map != NULL, "Failed to create map object");

   mapperPrint(map, PRINT_TXT_PRETTY);
   mapperDone(map);
   msx_set_debug(0);
}
END_TEST








START_TEST (test_copy)
{
   mapper_t map, map2;
   int      res;
   char    *dirStr = "./partners-test/p1/";
   char     clusterName[256];
      
   
   //msx_set_debug(MAP_DEBUG);
   print_start("creating a partners map");
   
   map = BuildPartnersMap(dirStr, PARTNER_SRC_TEXT);
   fail_unless(map != NULL, "Failed to create map object");

   //mapperPrint(map, PRINT_TXT_PRETTY);
   map2 = mapperCopy(map);
   fail_unless(map2 != NULL, "Failed to copy map to map2");

   //mapperPrint(map2, PRINT_TXT_PRETTY);
   
   res = mapper_hostname2cluster(map2, "mos1", clusterName);
   fail_unless(res == 1 , "Faild to find mos1 in partner map");
   fail_unless( strcmp(clusterName, "mos-lab1") == 0, "Result cluster name is wrong");
   
   mapperDone(map);
   msx_set_debug(0);
}
END_TEST


START_TEST (test_write)
{
   mapper_t map;
   char *dirStr = "./partners-test/p1/";
   char *writeDirStr = "./partners-test/p1-write/";
   int res;
   
   //msx_set_debug(MAP_DEBUG);
   print_start("creating a partners map");
   
   map = BuildPartnersMap(dirStr, PARTNER_SRC_TEXT);
   fail_unless(map != NULL, "Failed to create map object");

   // The name of the directory is not a variable inorder to prevent
   // error that will result in a deletion of other files
   system("rm -f ./partners-test/p1-write/*");

   res = WritePartnerMap(map, writeDirStr);
   fail_unless(res == 1, "Failed to write partner map");
   
   mapperDone(map);
   msx_set_debug(0);
}
END_TEST




START_TEST (test_x2cluster)
{
   mapper_t  map;
   char     *dirStr = "./partners-test/p1/";
   int       res;
   char      clusterName[256];
   
   //msx_set_debug(MAP_DEBUG);
   print_start("creating a partners map");
   
   map = BuildPartnersMap(dirStr, PARTNER_SRC_TEXT);
   fail_unless(map != NULL, "Failed to create map object");

   res = mapper_hostname2cluster(map, "mos1", clusterName);
   fail_unless(res == 1 , "Faild to find mos1 in partner map");

   fail_unless( strcmp(clusterName, "mos-lab1") == 0, "Result cluster name is wrong");


   res = mapper_hostname2cluster(map, "mos215", clusterName);
   fail_unless(res == 0 , "Host mos215 should not be in partners");


   mapper_cluster_info_t ci;
   res = mapper_getClusterInfo(map, "mos-lab1", &ci);
   fail_unless(res == 1, "Failed to get priority of mos-lab1");
   fail_unless(ci.ci_prio == 45, "Got wrong priority");

   ci.ci_prio = 30;
   res = mapper_setClusterInfo(map, "mos-lab2", &ci);
   fail_unless(res == 1, "Failed to set priority of mos-lab2");

   res = mapper_getClusterInfo(map, "mos-lab2", &ci);
   fail_unless(res == 1, "Failed to get priority of mos-lab2");
   fail_unless(ci.ci_prio == 30, "Got wrong priority");
   

   
   
   mapperDone(map);
   msx_set_debug(0);
}
END_TEST


START_TEST (test_cluster_iter)
{
	mapper_t  map;
	char     *dirStr = "./partners-test/p1/";
	//int       res;
	
	mapper_iter_t iter;
	mapper_node_info_t ni;
	int  a;
	
	print_start("test_cluster_iter");
	//msx_set_debug(MAP_DEBUG);
	
	map = BuildPartnersMap(dirStr, PARTNER_SRC_TEXT);
	fail_unless(map != NULL, "Failed to create map object");
	

	// All iteratror
	iter = mapperClusterNodesIter(map, "mos-lab2");
	fail_unless(iter != NULL, "Failed to create iterator");
	
	a = 0;
	while(mapperNext(iter, &ni))
		a++;
	fail_unless(a == 16, "Number of iterations is not the number of nodes");
	mapperIterDone(iter);

	mapper_cluster_iter_t cIter;
	char                  *clusterName;

	cIter = mapperClusterIter(map);
	a = 0;
	while(mapperClusterIterNext(cIter, &clusterName)) {
		//printf("Got cluster %s\n", clusterName);
		a++;
	}
	fail_unless(a == 3, "Number of iterations is not the number of clusters");


	mapper_range_iter_t rIter;
	mapper_range_t       mr;
	mapper_range_info_t  ri;
	
	rIter = mapperRangeIter(map, "mos-lab1");
	fail_unless(iter != NULL, "Failed to create range iterator");
	a = 0;
	while(mapperRangeIterNext(rIter, &mr, &ri)) {
		//printf("Got cluster %s\n", clusterName);
		a++;
 	}
	fail_unless(a == 1, "Number of iterations is not the number of ranges");
}
END_TEST


START_TEST (test_stress)
{
   mapper_t   map, map2;
   char      *dirStr = "./partners-test/p2/";
   int        i, res;
   char      clusterName[256];
   
   //msx_set_debug(MAP_DEBUG);
   print_start("stress test 1");
   
   for(i = 1 ; i< 100 ; i++) {
	   
	   map = BuildPartnersMap(dirStr, PARTNER_SRC_TEXT);
	   fail_unless(map != NULL, "Failed to create map object");
	   
	   //mapperPrint(map, PRINT_TXT_PRETTY);
	   if( i == 99)
		   break;
	   mapperDone(map);
   }
   
   //mapperPrint(map, PRINT_TXT_PRETTY);
   map2 = mapperCopy(map);
   fail_unless(map2 != NULL, "Failed to copy map to map2");

   mapperDone(map);
   //mapperPrint(map2, PRINT_TXT_PRETTY);

   for(i = 1 ; i< 100 ; i++)
   {
	   res = mapper_hostname2cluster(map2, "mos1", clusterName);
	   fail_unless(res == 1 , "Faild to find mos1 in partner map");
	   fail_unless( strcmp(clusterName, "mos-lab1") == 0, "Result cluster name is wrong");
	   
	   res = mapper_hostname2cluster(map2, "bmos-01", clusterName);
	   fail_unless(res == 1 , "Faild to find mos1 in partner map");
	   fail_unless( strcmp(clusterName, "bmos") == 0, "Result cluster name is wrong");
   }
   mapperDone(map2);
   msx_set_debug(0);
}
END_TEST

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

START_TEST (test_stress2)
{
   mapper_t   map;
   char      *dirStr = "./partners-test/p-empty/";
   int        i;
   unsigned long    beforeMem[3], afterMem[3];
   
   //msx_set_debug(MAP_DEBUG);
   print_start("test stress 2");
   beforeMem[0] = getMyRSS();
   beforeMem[1] = getMyMemSize();
   beforeMem[2] = checkVmSize();
   
   for(i = 0 ; i< 100 ; i++) {
           // Since this is an empty directory the build shoudl fail
	   map = BuildPartnersMap(dirStr, PARTNER_SRC_TEXT);
   }
   afterMem[0] = getMyRSS();
   afterMem[1] = getMyMemSize();
   afterMem[2] = checkVmSize();
   
   printf("Before:  %-8ld %-8ld %-8ld\n", beforeMem[0], beforeMem[1], beforeMem[2]);  
   printf("After:   %-8ld %-8ld %-8ld\n", afterMem[0], afterMem[1], afterMem[2]);  
   
   msx_set_debug(0);
}
END_TEST


START_TEST (test_stress3)
{
   mapper_t   map;
   char      *dirStr = "./partners-test/p1/";
   int        i, res;
   unsigned long    beforeMem[3], afterMem[3];
   char      clusterName[256];
   char *ptr;
   
   //msx_set_debug(MAP_DEBUG);
   print_start("test stress 2");
   beforeMem[0] = getMyRSS();
   beforeMem[1] = getMyMemSize();
   beforeMem[2] = checkVmSize();
   
   for(i = 0 ; i< 100 ; i++) {
	   map = BuildPartnersMap(dirStr, PARTNER_SRC_TEXT);
           fail_unless(map != NULL, "Failed to create map object");
           
           res = mapper_hostname2cluster(map, "mos1", clusterName);
           fail_unless(res == 1 , "Faild to find mos1 in partner map");
	   fail_unless( strcmp(clusterName, "mos-lab1") == 0,
                        "Result cluster name is wrong");
	   mapperDone(map);
   }
   afterMem[0] = getMyRSS();
   afterMem[1] = getMyMemSize();
   afterMem[2] = checkVmSize();
   
   printf("Before:  %-8ld %-8ld %-8ld\n", beforeMem[0], beforeMem[1], beforeMem[2]);  
   printf("After:   %-8ld %-8ld %-8ld\n", afterMem[0], afterMem[1], afterMem[2]);  
   
   msx_set_debug(0);
}
END_TEST

/***************************************************/
Suite *mapper_suite(void)
{
  Suite *s = suite_create("Mapper");

  TCase *tc_core = tcase_create("Core");
  TCase *tc_query = tcase_create("Queries");
  TCase *tc_stress = tcase_create("Stress");
  
  suite_add_tcase (s, tc_core);
  suite_add_tcase (s, tc_query);
  suite_add_tcase (s, tc_stress);
  
  tcase_add_test(tc_core, test_create);
  tcase_add_test(tc_core, test_create_bin);
  tcase_add_test(tc_core, test_copy);
  tcase_add_test(tc_core, test_write);
  
  tcase_add_test(tc_query, test_x2cluster);
  tcase_add_test(tc_query, test_cluster_iter);

  //tcase_add_test(tc_stress, test_stress);
 tcase_set_timeout(tc_stress, 30);
 tcase_add_test(tc_stress, test_stress2);
 tcase_add_test(tc_stress, test_stress3);
  
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
