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

//#include <common.h>
#include <msx_error.h>
#include <msx_debug.h>

#include <pe.h>
#include <debug_util.h>
#include <Mapper.h>
#include <MapperBuilder.h>
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
   mapper_t map;
   int max_ranges = 600;
   map = mapperInit(max_ranges);
   fail_unless(map != NULL, "Failed to create map object");
   mapperDone(map);
}
END_TEST


char *mosixmap_map_1 = 
"192.168.0.1 1 po\n";

START_TEST (test_mosixmap_builder_from_mem)
{
   mapper_t map;
   int n;
   struct in_addr ip;

   //msx_set_debug(MAP_DEBUG);
   
   print_start("Mosixmap Map from memory");
   map = BuildMosixMap(mosixmap_map_1, strlen(mosixmap_map_1) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");
   n = mapperTotalNodeNum(map);
   fail_unless(n == 1, "Total node number != 1");
   n = mapperRangesNum(map);
   fail_unless(n == 1, "Ranges number != 1");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   print_end();
}
END_TEST


char *bad_mosixmap_map_1 =
"# A remark  \n"
"Bla Bla\n"
"192.168.0.1 10   \n"
"192.168.0.1 10   \n";

char *bad_mosixmap_map_2 =
" 192.168.0.1   10   \n"
" 192.168.0.15    \n";

char *bad_mosixmap_map_3 =
" 192.168.0.1      \n"
" 192.168.0.15  10   \n";

char *bad_mosixmap_map_4 =
" 192.168.0.1   10   \n"
" 192.168.0.15 192.168.0.15 10   \n";

char *bad_mosixmap_map_5 =
" 192.168.0.1   10  pop\n"
" 192.168.0.15  10   \n";

char *bad_mosixmap_map_6 =
" 192.168.0.1   10  x\n"
" 192.168.0.15  10  x \n";

char *bad_mosixmap_map_7 =
" 192.168.0.1   10  \n"
" 192.168.0.15  10  xx \n";


char *bad_mosixmap_map_empty_1 = "";
char *bad_mosixmap_map_empty_2 = "\n\n\n";
char *bad_mosixmap_map_empty_3 = " \t\t\n\n\n";
char *bad_mosixmap_map_empty_4 = " \n     \n";

START_TEST (test_bad_mosixmap_maps)
{
  mapper_t map;
  // msx_set_debug(MAP_DEBUG);
  print_start("bad_syntax_maps");
  map = BuildMosixMap(bad_mosixmap_map_1, strlen(bad_mosixmap_map_1) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap map with BlaBla");

  map = BuildMosixMap(bad_mosixmap_map_2, strlen(bad_mosixmap_map_2) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap map with missing count");

  map = BuildMosixMap(bad_mosixmap_map_3, strlen(bad_mosixmap_map_3) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap map with missing count");

  map = BuildMosixMap(bad_mosixmap_map_5, strlen(bad_mosixmap_map_5) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap map with missing count");

  map = BuildMosixMap(bad_mosixmap_map_6, strlen(bad_mosixmap_map_6) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap map with missing count");

  map = BuildMosixMap(bad_mosixmap_map_7, strlen(bad_mosixmap_map_7) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap map with missing count");

  // FIXME currently this is not supported
  //map = BuildMosixMap(bad_mosixmap_map_4, strlen(bad_mosixmap_map_4) + 1, INPUT_MEM);
  //fail_unless(map == NULL, "Bad mosixmap map with duplicate IP");

  map = BuildMosixMap(bad_mosixmap_map_empty_1, strlen(bad_mosixmap_map_empty_1) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap empty 1");

  map = BuildMosixMap(bad_mosixmap_map_empty_2, strlen(bad_mosixmap_map_empty_2) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap empty 2");

  map = BuildMosixMap(bad_mosixmap_map_empty_3, strlen(bad_mosixmap_map_empty_3) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap empty 3");

  map = BuildMosixMap(bad_mosixmap_map_empty_4, strlen(bad_mosixmap_map_empty_4) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad mosixmap empty 4");
  
  msx_set_debug(0);
}
END_TEST



char *good_mosixmap_map_1 =
"# A remark  \n"
"#Bla Bla\n"
"# Some blanks lines \n"
"\n"
"     \n"
"\t\t\t\n"
"    192.168.0.1  10   \n"
"    192.168.0.20 10   \n"
"    192.168.1.1  10   \n"
"    192.168.1.20 10   \n"
"    192.168.2.1  10   \n"
"    192.168.2.20 10   \n"
"    192.168.3.1  10   \n"
"    192.168.3.11 10   \n";


char *good_mosixmap_map_2 =
"    192.168.0.11 10   \n"
"    192.168.0.1  10   \n"
"    192.168.0.21 10   \n"
"    ";


char *good_mosixmap_map_3 =
"    192.168.1.11 10   \n"
"    192.168.1.1  10   \n"
"    192.168.1.21 10   \n"
"    192.168.2.11 10   \n"
"    192.168.2.1  10   \n"
"    192.168.2.21 10   \n"
"    192.168.3.11 10   \n"
"    192.168.3.1  10   \n"
"    192.168.3.21 10   \n"
"    192.168.4.11 10   \n"
"    192.168.4.1  10   \n"
"    192.168.4.21 10   \n"
"    192.168.5.11 10   \n"
"    192.168.5.1  10   \n"
"    192.168.5.21 10   \n"
"    192.168.6.11 10   \n"
"    192.168.6.1  10   \n"
"    192.168.6.21 10   \n"
"    192.168.7.11 10   \n"
"    192.168.7.1  10   \n"
"    192.168.7.21 10   \n"
"    192.168.8.11 10   \n"
"    192.168.8.1  10   \n"
"    192.168.8.21 10   \n"
"    192.168.9.11 10   \n"
"    192.168.9.1  10   \n"
"    192.168.9.21 10   \n"
"    192.168.10.11 10   \n"
"    192.168.10.1  10   \n"
"    192.168.10.21 10   \n"
"\n";

char *good_mosixmap_map_4 =
"    192.168.0.11 10  p\n"
"    192.168.0.1  10  p\n"
"    192.168.0.21 10  po\n"
"    ";


START_TEST (test_good_mosixmap_maps)
{
  mapper_t map;
  // int res;
  //int max_ranges = 600;
  int n;
  
  print_start("good_mosixmap_maps");

  map = BuildMosixMap(good_mosixmap_map_1, strlen(good_mosixmap_map_1) + 1, INPUT_MEM);
  fail_unless(map != NULL, "Failed to create map object");
  n = mapperTotalNodeNum(map);
  fail_unless(n == 80, "Total node number != 80");
  n = mapperRangesNum(map);
  fail_unless(n == 8, "Ranges number != 8");
  mapperDone(map);
  

  map = BuildMosixMap(good_mosixmap_map_2, strlen(good_mosixmap_map_2) + 1, INPUT_MEM);
  fail_unless(map != NULL, "Failed to create map object");
  n = mapperTotalNodeNum(map);
  fail_unless(n == 30, "Total node number != 30");
  n = mapperRangesNum(map);
  fail_unless(n == 3, "Ranges number != 3");
  mapperDone(map);

  map = BuildMosixMap(good_mosixmap_map_3, strlen(good_mosixmap_map_3) + 1, INPUT_MEM);
  fail_unless(map != NULL, "Failed to create map object");
  n = mapperTotalNodeNum(map);
  fail_unless(n == 300, "Total node number != 300");
  n = mapperRangesNum(map);
  fail_unless(n == 30, "Ranges number != 30");
  mapperDone(map);

  map = BuildMosixMap(good_mosixmap_map_4, strlen(good_mosixmap_map_4) + 1, INPUT_MEM);
  fail_unless(map != NULL, "Failed to create map object");
  n = mapperTotalNodeNum(map);
  fail_unless(n == 30, "Total node number != 30");
  n = mapperRangesNum(map);
  fail_unless(n == 3, "Ranges number != 3");
  mapperDone(map);

  
  
  //printf("Printing map\n");
  //mapper_printf(map, PRINT_TXT);
  //mapper_done(map);
  
  
  //Print_end();
 } 
END_TEST 


/**************
 * Maps with intersections
 **************/
char *bad_mosixmap_intersect_1 =
"# A remark  \n"
"  192.168.0.1 10   \n"
"  192.168.0.1 10   \n";

char *bad_mosixmap_intersect_2 =
"    192.168.0.1 10   \n"
"    192.168.0.1 25   \n";

char *bad_mosixmap_intersect_3 =
"    192.168.0.1  10   \n"
"    192.168.0.6  25   \n";

char *bad_mosixmap_intersect_5 =
"    192.168.0.1   10   \n"
"    192.168.0.10  25   \n";

START_TEST (test_bad_mosixmap_intersect)
{
	mapper_t map;
	//msx_set_debug(MAP_DEBUG);
	map = BuildMosixMap(bad_mosixmap_intersect_1,
			       strlen(bad_mosixmap_intersect_1) + 1, INPUT_MEM);
	fail_unless(map == NULL, "Map with duplicate lines");
	
	map = BuildMosixMap(bad_mosixmap_intersect_2,
			       strlen(bad_mosixmap_intersect_2) + 1, INPUT_MEM);
	fail_unless(map == NULL, "IP intersect");
	
	map = BuildMosixMap(bad_mosixmap_intersect_3,
			       strlen(bad_mosixmap_intersect_3) + 1, INPUT_MEM);
	fail_unless(map == NULL, "IP intersect");
	
		
	map = BuildMosixMap(bad_mosixmap_intersect_5,
			       strlen(bad_mosixmap_intersect_5) + 1, INPUT_MEM);
	fail_unless(map == NULL, "PE intersect on edge");
	
	msx_set_debug(0);
}
END_TEST

/******************** Queries tests **************************/
/**************
 * Query map
 **************/
char *mosixmap_query_map_1 =
"    192.168.0.15  25   \n"
"    192.168.0.1   10   \n"
"    192.168.0.80   25   \n"
"    192.168.2.1   10   \n"
"    192.168.2.15  25   \n"
"    192.168.3.1   10   \n"
"    192.168.3.15  25   \n"
"    192.168.4.1    10   \n"
"    192.168.4.11   10   \n"
"    192.168.4.21   10   \n"
"    192.168.4.31   10   \n"
"    192.168.4.41   10   \n"
"    192.168.4.51   25   \n";


/* START_TEST (test_query) */
/* { */
/* 	mapper_t map; */
/* 	int res; */
/* 	char hostname[80]; */
/* 	pe_t pe; */
/* 	mapper_node_info_t inf; */
	
/* 	//msx_set_debug(MAP_DEBUG); */
/* 	print_start("test_query"); */
/* 	map = BuildMosixMap(mosixmap_query_map_1, */
/* 			       strlen(mosixmap_query_map_1) + 1, INPUT_MEM); */
/* 	fail_unless(map != NULL, "Failed to build map object from mosixmap_query_map_1"); */
	
/* 	res = mapper_node2host(map, PE_SET(0,1), hostname); */
/* 	fail_unless(res == 1 && strcmp(hostname,"192.168.0.1") == 0, "node2host() failed"); */
	
/* 	res = mapper_node2host(map, PE_SET(0,17), hostname); */
/* 	fail_unless(res == 1 && strcmp(hostname,"192.168.0.17") == 0, "node2host() failed"); */
	
/* 	res = mapper_node2host(map, PE_SET(0,81), hostname); */
/* 	fail_unless(res == 1 && strcmp(hostname,"192.168.0.81") == 0, "node2host() failed"); */
	
/* 	res = mapper_node2host(map, PE_SET(0,209), hostname); */
/* 	fail_unless(res == 1 && strcmp(hostname,"192.168.3.10") == 0, "node2host() failed"); */
	
/* 	res = mapper_node2host(map, PE_SET(0,270), hostname); */
/* 	fail_unless(res == 0, "node2host() should have failed"); */
	
	
/* 	//// host2node and adrr2node *\/ */
/* 	res = mapper_hostname2node(map, "192.168.3.10", &pe);  */
/* 	fail_unless(res == 1 && pe == PE_SET(0,209), "hostname2node() failed"); */
		
/* 	res = mapper_hostname2node(map, "192.168.4.75", &pe); */
/* 	fail_unless(res == 1 && pe == PE_SET(0,475), "hostname2node() failed"); */


/* 	res = mapper_hostname2node(map, "192.168.44.10", &pe);  */
/* 	fail_unless(res == 0, "hostname2node() should faile");  */
	
/* 	/// get_node_at_pos */
/* 	res = mapper_node_at_pos(map, 0, &inf); */
/* 	fail_unless(res == 1 && inf.ni_pe == PE_SET(0,1), "node_at_pos() failed"); */


/* 	res = mapper_node_at_pos(map, 75, &inf); */
/* 	//if(res) */
/* 	//printf("Got pe %d %s\n", inf.ni_pe, inet_ntoa(inf.ni_addr)); */

/* 	fail_unless(res == 1 && inf.ni_pe == PE_SET(0,155), "node_at_pos() failed"); */
/* 	res = mapper_node_at_pos(map, 180, &inf); */
/* 	inf.ni_addr.s_addr = htonl(inf.ni_addr.s_addr); */
/* 	//if(res) */
/* 	//printf("Got pe %d %s\n", inf.ni_pe, inet_ntoa(inf.ni_addr)); */
/* 	fail_unless(res == 1 && inf.ni_pe == PE_SET(0,451), "node_at_pos() failed"); */
	
	
/* 	mapperDone(map); */
	
/* } */
/* END_TEST */



START_TEST (test_map_iter_1)
{
  mapper_t map;
  int res;
  mapper_iter_t iter;
  mapper_node_info_t ni;
  int  n, cn, a;
  struct in_addr ip;
  
  print_start("test_map_iter_1");
  //msx_set_debug(MAP_DEBUG);
  
  map = BuildMosixMap(mosixmap_query_map_1,
			 strlen(mosixmap_query_map_1) + 1, INPUT_MEM);
  fail_unless(map != NULL, "Failed to build map object from mosixmap_query_map_1");

  inet_aton("192.168.4.5", &ip);
  n = mapperSetMyIP(map, &ip);
  fail_unless(n==1, "Setting my IP in mapper");


//res = mapperSetMyIP(map, PE_SET(6, 15));
  //fail_unless(res, "mapperSetMyIP failed, my IP is not in map");
  
  
  n = mapperTotalNodeNum(map);
  cn = mapperClusterNodeNum(map);
 
  // All iteratror
  iter = mapperIter(map, MAP_ITER_ALL);
  fail_unless(res, "Failed to create iterator");

  a = 0;
  while(mapperNext(iter, &ni))
	  a++;
  fail_unless(a == n, "Number of iterations is not the number of nodes");
  mapperIterDone(iter);
  
  // Cluster iteratror
  iter = mapperIter(map, MAP_ITER_CLUSTER);
  fail_unless(res, "Failed to create iterator");

  a = 0;
  while(mapperNext(iter, &ni))
	  a++;
  fail_unless(a == cn, "Number of iterations is not the number of cluster nodes");
  mapperIterDone(iter);
  
  mapperDone(map);
  msx_set_debug(0);
  print_end();
}
END_TEST


/* char *iter_map_2 = */
/* "cluster=1\n" */
/* "part=0 : 1 132.65.174.100 1\n" */
/* "part=0 : 2 132.65.34.139 2\n" */
/* "cluster=2\n" */
/* "part=0 : 1 132.65.34.135 4"; */

/* START_TEST (test_map_iter_2) */
/* { */
/*   mapper_t map; */
/*   int res; */
/*   mapper_iter_t iter; */
/*   mapper_node_info_t ni; */
/*   int  n, cn, pn, a; */
  
/*   print_start("test_map_iter_2"); */
  
/*   map = mapper_init(300); */
/*   fail_unless(map != NULL, "Cant create a map object"); */
/*   // The local node will be placed in partition 2 of cluster 1 */
/* //  mapper_set_my_pe(map, PE_SET(6, 15)); */
/*   res = mapper_set_from_mem(map, iter_map_2, strlen(iter_map_2)); */
/*   fail_unless(res, "set_from_mem returend 0 for a valid map"); */

/*   n = mapperTotalNodeNum(map); */
/*   fail_unless(n == 7, "Failed to get total nodes"); */
/*   cn = mapperClusterNodeNum(map); */
/*   fail_unless(cn == 3, "Failed to get total cluster nodes"); */
/*   pn = mapper_get_partition_node_num(map); */
/*   fail_unless(pn == 3, "Failed to get total partition nodes"); */

/*   // All iteratror */
/*   iter = mapper_iter_init(map, MAP_ITER_ALL); */
/*   fail_unless(res, "Failed to create iterator"); */

/*   a = 0; */
/*   while(mapper_next(iter, &ni)) */
/* 	  a++; */
/*   fail_unless(a == n, "Number of iterations is not the number of nodes"); */
/*   mapper_iter_done(iter); */
  
/*   // Cluster iteratror */
/*   iter = mapper_iter_init(map, MAP_ITER_CLUSTER); */
/*   fail_unless(res, "Failed to create iterator"); */

/*   a = 0; */
/*   while(mapper_next(iter, &ni)) */
/* 	  a++; */
/*   fail_unless(a == cn, "Number of iterations is not the number of cluster nodes"); */
/*   mapper_iter_done(iter); */

/*   // Partition iteratror */
/*   iter = mapper_iter_init(map, MAP_ITER_PARTITION); */
/*   fail_unless(res, "Failed to create iterator"); */

/*   a = 0; */
/*   while(mapper_next(iter, &ni)) */
/* 	  a++; */
/*   fail_unless(a == pn, "Number of iterations is not the number of partition nodes"); */
/*   mapper_iter_done(iter); */
  
/*   mapper_done(map); */

/*   print_end(); */
/* } */
/* END_TEST */



START_TEST (test_mem_leak)
{
  mapper_t            map;
  int                 res;
  mapper_iter_t       iter;
  mapper_node_info_t  ni;
  int                 a;
  
  print_start("test_mem_leak");
  //msx_set_debug(MAP_DEBUG);

  unsigned long beforeMemSize = getMyMemSize();
  fail_unless(beforeMemSize > 0 , "Failed to get memory size");

  for(int i=0 ; i<1000 ; i++) {
          map = BuildMosixMap(good_mosixmap_map_3,
                              strlen(good_mosixmap_map_3) + 1, INPUT_MEM);
          fail_unless(map != NULL, "Failed to build map object from good_mosixmap_map_3");
          
          
          iter = mapperIter(map, MAP_ITER_ALL);
          fail_unless(res, "Failed to create iterator");
          
          a = 0;
          while(mapperNext(iter, &ni))
                  a++;
          mapperIterDone(iter);
          mapperDone(map);
          
  }

  unsigned long afterMemSize = getMyMemSize();
  fail_unless(afterMemSize > 0 , "Failed to get memory size");
  if(debug)
          fprintf(stderr, "before %lu after %lu\n", beforeMemSize, afterMemSize);
  fail_unless( beforeMemSize == afterMemSize);
  
  msx_set_debug(0);
  print_end();
}
END_TEST

/***************************************************/
Suite *mapper_suite(void)
{
  Suite *s = suite_create("Mapper");

  TCase *tc_core = tcase_create("Core");
  TCase *tc_query = tcase_create("Queries");
  TCase *tc_iter = tcase_create("Iter");
  
  suite_add_tcase (s, tc_core);
  suite_add_tcase (s, tc_query);
  suite_add_tcase (s, tc_iter);
  
  tcase_add_test(tc_core, test_create);
  tcase_add_test(tc_core, test_mosixmap_builder_from_mem);
  tcase_add_test(tc_core, test_good_mosixmap_maps); 
  tcase_add_test(tc_core, test_bad_mosixmap_maps);
  tcase_add_test(tc_core, test_bad_mosixmap_intersect);
  tcase_add_test(tc_core, test_mem_leak);

   // The queires  
  //tcase_add_test(tc_query, test_query); 
  
  // Map Iterator checking 
  tcase_add_test(tc_iter, test_map_iter_1); 
  
  return s;
}


int main(int argc, char **argv)
{
  int nf;

  if(argc > 1)
          debug = 1;
  
  Suite *s = mapper_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
