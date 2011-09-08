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

#include <ModuleLogger.h>
#include <debug_util.h>
#include <pe.h>
#include <Mapper.h>
#include <MapperBuilder.h>
#include <infoVec.h>
#include <infoVecInternal.h>
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


char *info_desc = 
"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
"<local_info>"
"        <base name=\"tmem\"  type=\"unsigned long\"  unit=\"4KB\"/>"
"        <base name=\"speed\" type=\"unsigned long\"/>"
"</local_info>";



char *external_data_small =
" blabla 1 2 3 4 5\n"
" blabla 6 7 8 9 20\n";

char *external_data_big =
"GGGGGGGGGGGGGGG 99999999999999999999999999999999999999999\n"
"GGGGGGGGGGGGGGG 99999999999999999999999999999999999999999\n"
"GGGGGGGGGGGGGGG 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999\n"
"DDDDDDDDDDDDDDD 99999999999999999999999999999999999999999";

char *external_data_huge = NULL;

#define EXTERNAL_SMALL  (1)
#define EXTERNAL_BIG    (2)
#define EXTERNAL_HUGE   (3)

char infoBuff[4096];

typedef struct _test_data {
	unsigned long tmem;
	unsigned long speed;
        char          external[0];
} test_data_t;

void updateEntryPrio(ivec_t vec, char *ipStr, int prio) {
	char buff[250];
	node_info_t *node;
	test_data_t *data;
	int          n;
	
	// Creating an artifcial node structure
	node = (node_info_t *) buff;
	data = (test_data_t *) (buff + sizeof(node_info_t));
	inet_aton(ipStr, &(node->hdr.IP));
	node->hdr.pe = 1111;
	node->hdr.status = INFOD_ALIVE;
	node->hdr.psize = NODE_INFO_SIZE + sizeof(test_data_t);
	node->hdr.fsize = NODE_INFO_SIZE + sizeof(test_data_t);
	data->tmem  = 500;
	data->speed = 1000;
	   
	// Updating the vector
	gettimeofday(&node->hdr.time, NULL);
	n = infoVecUpdate(vec, node, NODE_INFO_SIZE + sizeof(test_data_t), prio);
	fail_unless(n!=0, "Failed to update vector");
}

void updateEntryPrioExternal(ivec_t vec, char *ipStr, int prio, int external_type) {
	node_info_t *node;
	test_data_t *data;
	int          n;

        int   external_size;
        char *external_ptr = NULL;

        fprintf(stderr, "Test....\n");
        switch(external_type) {
        case EXTERNAL_SMALL:
             external_size = strlen(external_data_small)+1;
             external_ptr = external_data_small;
             break;
        case EXTERNAL_BIG:
             external_size = strlen(external_data_big)+1;
             external_ptr = external_data_big;
             break;
        case EXTERNAL_HUGE:
             external_size = 32768;
             if(!external_data_huge) {
                  external_data_huge = (char *)malloc(32768);
                  memset(external_data_huge, 'd', 32768);
                  external_data_huge[32767] = '\0';
             }
             external_ptr = external_data_huge;
             break;
        default:
             exit(1);
        }

	// Creating an artifcial node structure
	node = (node_info_t *) infoBuff;
	data = (test_data_t *) &(node->data);
	inet_aton(ipStr, &(node->hdr.IP));
	node->hdr.pe = 3333;
	node->hdr.status = INFOD_ALIVE;
	node->hdr.psize = NODE_INFO_SIZE + sizeof(test_data_t);
	node->hdr.fsize = NODE_INFO_SIZE + sizeof(test_data_t) + external_size;
	data->tmem  = 550;
	data->speed = 3000;
        
        // Copying the external data to place
        fprintf(stderr, "Test 2....\n");
        memcpy(data->external, external_ptr, external_size);
        fprintf(stderr, "Test 3....\n");
	// Updating the vector
        fprintf(stderr, "Test 2....\n");
	gettimeofday(&node->hdr.time, NULL);
	n = infoVecUpdate(vec, node, node->hdr.fsize, prio);
	fail_unless(n!=0, "Failed to update vector");
}


void updateEntry(ivec_t vec, char *ipStr) {
	updateEntryPrio(vec, ipStr, 0);
}

void updateEntryExternal(ivec_t vec, char *ipStr, int external_type) {
	updateEntryPrioExternal(vec, ipStr, 0, external_type);
}




char *userview_map_1 = 
"1 192.168.0.1 5 \n";

char *userview_map_2 = 
"1    192.168.0.1 5 \n"
"10   192.168.1.1 5 \n"
"100  192.168.2.1 5 \n"
"1200 192.168.3.1 5 \n";



