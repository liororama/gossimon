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

#include <debug_util.h>
#include <pe.h>
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


char *userview_map_1 = 
"1 192.168.0.1 1 \n";

START_TEST (test_userview_builder_from_mem)
{
   mapper_t map;
   int n;
   struct in_addr ip;
   print_start("UserView Map from memory");
   map = BuildUserViewMap(userview_map_1, strlen(userview_map_1) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");
   n = mapperTotalNodeNum(map);
   fail_unless(n == 1, "Total node number != 1");
   n = mapperRangesNum(map);
   fail_unless(n == 1, "Ranges number != 1");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   n = mapperGetMyPe(map);
   fail_unless(n == 1, "Got wrong pe should be 1");
   mapperDone(map);
   
   print_end();
}
END_TEST


START_TEST (test_create_from_cmd)
{
   mapper_t map;
   char *cmdStr = "./mapper_cmd_1";

   //msx_set_debug(MAP_DEBUG);
   print_start("create from cmd");
   
   map = BuildUserViewMap(cmdStr, strlen(cmdStr), INPUT_CMD);
   fail_unless(map != NULL, "Failed to create map object");
   mapperDone(map);
   msx_set_debug(0);
}
END_TEST


START_TEST (test_create_from_file)
{
   mapper_t map;
   char *fileStr = "./mapper_file_1";

   //msx_set_debug(MAP_DEBUG);
   print_start("create from file");
   
   map = BuildUserViewMap(fileStr, strlen(fileStr), INPUT_FILE);
   fail_unless(map != NULL, "Failed to create map object");
   mapperDone(map);
   msx_set_debug(0);
}
END_TEST


/* char *create_from_another_1 = */
/* "cluster=1  \n" */
/* "   part=1 : 1   192.168.0.1   10   \n" */
/* "   part=2 : 15  192.168.0.15  25   \n" */
/* "   part=3 : 80 192.168.0.80   25   \n" */
/* "cluster=2 \n" */
/* "   part=1 : 1   192.168.2.1   10   \n" */
/* "   part=4 : 15  192.168.2.15  25   \n" */
/* "cluster=3 \n" */
/* "   part=1 : 1   192.168.3.1   10   \n" */
/* "   part=14 : 15  192.168.3.15  25   \n" */
/* "cluster=15 \n" */
/* "   part=1 :  1    192.168.4.1    10   \n" */
/* "   part=2 :  11   192.168.4.11   10   \n" */
/* "   part=3 :  21   192.168.4.21   10   \n" */
/* "   part=4 :  31   192.168.4.31   10   \n" */
/* "   part=5 :  41   192.168.4.41   10   \n" */
/* "   part=44 : 51   192.168.4.51   25   \n" */
/* "\n" */
/* " friend c1:p1  =>    c2:p1           \n" */
/* " friend c1:p2  =>    c3              \n" */
/* " friend c3     inf=> c15             \n" */
/* " friend c2:p4  U=>   c15             \n" */
/* " friend c3:p1  =>    c1:p2           \n" */
/* " friend c15    inf=> c1             \n" */
/* "\n" */
/* " zone=1 : c1 c2    \n" */
/* " zone=2 : c3 c15   \n"; */

/* START_TEST (test_create_from_another) */
/* { */
/* 	mapper_t map; */
/* 	mapper_t map2; */
/* 	int res; */
/* 	int max_ranges = 600; */
/* 	char *buff; */
	
/* 	print_start("create_from_another"); */
	
/* 	map = mapper_init(max_ranges); */
/* 	fail_unless(map != NULL, "Cant create a simple map object??"); */
/* 	mapper_set_my_pe(map, PE_SET(1, 15)); */
/* 	res = mapper_set_from_mem(map, create_from_another_1, */
/* 				  strlen(create_from_another_1)); */
/* 	fail_unless(res == 1, "set_from_mem returend 0 for a good map"); */

/* 	buff = malloc(4096); */
/* 	fail_unless(buff != NULL, "Faild to allocated mem"); */

/* 	mapper_sprintf(buff, map, PRINT_TXT); */

/* 	fprintf(stderr, "BUFF:\n%s\n", buff); */
	
/* 	map2 = mapper_init(max_ranges); */
/* 	fail_unless(map2 != NULL, "Cant create a simple map object??"); */
/* 	mapper_set_my_pe(map2, PE_SET(1, 15)); */
/* 	res = mapper_set_from_mem(map2, buff, strlen(buff)); */
/* 	fail_unless(res == 1, "set_from_mem returend 0 for setting map from buffer"); */
/* 	mapper_done(map); */
	
/* } */
/* END_TEST */


START_TEST (test_limits)
{
  mapper_t map;
  int max_ranges = -1;
  map = mapperInit(max_ranges);
  fail_unless(!map, "Created a map object with max_ranges = -1");
  mapperDone(map);
}
END_TEST

char *bad_userview_map_1 =
"# A remark  \n"
"Bla Bla\n"
"1 192.168.0.1 10   \n"
"21 192.168.0.1 10   \n";

char *bad_userview_map_2 =
" 1  192.168.0.1   10   \n"
" 15 192.168.0.15    \n";

char *bad_userview_map_3 =
" 1  192.168.0.1   10   \n"
" 192.168.0.15  10   \n";

char *bad_userview_map_4 =
" 1  192.168.0.1   10   \n"
" 11 192.168.0.15 192.168.0.15 10   \n";


char *bad_userview_map_empty_1 = "";
char *bad_userview_map_empty_2 = "\n\n\n";
char *bad_userview_map_empty_3 = " \t\t\n\n\n";
char *bad_userview_map_empty_4 = " \n     \n";

START_TEST (test_bad_userview_maps)
{
  mapper_t map;
  // msx_set_debug(MAP_DEBUG);
  print_start("bad_syntax_maps");
  map = BuildUserViewMap(bad_userview_map_1, strlen(bad_userview_map_1) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad userview map with BlaBla");

  map = BuildUserViewMap(bad_userview_map_2, strlen(bad_userview_map_2) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad userview map with missing count");

  map = BuildUserViewMap(bad_userview_map_3, strlen(bad_userview_map_3) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad userview map with missing base");

  // FIXME currently this is not supported
  //map = BuildUserViewMap(bad_userview_map_4, strlen(bad_userview_map_4) + 1, INPUT_MEM);
  //fail_unless(map == NULL, "Bad userview map with duplicate IP");

  map = BuildUserViewMap(bad_userview_map_empty_1, strlen(bad_userview_map_empty_1) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad userview empty 1");

  map = BuildUserViewMap(bad_userview_map_empty_2, strlen(bad_userview_map_empty_2) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad userview empty 2");

  map = BuildUserViewMap(bad_userview_map_empty_3, strlen(bad_userview_map_empty_3) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad userview empty 3");

  map = BuildUserViewMap(bad_userview_map_empty_4, strlen(bad_userview_map_empty_4) + 1, INPUT_MEM);
  fail_unless(map == NULL, "Bad userview empty 4");
  
  msx_set_debug(0);
}
END_TEST



char *good_userview_map_1 =
"# A remark  \n"
"#Bla Bla\n"
"# Some blanks lines \n"
"\n"
"     \n"
"\t\t\t\n"
"   1   192.168.0.1  10   \n"
"   11 192.168.0.20 10   \n"
"   100   192.168.1.1  10   \n"
"   111  192.168.1.20 10   \n"
"   200   192.168.2.1  10   \n"
"   211  192.168.2.20 10   \n"
"   300   192.168.3.1  10   \n"
"   311  192.168.3.11 10   \n";


char *good_userview_map_2 =
"   11  192.168.0.11 10   \n"
"   1   192.168.0.1  10   \n"
"   21  192.168.0.21 10   \n"
"    ";


char *good_userview_map_3 =
"   11  192.168.1.11 10   \n"
"   1   192.168.1.1  10   \n"
"   21  192.168.1.21 10   \n"
"   111  192.168.2.11 10   \n"
"   101   192.168.2.1  10   \n"
"   121  192.168.2.21 10   \n"
"   211  192.168.3.11 10   \n"
"   201   192.168.3.1  10   \n"
"   221  192.168.3.21 10   \n"
"   311  192.168.4.11 10   \n"
"   301   192.168.4.1  10   \n"
"   321  192.168.4.21 10   \n"
"   411  192.168.5.11 10   \n"
"   401   192.168.5.1  10   \n"
"   421  192.168.5.21 10   \n"
"   511  192.168.6.11 10   \n"
"   501   192.168.6.1  10   \n"
"   521  192.168.6.21 10   \n"
"   611  192.168.7.11 10   \n"
"   601   192.168.7.1  10   \n"
"   621  192.168.7.21 10   \n"
"   711  192.168.8.11 10   \n"
"   701   192.168.8.1  10   \n"
"   721  192.168.8.21 10   \n"
"   811  192.168.9.11 10   \n"
"   801   192.168.9.1  10   \n"
"   821  192.168.9.21 10   \n"
"   911  192.168.10.11 10   \n"
"   901   192.168.10.1  10   \n"
"   921  192.168.10.21 10   \n"
"\n";


START_TEST (test_good_userview_maps)
{
  mapper_t map;
  // int res;
  //int max_ranges = 600;
  int n;
  
  print_start("good_userview_maps");

  map = BuildUserViewMap(good_userview_map_1, strlen(good_userview_map_1) + 1, INPUT_MEM);
  fail_unless(map != NULL, "Failed to create map object");
  n = mapperTotalNodeNum(map);
  fail_unless(n == 80, "Total node number != 80");
  n = mapperRangesNum(map);
  fail_unless(n == 8, "Ranges number != 8");
  mapperDone(map);
  

  map = BuildUserViewMap(good_userview_map_2, strlen(good_userview_map_2) + 1, INPUT_MEM);
  fail_unless(map != NULL, "Failed to create map object");
  n = mapperTotalNodeNum(map);
  fail_unless(n == 30, "Total node number != 30");
  n = mapperRangesNum(map);
  fail_unless(n == 3, "Ranges number != 3");
  mapperDone(map);

  map = BuildUserViewMap(good_userview_map_3, strlen(good_userview_map_3) + 1, INPUT_MEM);
  fail_unless(map != NULL, "Failed to create map object");
  n = mapperTotalNodeNum(map);
  fail_unless(n == 300, "Total node number != 300");
  n = mapperRangesNum(map);
  fail_unless(n == 30, "Ranges number != 30");
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
char *bad_userview_intersect_1 =
"# A remark  \n"
" 1 192.168.0.1 10   \n"
" 1 192.168.0.1 10   \n";

char *bad_userview_intersect_2 =
"   1  192.168.0.1 10   \n"
"   15 192.168.0.1 25   \n";

char *bad_userview_intersect_3 =
"  1  192.168.0.1  10   \n"
"  5  192.168.0.6  25   \n";

char *bad_userview_intersect_4 =
"   1  192.168.0.1  10   \n"
"   10  192.168.1.2  25   \n";

char *bad_userview_intersect_5 =
"   1   192.168.0.1   10   \n"
"   15  192.168.0.10  25   \n";

START_TEST (test_bad_userview_intersect)
{
	mapper_t map;
	//msx_set_debug(MAP_DEBUG);
	map = BuildUserViewMap(bad_userview_intersect_1,
			       strlen(bad_userview_intersect_1) + 1, INPUT_MEM);
	fail_unless(map == NULL, "Map with duplicate lines");
	
	map = BuildUserViewMap(bad_userview_intersect_2,
			       strlen(bad_userview_intersect_2) + 1, INPUT_MEM);
	fail_unless(map == NULL, "IP intersect");
	
	map = BuildUserViewMap(bad_userview_intersect_3,
			       strlen(bad_userview_intersect_3) + 1, INPUT_MEM);
	fail_unless(map == NULL, "IP intersect");
	
	map = BuildUserViewMap(bad_userview_intersect_4,
			       strlen(bad_userview_intersect_4) + 1, INPUT_MEM);
	fail_unless(map == NULL, "PE intersect on edge");
	
	map = BuildUserViewMap(bad_userview_intersect_5,
			       strlen(bad_userview_intersect_5) + 1, INPUT_MEM);
	fail_unless(map == NULL, "PE intersect on edge");
	
	msx_set_debug(0);
}
END_TEST

/******************** Queries tests **************************/
/**************
 * Query map
 **************/
char *userview_query_map_1 =
"  15   192.168.0.15  25   \n"
"  1    192.168.0.1   10   \n"
"  80   192.168.0.80   25   \n"
"  120  192.168.2.1   10   \n"
"  150  192.168.2.15  25   \n"
"  200  192.168.3.1   10   \n"
"  215  192.168.3.15  25   \n"
"  301    192.168.4.1    10   \n"
"  312   192.168.4.11   10   \n"
"  325   192.168.4.21   10   \n"
"  340   192.168.4.31   10   \n"
"  441   192.168.4.41   10   \n"
"  451   192.168.4.51   25   \n";


START_TEST (test_query)
{
	mapper_t map;
	int res;
	char hostname[80];
	pe_t pe;
	mapper_node_info_t inf;
	
	//msx_set_debug(MAP_DEBUG);
	print_start("test_query");
	map = BuildUserViewMap(userview_query_map_1,
			       strlen(userview_query_map_1) + 1, INPUT_MEM);
	fail_unless(map != NULL, "Failed to build map object from userview_query_map_1");
	
	res = mapper_node2host(map, PE_SET(0,1), hostname);
	fail_unless(res == 1 && strcmp(hostname,"192.168.0.1") == 0, "node2host() failed");
	
	res = mapper_node2host(map, PE_SET(0,17), hostname);
	fail_unless(res == 1 && strcmp(hostname,"192.168.0.17") == 0, "node2host() failed");
	
	res = mapper_node2host(map, PE_SET(0,81), hostname);
	fail_unless(res == 1 && strcmp(hostname,"192.168.0.81") == 0, "node2host() failed");
	
	res = mapper_node2host(map, PE_SET(0,209), hostname);
	fail_unless(res == 1 && strcmp(hostname,"192.168.3.10") == 0, "node2host() failed");
	
	res = mapper_node2host(map, PE_SET(0,270), hostname);
	fail_unless(res == 0, "node2host() should have failed");
	
	
	//// host2node and adrr2node */
	res = mapper_hostname2node(map, "192.168.3.10", &pe); 
	fail_unless(res == 1 && pe == PE_SET(0,209), "hostname2node() failed");
		
	res = mapper_hostname2node(map, "192.168.4.75", &pe);
	fail_unless(res == 1 && pe == PE_SET(0,475), "hostname2node() failed");


	res = mapper_hostname2node(map, "192.168.44.10", &pe); 
	fail_unless(res == 0, "hostname2node() should faile"); 
	
	/// get_node_at_pos
	res = mapper_node_at_pos(map, 0, &inf);
	fail_unless(res == 1 && inf.ni_pe == PE_SET(0,1), "node_at_pos() failed");


	res = mapper_node_at_pos(map, 75, &inf);
	//if(res)
	//printf("Got pe %d %s\n", inf.ni_pe, inet_ntoa(inf.ni_addr));

	fail_unless(res == 1 && inf.ni_pe == PE_SET(0,155), "node_at_pos() failed");
	res = mapper_node_at_pos(map, 180, &inf);
	inf.ni_addr.s_addr = htonl(inf.ni_addr.s_addr);
	//if(res)
	//printf("Got pe %d %s\n", inf.ni_pe, inet_ntoa(inf.ni_addr));
	fail_unless(res == 1 && inf.ni_pe == PE_SET(0,451), "node_at_pos() failed");
	
	
	mapperDone(map);
	
}
END_TEST



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
  
  map = BuildUserViewMap(userview_query_map_1,
			 strlen(userview_query_map_1) + 1, INPUT_MEM);
  fail_unless(map != NULL, "Failed to build map object from userview_query_map_1");

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

char *crc_map_1 =
"# First crc map \n"
"  15   192.168.0.15  25   \n"
"  1    192.168.0.1   10   \n";


char *crc_map_1_1 =
"# Second crc map \n"
"  15   192.168.0.15  25   \n"
"  1    192.168.0.1   10   \n";

char *crc_map_2 =
"# First crc map \n"
"  15   192.168.0.15  26   \n"
"  1    192.168.0.1   10   \n";



START_TEST (test_map_crc32)
{
  mapper_t map1;
  mapper_t map1_1;
  mapper_t map2;

  print_start("test_map_crc32");
  //msx_set_debug(MAP_DEBUG);
  
  map1 = BuildUserViewMap(crc_map_1, strlen(crc_map_1) + 1, INPUT_MEM);
  fail_unless(map1 != NULL, "Failed to build map object from crc_map_1");

  map1_1 = BuildUserViewMap(crc_map_1_1, strlen(crc_map_1_1) + 1, INPUT_MEM);
  fail_unless(map1_1 != NULL, "Failed to build map object from crc_map_1_1");

  map2 = BuildUserViewMap(crc_map_2, strlen(crc_map_2) + 1, INPUT_MEM);
  fail_unless(map2 != NULL, "Failed to build map object from crc_map_2");
  
  unsigned int crc1, crc1_1, crc2;

  crc1   = mapperGetCRC32(map1);
  crc1_1 = mapperGetCRC32(map1_1);
  crc2   = mapperGetCRC32(map2);

  if(debug)
          fprintf(stderr, "Got crc %u : %u : %u\n", crc1, crc1_1, crc2);

  fail_unless(crc1 == crc1_1, "Maps 1 and 1_1 are the same except for comment");
  fail_unless(crc1 != crc2,   "Maps 1 and 2 differ in one node");

  mapperDone(map1);
  mapperDone(map1_1);
  mapperDone(map2);
  msx_set_debug(0);
  print_end();
}
END_TEST


char *stress_map_1 =
"  15   192.168.0.15  25   \n"
"  1    192.168.0.1   10   \n"
"  80   192.168.0.80   25   \n"
"  120  192.168.2.1   10   \n"
"  150  192.168.2.15  25   \n"
"  200  192.168.3.1   10   \n"
"  215  192.168.3.15  25   \n"
"  301    192.168.4.1    10   \n"
"  312   192.168.4.11   10   \n"
"  325   192.168.4.21   10   \n"
"  340   192.168.4.31   10   \n"
"  441   192.168.4.41   10   \n"
"  451   192.168.4.51   25   \n";


START_TEST (test_stress)
{
	mapper_t map;
        unsigned long    beforeMem[3], afterMem[3];
           
	//msx_set_debug(MAP_DEBUG);
	print_start("test_stress");

        beforeMem[0] = getMyRSS();
        beforeMem[1] = getMyMemSize();
        
        for(int i=0 ; i< 200; i++) {
             map = BuildUserViewMap(stress_map_1, strlen(stress_map_1) + 1, INPUT_MEM);
             fail_unless(map != NULL, "Failed to build map object from stress_map_1");
             mapperDone(map);
        }
        afterMem[0] = getMyRSS();
        afterMem[1] = getMyMemSize();
        printf("\n");
        printf("Before:  %-8ld %-8ld \n", beforeMem[0], beforeMem[1]);  
        printf("After:   %-8ld %-8ld \n", afterMem[0], afterMem[1]);  
        
}
END_TEST


/***************************************************/
Suite *mapper_suite(void)
{
  Suite *s = suite_create("Mapper");

  TCase *tc_core   = tcase_create("Core");
  TCase *tc_query  = tcase_create("Queries");
  TCase *tc_iter   = tcase_create("Iter");
  TCase *tc_stress = tcase_create("Stress");
  
  
  suite_add_tcase (s, tc_core);
  suite_add_tcase (s, tc_query);
//  suite_add_tcase (s, tc_compat);
  suite_add_tcase (s, tc_iter);
  suite_add_tcase (s, tc_stress);
  
  tcase_add_test(tc_core, test_create);
  tcase_add_test(tc_core, test_userview_builder_from_mem);
/*   tcase_add_test(tc_core, test_create_from_another); */
  tcase_add_test(tc_core, test_create_from_file); 
  tcase_add_test(tc_core, test_create_from_cmd); 

  tcase_add_test(tc_core, test_limits);
  tcase_add_test(tc_core, test_good_userview_maps); 
  tcase_add_test(tc_core, test_bad_userview_maps);
  tcase_add_test(tc_core, test_bad_userview_intersect);
  tcase_add_test(tc_core, test_map_crc32);
  
/*   // The queires  */
  tcase_add_test(tc_query, test_query); 

/*   // Compatability with old maps */
/*   tcase_add_test(tc_compat, test_compat); */

   // Map Iterator checking 
  tcase_add_test(tc_iter, test_map_iter_1); 
  //tcase_add_test(tc_iter, test_map_iter_2); 

  tcase_add_test(tc_core, test_stress);
  
  return s;
}


int main(int argc, char **argv)
{
  int nf;
  //struct in_addr addr;
  //in_addr_t ipaddr1, ipaddr2;
  
  Suite *s = mapper_suite();
  SRunner *sr = srunner_create(s);

  if(argc > 1 )
          debug = 1;
  //msx_set_debug(MAP_DEBUG);
  
  //ipaddr1 = inet_addr("192.168.0.11");
  //ipaddr2 = inet_addr("192.168.0.15");
  
  //ipaddr1 = ntohl(ipaddr1);
  //ipaddr2 = ntohl(ipaddr2);
  
  //printf("%ul %ul\n", ipaddr1, ipaddr2);
  //if(ipaddr1 > ipaddr2 )
  //	  printf("1 > 2\n");
  // else
  //	  printf("2 > 1\n");
  //return 1;
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