START_TEST (test_create)
{
   mapper_t map;
   ivec_t ivec;
   int n;
   struct in_addr ip;

   print_start("UserView Map from memory");
   //msx_set_debug(VEC_DEBUG);

   map = BuildUserViewMap(userview_map_1, strlen(userview_map_1) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 16, info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");
   infoVecPrint(stdout, ivec, 0);
   infoVecFree(ivec);

   mapperDone(map);


   
   map = BuildUserViewMap(userview_map_2, strlen(userview_map_2) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 16, info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");
   infoVecPrint(stdout, ivec, 0);
   infoVecFree(ivec);

   mapperDone(map);

   print_end();
   msx_set_debug(0);
}
END_TEST

char *userview_map_3 = 
"1    192.168.0.1 5 \n"
"10   192.168.1.1 5 \n"
"100  192.168.2.1 15 \n"
"1200 192.168.3.1 25 \n";


START_TEST (test_find)
{
   mapper_t map;
   ivec_t ivec;
   int n;
   struct in_addr ip;
   ivec_entry_t *e;
   int  index;
   
   print_start("infoVecFindByIP");
   //msx_set_debug(VEC_DEBUG);
   
   map = BuildUserViewMap(userview_map_3, strlen(userview_map_3) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 16, info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");
   //infoVecPrint(stdout, ivec, 0);
   inet_aton("192.168.0.2", &ip);
   e = infoVecFindByIP( ivec, &ip, &index );
   fail_unless(e != NULL, "Failed to find 192.168.0.2 in ivec");
   fail_unless(strcmp(e->name, "192.168.0.2") == 0, "Name of entry is not correct");

   inet_aton("192.168.3.22", &ip);
   e = infoVecFindByIP( ivec, &ip, &index );
   fail_unless(e != NULL, "Failed to find 192.168.3.22 in ivec");

   inet_aton("192.168.4.22", &ip);
   e = infoVecFindByIP( ivec, &ip, &index );
   fail_unless(e == NULL, "Found 192.168.4.22 in ivec where it is not there");

   
   infoVecFree(ivec);

   mapperDone(map);
     
   print_end();
   msx_set_debug(0);
}
END_TEST


char *userview_map_4 = 
"1    192.168.0.1 5 \n"
"10   192.168.1.1 3 \n";

START_TEST (test_updateInfoVec)
{
   mapper_t map;
   ivec_t ivec;
   int n;
   struct in_addr ip;
   ivec_entry_t *e;
   int  index;
   char buff[200];
   node_info_t  *node;
   test_data_t  *data;
   
   print_start("infoVecUpdate");
   //msx_set_debug(VEC_DEBUG);
   
   map = BuildUserViewMap(userview_map_4, strlen(userview_map_4) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 16, info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");
   infoVecPrint(stdout, ivec, 0);

   // Creating an artifcial node structure
   node = (node_info_t *) buff;
   data = (test_data_t *) (buff + sizeof(node_info_t));
   inet_aton("192.168.0.1", &node->hdr.IP);
   node->hdr.psize = NODE_INFO_SIZE + sizeof(test_data_t);
   node->hdr.fsize = NODE_INFO_SIZE + sizeof(test_data_t);
   data->tmem  = 500;
   data->speed = 1000;
   inet_aton("192.168.0.1", &ip);
   //e = infoVecFindByIP( ivec, &ip, &index );
   //data = (test_data_t*)e->info->data;
   //printf("Got %p\n", data);

   // Updating the vector
   gettimeofday(&node->hdr.time, NULL);
   n = infoVecUpdate(ivec, node, NODE_INFO_SIZE + sizeof(test_data_t), 0);
   fail_unless(n != 0 , "Failed to update vector");
   e = infoVecFindByIP( ivec, &ip, &index );
   fail_unless(e != NULL, "Failed to find 192.168.3.22 in ivec");
   data = (test_data_t*)e->info->data;
   fail_unless(data->tmem == 500, "The data in the vector is not what we inserted");
   //printf("Got %ld\n", data->tmem);
   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST


char *test_stats_map = 
"1    192.168.0.1 3 \n"
"10   192.168.1.1 3 \n";

START_TEST (test_infoVecStats)
{
   mapper_t map;
   ivec_t ivec;
   int n;
   struct in_addr ip;
   char buff[200];
   node_info_t  *node;
   test_data_t  *data;
   infod_stats_t stats;
   
   print_start("infoVecStats");
   msx_set_debug(VEC_DEBUG);
   
   map = BuildUserViewMap(test_stats_map, strlen(test_stats_map) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 16, info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");
   infoVecPrint(stdout, ivec, 0);

   for(int i=0 ; i < 3 ; i++) {
	   char ipStr[50];
	   // Creating an artifcial node structure
	   node = (node_info_t *) buff;
	   data = (test_data_t *) (buff + sizeof(node_info_t));
	   sprintf(ipStr, "192.168.0.%d", i+1); 
	   inet_aton(ipStr, &node->hdr.IP);
	   node->hdr.psize = NODE_INFO_SIZE + sizeof(test_data_t);
	   node->hdr.fsize = NODE_INFO_SIZE + sizeof(test_data_t);
           node->hdr.status = INFOD_ALIVE;
           data->tmem  = 500;
	   data->speed = 1000;
	   
	   // Updating the vector
	   gettimeofday(&node->hdr.time, NULL);
	   n = infoVecUpdate(ivec, node, NODE_INFO_SIZE + sizeof(test_data_t), 0);
	   fail_unless(n!=0, "Failed to update vector");
   }
   infoVecStats(ivec, &stats);
   fail_unless(stats.total_num == 6, "Total node num is not 6");
   //fprintf(stderr, "ALive %d\n", stats.num_alive);
   fail_unless(stats.num_alive == 3, "Alive number is not 3");

   // Testing get random nodes
   struct in_addr randIP;

   for(int i= 0; i < 10 ; i++) {
	   infoVecRandomNode(ivec, 1, &randIP);
	   //if(debug)
	   //   printf("Got random from alive %s\n", inet_ntoa(randIP));
	   fail_unless(strcmp("192.168.0.2", inet_ntoa(randIP))==0 ||
		       strcmp("192.168.0.3", inet_ntoa(randIP))==0,
		       "Random node is not from alive nodes");

	   infoVecRandomNode(ivec, 0, &randIP);
	   //if(debug)
	   //	   printf("Got random from all %s\n", inet_ntoa(randIP));
	   fail_unless(strcmp("192.168.0.2", inet_ntoa(randIP))==0 ||
		       strcmp("192.168.0.3", inet_ntoa(randIP))==0 ||
		       strcmp("192.168.1.1", inet_ntoa(randIP))==0 ||
		       strcmp("192.168.1.2", inet_ntoa(randIP))==0 ||
		       strcmp("192.168.1.3", inet_ntoa(randIP))==0, 
		       "Random node is not from alive nodes");
   }
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST


char *test_vec_punish = 
"1    192.168.0.1 3 \n"
"10   192.168.1.1 3 \n";

START_TEST (test_infoVecPunish)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n, index;
   ivec_entry_t     *e;
   struct in_addr    ip;
      
   print_start("infoVecPunish");
   //msx_set_debug(VEC_DEBUG);
   
   map = BuildUserViewMap(test_vec_punish, strlen(test_vec_punish) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 16, info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");
   infoVecPrint(stdout, ivec, 0);

   updateEntry(ivec, "192.168.0.1");
   updateEntry(ivec, "192.168.0.2");
   updateEntry(ivec, "192.168.0.3");
   inet_aton("192.168.0.2", &ip);
   n = infoVecPunish(ivec, &ip, INFOD_DEAD_CONNECT);
   fail_unless(n != 0, "Failed to punish a valid node in vector");
   // Checking the status of the node
   e = infoVecFindByIP(
	   ivec, &ip, &index );
   fail_unless(e!=NULL, "Failed to find the dead node");
   fail_unless(e->isdead && e->info->hdr.cause == INFOD_DEAD_CONNECT,
	       "Node is not dead or cause is not correct");

   // Punishing a non existent node
   inet_aton("192.168.100.2", &ip);
   n = infoVecPunish(ivec, &ip, INFOD_DEAD_CONNECT);
   fail_unless(n == 0, "Failed to punish a valid node in vector");

   // Punishing an already dead node
   inet_aton("192.168.1.2", &ip);
   n = infoVecPunish(ivec, &ip, INFOD_DEAD_CONNECT);
   fail_unless(n != 0, "Failed to punish a valid node in vector");
   e = infoVecFindByIP( ivec, &ip, &index );
   fail_unless(e!=NULL, "Failed to find the already dead node");
   fail_unless(e->isdead && e->info->hdr.cause == INFOD_DEAD_CONNECT,
	       "Node is not dead or cause is not correct");

   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST

char *test_vec_get_window = 
"1    192.168.0.1 10 \n";

START_TEST (test_infoVecGetWindow)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n;
//   ivec_entry_t     *e;
   struct in_addr    ip;
      
   print_start("infoVecGetWindow");
   //msx_set_debug(VEC_DEBUG | WIN_DEBUG);
  
   
   map = BuildUserViewMap(test_vec_get_window, strlen(test_vec_get_window) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.3", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 4 , info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");


   updateEntry(ivec, "192.168.0.1");
   updateEntry(ivec, "192.168.0.2");
   updateEntry(ivec, "192.168.0.3");
   updateEntry(ivec, "192.168.0.4");
   updateEntry(ivec, "192.168.0.5");
   infoVecPrint(stdout, ivec, 0);
   infoVecPrint(stdout, ivec, 2);
   int   winSize; 
   char *winBuff;
   winBuff = infoVecGetWindow(ivec, &winSize, 1);
   fail_unless(winBuff != NULL, "Failed getting window to send\n");

   info_msg_t        *msg  = (info_msg_t*)winBuff;
   info_msg_entry_t  *infoEntry = NULL;
   //printf("Window size %d\n", msg->num);
   fail_unless(msg->num == 4, "Window size is not as expected");
   infoEntry = msg->data;
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.3") == 0,
	       "First entry is not local node");

   infoEntry = (info_msg_entry_t *) ((char *)msg->data + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.5") == 0,
	       "Second entry is not 192.168.0.5");

   infoEntry = (info_msg_entry_t *) ((char *)infoEntry + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.4") == 0,
	       "Third entry is not 192.168.0.4");

   
   updateEntryPrio(ivec, "192.168.0.1", 10);
   updateEntryPrio(ivec, "192.168.0.2", 5);

   // Adding some nodes with higher prio
   infoVecPrint(stdout, ivec, 2);

   winBuff = infoVecGetWindow(ivec, &winSize, 1);
   fail_unless(winBuff != NULL, "Failed getting window to send\n");
   msg  = (info_msg_t*)winBuff;
   fail_unless(msg->num == 4, "Window size is not as expected");
   infoEntry = msg->data;
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.3") == 0,
	       "First entry is not local node (high prio)");

   infoEntry = (info_msg_entry_t *) ((char *)msg->data + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.1") == 0,
	       "Second entry is not 192.168.0.1 (high prio)");

   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST


START_TEST (test_infoVecGetUptoAgeWindow)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n;
//   ivec_entry_t     *e;
   struct in_addr    ip;
      
   print_start("infoVecGetUptoAgeWindow");
   //msx_set_debug(VEC_DEBUG);
   
   map = BuildUserViewMap(test_vec_get_window, strlen(test_vec_get_window) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.3", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_UPTOAGE, 2 , info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");


   updateEntry(ivec, "192.168.0.1");
   updateEntry(ivec, "192.168.0.2");
   updateEntry(ivec, "192.168.0.3");
   updateEntry(ivec, "192.168.0.4");
   updateEntry(ivec, "192.168.0.5");
   infoVecPrint(stdout, ivec, 0);
   infoVecPrint(stdout, ivec, 2);
   int   winSize; 
   char *winBuff;
   winBuff = infoVecGetWindow(ivec, &winSize, 1);
   fail_unless(winBuff != NULL, "Failed getting window to send\n");

   info_msg_t        *msg  = (info_msg_t*)winBuff;
   info_msg_entry_t  *infoEntry = NULL;
   //fail_unless(msg->num == 4, "Window size is not as expected");

   infoEntry = msg->data;
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.3") == 0,
	       "First entry is not local node");

   infoEntry = (info_msg_entry_t *) ((char *)msg->data + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.5") == 0,
	       "Second entry is not 192.168.0.5");

   infoEntry = (info_msg_entry_t *) ((char *)infoEntry + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.4") == 0,
	       "Third entry is not 192.168.0.4");


   // Checking the up to age where only 2 entries should be in the vector
   // The local and .5
   sleep(2);
   updateEntry(ivec, "192.168.0.5");
   winBuff = infoVecGetWindow(ivec, &winSize, 1);
   fail_unless(winBuff != NULL, "Failed getting window to send\n");

   msg  = (info_msg_t*)winBuff;
   infoEntry = NULL;
   fail_unless(msg->num == 2, "Window size is not as expected");

   infoEntry = msg->data;
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.3") == 0,
	       "First entry is not local node (after 2 sec)");

   infoEntry = (info_msg_entry_t *) ((char *)msg->data + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.5") == 0,
	       "Second entry is not 192.168.0.5 (after 2 sec)");

   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST


char *test_vec_use_remote_window = 
"1    192.168.0.1 10 \n";

START_TEST (test_infoVecUseRemoteWindow)
{
   mapper_t          map, map2;
   ivec_t            ivec, ivec2;
   int               n;
//   ivec_entry_t     *e;
   int                index;
   struct in_addr    ip;
      
   print_start("infoVecUseRemoteWindow");
   //msx_set_debug(VEC_DEBUG | WIN_DEBUG);
   
   //===== Building first map and vector
   map = BuildUserViewMap(test_vec_use_remote_window,
			  strlen(test_vec_use_remote_window) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.3", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 4 , info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");

   //===== Building second map and vector
   map2 = BuildUserViewMap(test_vec_use_remote_window,
			  strlen(test_vec_use_remote_window) + 1, INPUT_MEM);
   fail_unless(map2 != NULL, "Failed to create map object");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map2, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec2 = infoVecInit(map2, 500, INFOVEC_WIN_FIXED, 4 , info_desc, 0);
   fail_unless(ivec2 != NULL, "Failed to create info vector");

   //infoVecPrint(stdout, ivec, 0);
   //infoVecPrint(stdout, ivec, 2);
   // Setting the first window entries
   updateEntry(ivec, "192.168.0.1");
   updateEntry(ivec, "192.168.0.2");
   updateEntry(ivec, "192.168.0.3");
   updateEntry(ivec, "192.168.0.4");
   updateEntry(ivec, "192.168.0.5");

   // Getting the window from the first vector
   int   winSize; 
   char *winBuff;
   infoVecPrint(stdout, ivec, 2);
   winBuff = infoVecGetWindow(ivec, &winSize, 1);
   fail_unless(winBuff != NULL, "Failed getting window to send\n");
   
   //infoVecPrint(stdout, ivec2, 0);
   // Giving the window to the second vector
   n = infoVecUseRemoteWindow(ivec2, winBuff, winSize);
   infoVecPrint(stdout, ivec2, 0);
   infoVecPrint(stdout, ivec2, 2);
   
   fail_unless(n != 0, "Failed using remote window\n");

   winBuff = infoVecGetWindow(ivec2, &winSize, 1);
   fail_unless(winBuff != NULL, "Failed getting window from vec2\n");

   // Checking that the window was updated correctly
   info_msg_t        *msg  = (info_msg_t*)winBuff;
   info_msg_entry_t  *infoEntry = NULL;
   fail_unless(msg->num == 4, "Window size is not as expected");
   infoEntry = msg->data;
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.1") == 0,
	       "First entry in vec2 is not local node");

   infoEntry = (info_msg_entry_t *) ((char *)msg->data + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.5") == 0,
	       "Second entry in vec2 is not 192.168.0.5");

   // Checking that the vector was updated correctly
   // infoVecPrint(stdout, ivec2, 0);
   inet_aton("192.168.0.5", &ip);
   ivec_entry_t  *e = infoVecFindByIP( ivec2, &ip, &index );
   fail_unless(e != NULL, "Failed to find 192.168.0.5 in ivec");
   fail_unless(e->isdead == 0, "192.168.0.5 should be alive\n");
   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST






char *test_vec_queries = 
"1     192.168.0.1  10 \n"
"30    192.168.0.30 10 \n"
"50    192.168.0.50 10 \n";

START_TEST (test_infoVecQueries)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n;
//   int               index;
   struct in_addr    ip;
      
   print_start("infoVecQueries");
   //msx_set_debug(VEC_DEBUG);

   //===== Building first map and vector
   map = BuildUserViewMap(test_vec_queries, strlen(test_vec_queries) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.3", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 4 , info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");

   // Setting the first window entries
   updateEntry(ivec, "192.168.0.1");
   updateEntry(ivec, "192.168.0.2");
   updateEntry(ivec, "192.168.0.3");
   updateEntry(ivec, "192.168.0.4");
   updateEntry(ivec, "192.168.0.5");
   updateEntry(ivec, "192.168.0.51");
   updateEntry(ivec, "192.168.0.52");
   
   
   // Doing queries
   ivec_entry_t **resVec;

   // AllEntries
   resVec = infoVecGetAllEntries(ivec);
   fail_unless(resVec != NULL, "Failed to gett all vector entries");
//   for(i=0 ; i < infoVecGetSize(ivec) ; i++) {
   fail_unless(strcmp(resVec[0]->name, "192.168.0.1") == 0,
	       "First entry in allEntries query is not correct");
//   }

   // By IP list
   struct in_addr ipList[4];
   inet_aton("192.168.0.51",  &ipList[0]);
   inet_aton("192.168.0.1",   &ipList[1]);
   inet_aton("192.168.1.100", &ipList[2]);
   inet_aton("192.168.0.31",  &ipList[3]);

   resVec = infoVecGetEntriesByIP(ivec, ipList, 4);
   fail_unless(strcmp(resVec[0]->name, "192.168.0.51") == 0, "51 is not first");
   fail_unless(strcmp(resVec[1]->name, "192.168.0.1") == 0, "1 is not second");
   fail_unless(resVec[2] == NULL , "1.100 should not exists");
   fail_unless(strcmp(resVec[3]->name, "192.168.0.31") == 0, "31 is not first");
   fail_unless(resVec[3]->isdead, "31 is not dead");
	

   // Packing the replay of byIps
   char *resBuff;
   int   resSize = 0;
   resBuff = infoVecPackQueryReplay(resVec, 4, NULL, &resSize);
   fail_unless(resBuff != NULL, "Failed to pack result vector");

   info_replay_t *infoRpl = (info_replay_t *) resBuff;
   fail_unless(infoRpl->num = 4, "Number of info_replay_entry is not 4");
   info_replay_entry_t *infoEnt = infoRpl->data;
   fail_unless(strcmp(infoEnt->name, "192.168.0.51") ==0,
	       "First entry in packed buffer is not 51");
   fail_unless(strcmp(inet_ntoa(infoEnt->data->hdr.IP), "192.168.0.51") == 0,
	       "IP of first packed entry is not 51");
   infoEnt = (info_replay_entry_t *)((char *)infoRpl->data + infoEnt->size);
   fail_unless(strcmp(infoEnt->name, "192.168.0.1") ==0,
	       "Second entry in packed buffer is not 1");

   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST


char *test_vec_stress = 
"1      192.168.0.1 200 \n"
"301    192.168.1.1 200 \n";
//"600    192.168.2.1 200 \n";
//"900    192.168.3.1 200 \n";


START_TEST (test_infoVecStress)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n;
//   int               index;
   struct in_addr    ip;
      
   print_start("infoVecStress");
   //msx_set_debug(VEC_DEBUG);

   //===== Building first map and vector
   map = BuildUserViewMap(test_vec_stress, strlen(test_vec_stress) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.3", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 32 , info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");

   for(int i=0 ; i < 1000 ; i++) {
	   struct in_addr randIP;

	   infoVecRandomNode(ivec, 0, &randIP);
	   updateEntry(ivec, inet_ntoa(randIP));
	    // Getting the window from the first vector
	   int   winSize; 
	   char *winBuff;
	   winBuff = infoVecGetWindow(ivec, &winSize, 1);
	   fail_unless(winBuff != NULL, "Failed getting window to send\n");

	   //free(winBuff);

   }
   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST


char *test_vec_stress2 = 
"1      192.168.0.1 20 \n"
"301    192.168.1.1 20 \n";
//"600    192.168.2.1 200 \n";
//"900    192.168.3.1 200 \n";


START_TEST (test_infoVecStress2)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n;
   //   int               index;
   struct in_addr    ip;
   unsigned long    beforeMem[3], afterMem[3];
      
   print_start("infoVecStress2");
   //msx_set_debug(VEC_DEBUG);

   //===== Building first map and vector
   map = BuildUserViewMap(test_vec_stress, strlen(test_vec_stress) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.3", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   beforeMem[0] = getMyRSS();
   beforeMem[1] = getMyMemSize();
   beforeMem[2] = checkVmSize();

   for(int i=0 ; i < 10 ; i++)
   {
     ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 32 , info_desc, 0);
        fail_unless(ivec != NULL, "Failed to create info vector");
        
        infoVecFree(ivec);
   }
   afterMem[0] = getMyRSS();
   afterMem[1] = getMyMemSize();
   afterMem[2] = checkVmSize();
   
   printf("Before:  %-8ld %-8ld %-8ld\n", beforeMem[0], beforeMem[1], beforeMem[2]);  
   printf("After:   %-8ld %-8ld %-8ld\n", afterMem[0], afterMem[1], afterMem[2]);  
   
   
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST




char *test_vec_random = 
"1      192.168.0.1 4 \n";



START_TEST (test_infoVecRandom)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n;
//   int               index;
   struct in_addr    ip;
      
   print_start("infoVecRandom");
   //msx_set_debug(VEC_DEBUG);

   //===== Building first map and vector
   map = BuildUserViewMap(test_vec_random, strlen(test_vec_random) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 32 , info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");

   // Checking that not matter what the random node will not be the local node
   for(int i=0 ; i < 100 ; i++) {
	   struct in_addr randIP;
	   
	   n = infoVecRandomNode(ivec, 0, &randIP);
	   fail_unless(n>0, "infoVecRandomNode failed");
	   //printf("Got %s\n", inet_ntoa(randIP));
	   fail_unless(strcmp("192.168.0.1", inet_ntoa(randIP)) != 0,
		       "Random node was local node");
	   
	   n = infoVecRandomNode(ivec, 1, &randIP);
	   fail_unless(n == 0, "infoVecRandomNode succeded but should fail, since there"
		       "are no live nodes");
   }

   // This time we have live nodes and we see that the local node is again not chosen

   updateEntry(ivec, "192.168.0.1");
   updateEntry(ivec, "192.168.0.2");
   updateEntry(ivec, "192.168.0.3");
   for(int i=0 ; i < 100 ; i++) {
	   struct in_addr randIP;
	   
	   n = infoVecRandomNode(ivec, 0, &randIP);
	   fail_unless(n>0, "infoVecRandomNode failed");
	   //printf("Got %s\n", inet_ntoa(randIP));
	   fail_unless(strcmp("192.168.0.1", inet_ntoa(randIP)) != 0,
		       "Random node was local node");
	   
	   n = infoVecRandomNode(ivec, 1, &randIP);
	   fail_unless(n > 0, "infoVecRandomNode failed but there are live nodes");
	   fail_unless(strcmp("192.168.0.1", inet_ntoa(randIP)) != 0,
		       "Random node was local node");
	   fail_unless(strcmp("192.168.0.4", inet_ntoa(randIP)) != 0,
		       "Random node was a dead node");
	   
   }


   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST

char *test_vec_age_measure = 
"1      192.168.0.1 4 \n";


/* START_TEST (test_infoVecAgeMeasure) */
/* { */
/*    mapper_t          map; */
/*    ivec_t            ivec; */
/*    int               n; */
/* //   ivec_entry_t     *e; */
/*    struct in_addr    ip; */
      
/*    print_start("infoVecAgeMeasure"); */
/* //   msx_set_debug(VEC_DEBUG); */
   
/*    map = BuildUserViewMap(test_vec_age_measure, strlen(test_vec_age_measure) + 1, INPUT_MEM); */
/*    fail_unless(map != NULL, "Failed to create map object"); */

/*    inet_aton("192.168.0.1", &ip); */
/*    n = mapperSetMyIP(map, &ip); */
/*    fail_unless(n==1, "Setting my IP in mapper"); */

/*    ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 4 , info_desc, 0); */
/*    fail_unless(ivec != NULL, "Failed to create info vector"); */


/*    updateEntry(ivec, "192.168.0.1"); */
/*    updateEntry(ivec, "192.168.0.2"); */
/*    updateEntry(ivec, "192.168.0.3"); */
/*    updateEntry(ivec, "192.168.0.4"); */

/*    ivec_age_measure_t im; */
/*    infoVecSetAgeMeasureSamplesNum(ivec, 4); */
/*    for(int i=0 ; i< 4 ; i++) { */
/* 	   infoVecDoAgeMeasure(ivec); */
/* 	   infoVecGetAgeMeasure(ivec, &im); */
/* 	   if(debug) printf("sample %d, avg %.4f\n", */
/*                             im.im_sampleNum, im.im_avgAge); */
/* 	   fail_unless(im.im_avgAge < i+1, "Running avg is wrong");  */
	   
/* 	   sleep(1); */
/*    } */
/*    // Clearing the age measure and doing a new one  */
/*    infoVecClearAgeMeasure(ivec); */
/*    infoVecDoAgeMeasure(ivec); */
/*    infoVecGetAgeMeasure(ivec, &im); */
/*    fail_unless(im.im_avgAge > 4, "Running avg is wrong");  */

/*    if(debug) */
/*            printf("hererrererererer\n"); */
/*    infoVecFree(ivec); */
/*    mapperDone(map); */
/*    print_end(); */
/*    msx_set_debug(0); */
/* } */
/* END_TEST */

START_TEST (test_infoVecOldest)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n;
//   ivec_entry_t     *e;
   struct in_addr    ip;
      
   print_start("infoVecOldest");
   //msx_set_debug(VEC_DEBUG);
   
   map = BuildUserViewMap(test_vec_age_measure, strlen(test_vec_age_measure) + 1, INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.1", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 4 , info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");


   updateEntry(ivec, "192.168.0.1");
   n = infoVecOldestNode(ivec, &ip);
   fail_unless(n == 0, "OldestNode should have failed (only local node alive)");
   
   updateEntry(ivec, "192.168.0.2");
   updateEntry(ivec, "192.168.0.3");
   updateEntry(ivec, "192.168.0.4");

   n = infoVecOldestNode(ivec, &ip);
   fail_unless(strcmp("192.168.0.2", inet_ntoa(ip)) == 0,
		       "Oldest node is not correct");
   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST



char *test_vec_get_window_external = 
"1    192.168.0.1 10 \n";

START_TEST (test_infoVecGetWindowExternal)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n;
//   ivec_entry_t     *e;
   struct in_addr    ip;

   print_start("infoVecGetWindowExternal");
   if(debug)
           msx_set_debug(VEC_DEBUG | WIN_DEBUG);
  
   
   map = BuildUserViewMap(test_vec_get_window_external,
                          strlen(test_vec_get_window_external) + 1,
                          INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.3", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");
   
   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, 4 , info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");
   
   updateEntryExternal(ivec, "192.168.0.1", EXTERNAL_SMALL);
   updateEntryExternal(ivec, "192.168.0.2", EXTERNAL_BIG);
   updateEntryExternal(ivec, "192.168.0.3", EXTERNAL_SMALL);
   updateEntryExternal(ivec, "192.168.0.4", EXTERNAL_BIG);
   updateEntryExternal(ivec, "192.168.0.5", EXTERNAL_SMALL);
   infoVecPrint(stdout, ivec, 0);
   infoVecPrint(stdout, ivec, 2);
   
   int   winSize; 
   char *winBuff;
   winBuff = infoVecGetWindow(ivec, &winSize, 1);
   fail_unless(winBuff != NULL, "Failed getting window to send\n");
   
   info_msg_t        *msg  = (info_msg_t*)winBuff;
   info_msg_entry_t  *infoEntry = NULL;
   //printf("Window size %d\n", msg->num);
   fail_unless(msg->num == 4, "Window size is not as expected");

   //************** The 192.168.0.3 entry
   infoEntry = msg->data;
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.3") == 0,
	       "First entry is not local node");
   
   test_data_t *test_data;
   test_data = (test_data_t *) infoEntry->data->data;

   //printf("Got external:\n%s\n", test_data->external);
   fail_unless(strcmp(test_data->external, external_data_small) == 0,
               "External data in local node is not correct\n");
                     
   //************** The 192.168.0.5 entry
   infoEntry = (info_msg_entry_t *) ((char *)msg->data + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.5") == 0,
	       "Second entry is not 192.168.0.5");

   test_data = (test_data_t *) infoEntry->data->data;
   
   fail_unless(strcmp(test_data->external, external_data_small) == 0,
               "External data in 0.5 node is not correct\n");

   //************** The 192.168.0.4 entry
   infoEntry = (info_msg_entry_t *) ((char *)infoEntry + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.4") == 0,
	       "Third entry is not 192.168.0.4");

   test_data = (test_data_t *) infoEntry->data->data;
   
   fail_unless(strcmp(test_data->external, external_data_big) == 0,
               "External data in 0.4 node is not correct\n");
   
   //**************** changing the data *******************
   updateEntryExternal(ivec, "192.168.0.1", EXTERNAL_BIG);
   updateEntryExternal(ivec, "192.168.0.2", EXTERNAL_SMALL);
   updateEntryExternal(ivec, "192.168.0.3", EXTERNAL_BIG);
   updateEntryExternal(ivec, "192.168.0.4", EXTERNAL_SMALL);
   updateEntryExternal(ivec, "192.168.0.5", EXTERNAL_BIG);

   infoVecPrint(stdout, ivec, 0);
   infoVecPrint(stdout, ivec, 2);
 
   winBuff = infoVecGetWindow(ivec, &winSize, 1);
   fail_unless(winBuff != NULL, "Failed getting window to send\n");
   
   msg  = (info_msg_t*)winBuff;
   infoEntry = NULL;
   //printf("Window size %d\n", msg->num);
   fail_unless(msg->num == 4, "Window size is not as expected");

   //************** The 192.168.0.3 entry
   infoEntry = msg->data;
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.3") == 0,
	       "First entry is not local node");
   
   test_data = (test_data_t *) infoEntry->data->data;

   //printf("Got external:\n%s\n", test_data->external);
   fail_unless(strcmp(test_data->external, external_data_big) == 0,
               "External data in local node is not correct\n");
                     
   //************** The 192.168.0.5 entry
   infoEntry = (info_msg_entry_t *) ((char *)msg->data + infoEntry->size);
   //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP));
   fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.5") == 0,
	       "Second entry is not 192.168.0.5");

   test_data = (test_data_t *) infoEntry->data->data;
   
   fail_unless(strcmp(test_data->external, external_data_big) == 0,
               "External data in 0.5 node is not correct\n");

  
   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST


char *test_vec_get_window_big = 
"1    192.168.0.1 20 \n";

START_TEST (test_infoVecGetWindowBig)
{
   mapper_t          map;
   ivec_t            ivec;
   int               n;
   struct in_addr    ip;

   print_start("infoVecGetWindowExternal");
   if(debug)
        msx_set_debug(VEC_DEBUG | WIN_DEBUG);
  
   
   map = BuildUserViewMap(test_vec_get_window_big,
                          strlen(test_vec_get_window_big) + 1,
                          INPUT_MEM);
   fail_unless(map != NULL, "Failed to create map object");

   inet_aton("192.168.0.3", &ip);
   n = mapperSetMyIP(map, &ip);
   fail_unless(n==1, "Setting my IP in mapper");
   
   int winSize = 16;

   ivec = infoVecInit(map, 500, INFOVEC_WIN_FIXED, winSize , info_desc, 0);
   fail_unless(ivec != NULL, "Failed to create info vector");
  
   for(int i=0 ; i < winSize ; i++) {
        char nameBuff[32];
        sprintf(nameBuff, "192.168.0.%d", i+1);
        updateEntryExternal(ivec, nameBuff, EXTERNAL_HUGE);
   }

   infoVecPrint(stdout, ivec, 0);
   infoVecPrint(stdout, ivec, 2);
   

   char *winBuff;
   winBuff = infoVecGetWindow(ivec, &winSize, 1);
   fail_unless(winBuff != NULL, "Failed getting window to send\n");
   
   info_msg_t        *msg  = (info_msg_t*)winBuff;
   info_msg_entry_t  *infoEntry = NULL;
   printf("Window size %d\n", msg->num);
   fail_unless(msg->num == 16, "Window size is not as expected");

   /* //\************** The 192.168.0.3 entry */
   /* infoEntry = msg->data; */
   /* fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.3") == 0, */
   /*             "First entry is not local node"); */
   
   /* test_data_t *test_data; */
   /* test_data = (test_data_t *) infoEntry->data->data; */

   /* //printf("Got external:\n%s\n", test_data->external); */
   /* fail_unless(strcmp(test_data->external, external_data_small) == 0, */
   /*             "External data in local node is not correct\n"); */
                     
   /* //\************** The 192.168.0.5 entry */
   /* infoEntry = (info_msg_entry_t *) ((char *)msg->data + infoEntry->size); */
   /* //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP)); */
   /* fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.5") == 0, */
   /*             "Second entry is not 192.168.0.5"); */

   /* test_data = (test_data_t *) infoEntry->data->data; */
   
   /* fail_unless(strcmp(test_data->external, external_data_small) == 0, */
   /*             "External data in 0.5 node is not correct\n"); */

   /* //\************** The 192.168.0.4 entry */
   /* infoEntry = (info_msg_entry_t *) ((char *)infoEntry + infoEntry->size); */
   /* //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP)); */
   /* fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.4") == 0, */
   /*             "Third entry is not 192.168.0.4"); */

   /* test_data = (test_data_t *) infoEntry->data->data; */
   
   /* fail_unless(strcmp(test_data->external, external_data_big) == 0, */
   /*             "External data in 0.4 node is not correct\n"); */
   
   /* //\**************** changing the data ******************* */
   /* updateEntryExternal(ivec, "192.168.0.1", EXTERNAL_BIG); */
   /* updateEntryExternal(ivec, "192.168.0.2", EXTERNAL_SMALL); */
   /* updateEntryExternal(ivec, "192.168.0.3", EXTERNAL_BIG); */
   /* updateEntryExternal(ivec, "192.168.0.4", EXTERNAL_SMALL); */
   /* updateEntryExternal(ivec, "192.168.0.5", EXTERNAL_BIG); */

   /* infoVecPrint(stdout, ivec, 0); */
   /* infoVecPrint(stdout, ivec, 2); */
 
   /* winBuff = infoVecGetWindow(ivec, &winSize, 1); */
   /* fail_unless(winBuff != NULL, "Failed getting window to send\n"); */
   
   /* msg  = (info_msg_t*)winBuff; */
   /* infoEntry = NULL; */
   /* //printf("Window size %d\n", msg->num); */
   /* fail_unless(msg->num == 4, "Window size is not as expected"); */

   /* //\************** The 192.168.0.3 entry */
   /* infoEntry = msg->data; */
   /* fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.3") == 0, */
   /*             "First entry is not local node"); */
   
   /* test_data = (test_data_t *) infoEntry->data->data; */

   /* //printf("Got external:\n%s\n", test_data->external); */
   /* fail_unless(strcmp(test_data->external, external_data_big) == 0, */
   /*             "External data in local node is not correct\n"); */
                     
   /* //\************** The 192.168.0.5 entry */
   /* infoEntry = (info_msg_entry_t *) ((char *)msg->data + infoEntry->size); */
   /* //printf("IP %s\n", inet_ntoa(infoEntry->data->hdr.IP)); */
   /* fail_unless(strcmp(inet_ntoa(infoEntry->data->hdr.IP), "192.168.0.5") == 0, */
   /*             "Second entry is not 192.168.0.5"); */

   /* test_data = (test_data_t *) infoEntry->data->data; */
   
   /* fail_unless(strcmp(test_data->external, external_data_big) == 0, */
   /*             "External data in 0.5 node is not correct\n"); */

  
   
   infoVecFree(ivec);
   mapperDone(map);
   print_end();
   msx_set_debug(0);
}
END_TEST


/***************************************************/
Suite *InfoVec_suite(void)
{
  Suite *s = suite_create("InfoVec");

  TCase *tc_core = tcase_create("Core");
  TCase *tc_query = tcase_create("Queries");
  TCase *tc_stress = tcase_create("Stress");
  TCase *tc_external = tcase_create("External");
  
  suite_add_tcase (s, tc_core);
  suite_add_tcase (s, tc_query);
  suite_add_tcase (s, tc_stress);
  suite_add_tcase (s, tc_external);
  
  tcase_set_timeout (tc_query, 10);

  /* tcase_add_test(tc_core, test_create); */
  /* tcase_add_test(tc_core, test_find); */
  /* tcase_add_test(tc_core, test_updateInfoVec); */
  /* tcase_add_test(tc_core, test_infoVecPunish); */
  /* tcase_add_test(tc_core, test_infoVecGetWindow); */
  /* tcase_add_test(tc_core, test_infoVecGetUptoAgeWindow); */
  /* tcase_add_test(tc_core, test_infoVecUseRemoteWindow); */
  /* tcase_add_test(tc_core, test_infoVecRandom); */
  
  /* tcase_add_test(tc_query, test_infoVecStats); */
  /* tcase_add_test(tc_query, test_infoVecQueries); */
  /* //tcase_add_test(tc_query, test_infoVecAgeMeasure); */
  /* tcase_add_test(tc_query, test_infoVecOldest); */
  
  /* tcase_add_test(tc_stress, test_infoVecStress); */

  /* tcase_add_test(tc_stress, test_infoVecStress2); */

  /* tcase_add_test(tc_external, test_infoVecGetWindowExternal); */
  tcase_add_test(tc_external, test_infoVecGetWindowBig);
  
  return s;
}


int main(int argc, char **argv)
{
  int nf;

  if(argc > 1 )
          debug = 1;


  //printf("ddddd\n");
  Suite *s = InfoVec_suite();
  SRunner *sr = srunner_create(s);
  mlog_init();
  mlog_addOutputFile("/dev/tty", 1);
  mlog_addOutputSyslog("INFOD", LOG_PID, LOG_DAEMON);
  mlog_registerColorFormatter((color_formatter_func_t)sprintf_color);
  
 
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


