/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

/******************************************************************************
 *
 * Author(s): Amar Lior
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <math.h>

#include <info.h>
#include <infoitems.h>
#include <infolib.h>
#include <infoxml.h>
#include <easy_args.h>
#include <info_iter.h>
#include <info_reader.h>

#include <msx_common.h>

#include <host_list.h>
#include <LoadConstraints.h>
#include <gossimon_config.h>

// If the difference between the loads of two nodes is less than
// this value, they are considered as having the same load
#define LOAD_DIFF_ENV_NAME   "EQUAL_LOAD_DIFF"
#define DEF_LOAD_INTERVAL    (5)

#define LC_DIR               GOSSIMON_CONFIG_DIR
#define LC_PATH              LC_DIR"/.gossimon-lc-info"
#define LC_TMP_PREFIX        LC_DIR"/.gossimon-lc-tmp-info"

#define DEF_TIME_CONSTRAINT  (3)

#define STD_SPEED (10000)

typedef enum {
	LB_ALGO_REG,
	LB_ALGO_2_CHOICE,
} lb_algorithm_t;

typedef struct _node_info_t {
	char           *name;
	struct in_addr  ip;
	int             load;
	int             totalMem;
	int             freeMem;
	int             speed;
	int             ncpus;
	int             prioLevel;
	
	int             assignedProcesses;
	int             assignedMemory;
	int             update;

} nodeInfo_t;


typedef struct {
	char         *pName;
	char          verbose;
	int           debug;
	
	char         *infoHost;
	int           infoPort;
	
	int           grid;
	int           procNum;
	int           memory;
	char        **machineList;
	int           machineListSize;
    
	nodeInfo_t   *nodesInfo;
	int           nodesNum;
	
	nodeInfo_t   *resultList;
	int           resultListSize;
	
	lb_algorithm_t lbAlgorithm;
	
	idata_t      *infoData;
	var_t        *loadVar;
	var_t        *speedVar;
	var_t        *ncpusVar;
	var_t        *totalMemVar;
	var_t        *freeMemVar;
	var_t        *prioLevelVar;
	
	int          equalLoadDiff; // Difference between same load nodes
	
	char         noLC;
	int          timeLCVal;
	int          processLCPid;
	char        *lcFile;
	loadConstraint_t lc;	
} bestnode_prop_t;

bestnode_prop_t BN;

void usage();
int  getNodesInfo();
void fixNodesInfo();
void printResults();
void setDefaultValues(int argc, char **argv, bestnode_prop_t *bnp);
int  buildMachineListFromStr(char *machineStr);
int  buildMachineListFromFile(char *machineFile);
int  findNameInList(char *name);
void printNodeInfo(nodeInfo_t *infos, int size);
int  bestNodesOfAll();
int  chooseBestNode(int *bestNodePos);
void updateChosenNodeLoad(int bestNodePos);
int  calcFutureLoad(nodeInfo_t *node, int procNum);
void loadLC();
int  applyNodesLC();
int  updateLC();

int set_server( void *void_str ) {
	BN.infoHost = strdup((char*) void_str);
	return 0;
}
int set_port( void *void_int ) {
	BN.infoPort  = *(int*)void_int ;
	return 0;
}

int set_grid( void *void_void ) {
	BN.grid  = 1 ;
	return 0;
}

int set_memory( void *void_int ) {
	BN.memory  = *(int*)void_int ;
	return 0;
}

int set_parallel( void *void_int ) {
	BN.procNum  = *(int*)void_int ;
	return 0;
}

int set_nolc( void *void_str ) {
	BN.noLC = 1;
	return 0;
}

int set_lcFile( void *void_str ) {
	BN.lcFile = strdup((char*) void_str);
	return 0;
}

int set_tlc( void *void_int ) {
	BN.timeLCVal  = *(int*)void_int ;
	return 0;
}

int set_plc( void *void_int ) {
	BN.processLCPid  = *(int*)void_int ;
	return 0;
}


int set_machines( void *void_str ) {
    char *machineStr;
    machineStr = strdup((char*) void_str);
    if(!buildMachineListFromStr(machineStr)) {
	fprintf(stderr, "Error: machine list is not leagal\n");
	return 1;
    }
    return 0;
}

int set_machineFile( void *void_str ) {
    char *machineFile;
    machineFile = strdup((char*) void_str);
    if(!buildMachineListFromFile(machineFile)) {
	fprintf(stderr, "Error: machine file is not leagal\n");
	return 1;
    }
    return 0;
}

int set_loadDiff( void *void_double ) {
	BN.equalLoadDiff  = *(double*)void_double ;
	return 0;
}

int set_twoChoice( void *void_str ) {
	BN.lbAlgorithm = LB_ALGO_2_CHOICE;
	return 0;
}


int set_debug( void *void_str ) {
	BN.debug = 1;
	return 0;
}

int set_verbose( void *void_str ) {
	BN.verbose = 1;
	return 0;
}


int show_copyright() {
     printf("infod-best version " GOSSIMON_VERSION_STR "\n" GOSSIMON_COPYRIGHT_STR "\n\n");
     exit(0);
}


void usage()
{
  fprintf(stderr,
          "%1$s version " GOSSIMON_VERSION_STR "\n"
          "\n"
          "Usage: %1$s -G{n} -h|--server <hostname> -d<load diff>-v\n"
          "\n"
          " Find the best available nodes in the cluster or the grid\n"
          "\n"
          "OPTIONS\n"
          "  -h, --server  <hostname> \n"
          "                  Connect to the given host name for retrieving information\n"
          //		"      --port <p>  Use alternative port number\n"
          "  -G              Choose also from grid nodes\n"
          "  -m <M>          Choose nodes with at least M Megabytes of free memory\n"
          "  -p <n>          Find the best n nodes, this option is for parallel programs\n"
          "      --lcfile <file>\n"
          "                  Use the given file as the load constraints information file\n"
          "      --tlc <timeout in sec> \n"
          "                  Add a timed load constraint to the nodes choosen. The timeout\n"
          "                  unit is in seconds. Each process is considered as pure CPU one\n"
          "      --plc pid   Add a process load constraint. As long the the given pid\n"
          "                  exists the load of the chosen node will be incremented by\n"
          "                  the load constraints\n"
          "      --machines <m1,m2,m3..>\n"
          "                  Choose the best node out of the given list\n"
          "      --machine-file <file-name>\n"
          "                  Take the list of possible nodes from the given file.\n"
          "                  Each line in the file represents one machine\n"
          "  -d <load-diff>  Ignore differences smaller than load-diff units\n"
          "  -v              Print verbose information when doing decisions\n"
          
          //		"      --power-of-two\n"
          //		"                  Use 2 choice algorithm\n"
          "     --copyright  Show copyright message\n"
          "     --help       This help message\n"
          , BN.pName);
  return;
}

struct argument cmdargs[] = {
	// Assignment modifiers
	{ ARGUMENT_FLAG | ARGUMENT_SHORT,       'G', NULL,       set_grid},
	{ ARGUMENT_NUMERICAL | ARGUMENT_SHORT,  'm', NULL,       set_memory},
	{ ARGUMENT_NUMERICAL | ARGUMENT_BOTH,   'p', "parallel", set_parallel},
	{ ARGUMENT_DOUBLE | ARGUMENT_SHORT,     'd', NULL,       set_loadDiff},

	// LC arguments
	{ ARGUMENT_FLAG   | ARGUMENT_FULL,       0, "nolc",      set_nolc},
	{ ARGUMENT_STRING | ARGUMENT_FULL,       0, "lcfile",    set_lcFile},
	{ ARGUMENT_NUMERICAL | ARGUMENT_FULL,    0, "tlc",       set_tlc},
	{ ARGUMENT_NUMERICAL | ARGUMENT_FULL,    0, "plc",       set_plc},

	// Machine lists
	{ ARGUMENT_STRING | ARGUMENT_FULL,       0 , "machines", set_machines},
	{ ARGUMENT_STRING | ARGUMENT_FULL,       0 , "machine-file", set_machineFile},

	// General
	//{ ARGUMENT_FLAG   | ARGUMENT_FULL, 0,     "power-of-two", set_twoChoice},
	{ ARGUMENT_FLAG   | ARGUMENT_FULL,       0 , "verbose",  set_verbose},
	{ ARGUMENT_FLAG   | ARGUMENT_SHORT,     'v', NULL,       set_verbose},
	{ ARGUMENT_FLAG   | ARGUMENT_FULL,       0 , "debug",    set_debug},
	{ ARGUMENT_STRING | ARGUMENT_FULL,      0,  "server",    set_server},
	{ ARGUMENT_STRING | ARGUMENT_SHORT,     'h', NULL,       set_server},
	//{ ARGUMENT_STRING | ARGUMENT_FULL,    0,     "port",   set_port},
        { ARGUMENT_FLAG   | ARGUMENT_FULL,       0 , "copyright", show_copyright},
	{ ARGUMENT_FLAG   | ARGUMENT_FULL,       0 , "help",     usage},
	{ ARGUMENT_USAGE_HANDLER , 0, "", usage},
	{ 0, 0, NULL, 0},
};


int main(int argc, char **argv)
{
	setDefaultValues(argc, argv, &BN);
	parse_command_line( argc, argv, cmdargs );
	
	if(!getNodesInfo()) {
		if(BN.verbose) fprintf(stderr, "Error getting information\n");
		return 1;
	}
	fixNodesInfo();
	
	if(!BN.noLC) {
		loadLC();
		applyNodesLC();
	}
	// Allocating memory for the results
	// Allocating space for all the candidates
	BN.resultList = malloc(sizeof(nodeInfo_t) * BN.procNum);
	if(!BN.resultList) {
	    fprintf(stderr, "Error: allocating memory for results\n");
	    return 1;
	}
	
	if(!bestNodesOfAll())
		return 1;

	if(!BN.noLC)
		if(!updateLC())
			return 1;
	printResults();
	//printf("%d\n", bestnode);
	return 0;
}

void setDefaultValues(int argc, char **argv, bestnode_prop_t *bnp) {
	char *x;
	
	bzero(bnp, sizeof(bestnode_prop_t));

	bnp->infoHost = "localhost";
	bnp->grid = 0;
	bnp->procNum = 1;
	bnp->lbAlgorithm = LB_ALGO_REG;
	bnp->debug = 0;
	bnp->memory = 0;
	bnp->verbose = 0;
	bnp->pName = strdup(argv[0]);
	if((x = strrchr(argv[0], '/')))
		bnp->pName = x+1;
	
	bnp->equalLoadDiff = DEF_LOAD_INTERVAL;
	bnp->resultList = NULL;
	bnp->resultListSize = 0;

	bnp->timeLCVal = DEF_TIME_CONSTRAINT;
	bnp->processLCPid = 0;
	bnp->noLC = 0;
	bnp->lcFile = strdup(LC_PATH);
	bnp->lc = NULL;
}

int cmpHostName(const void *a, const void *b)
{
    nodeInfo_t *nodeA = (nodeInfo_t *)a;
    nodeInfo_t *nodeB = (nodeInfo_t *)b;

    int lenA, lenB;
    lenA = strlen(nodeA->name);
    lenB = strlen(nodeB->name);

    if(lenA > lenB)
	return 1;
    if(lenB > lenA)
	return -1;
    
    return strcmp(nodeA->name, nodeB->name);

}

void printResults() {
	int i;

	// Sort the result list according to name
	qsort(BN.resultList, BN.resultListSize, sizeof(nodeInfo_t),
	      cmpHostName);

	if(BN.resultListSize > 0) {
	    for(i=0 ; i < BN.resultListSize ; i++) {
		printf("%s ", BN.resultList[i].name);
	    }
	    printf("\n");
	}
}
	

/** Get the requested info from infod and place it in the infoData;
    The infor is requested according to the class
 */
int getNodesInfo()
{
    idata_t           *data = NULL;	
    char              *desc = NULL;
    variable_map_t    *map  = NULL;	
    idata_iter_t   *iter = NULL;
    idata_entry_t  *cur  = NULL;
    node_info_t    *node = NULL;
    void           *ptr  = NULL;
    unsigned long   smallest_speed = -1, cur_speed = -1;
    unsigned long   best_load = -1, cur_load = -1;
    int             i, candidates;
    struct timeval  t;
    int             page_sz = getpagesize();
    
    // Getting description and building variables
    if( !( desc = infolib_info_description( BN.infoHost, MSX_INFOD_DEF_PORT )) ||
	!( map  = create_info_mapping( desc ))                  ||
	!( BN.loadVar     = get_var_desc( map , ITEM_LOAD_NAME ))   ||
	!( BN.speedVar    = get_var_desc( map , ITEM_SPEED_NAME )) ||
	!( BN.ncpusVar     = get_var_desc( map , ITEM_NCPUS_NAME ))  ||
	!( BN.totalMemVar  = get_var_desc( map , ITEM_TMEM_NAME)) ||
	!( BN.freeMemVar  = get_var_desc( map , ITEM_FREEPAGES_NAME)) ||
	!( BN.prioLevelVar  = get_var_desc( map , ITEM_PRIO_NAME)))
	    return 0;
    free(desc);
    
    // Getting the information vector
    data = infolib_all( BN.infoHost, MSX_INFOD_DEF_PORT );
    
    if(!data)
	return 0;
    BN.infoData = data;
    
    // Reading the information and filtering it
    gettimeofday( &t, NULL );
    srand( (t.tv_usec * getpid()) );
    
    /* Create an iterator */
    if( !(iter = idata_iter_init( BN.infoData ))) {
	if(BN.verbose) fprintf(stderr, "Error initializing iterator\n");
	return 0;
    }
    // First we count the number of possible candidates
    for(candidates=0 , cur = idata_iter_next(iter); cur != NULL ;
	     cur = idata_iter_next( iter ) )
	{
	    // Skeeping dead nodes
	    if( ( cur->valid == 0 ) ||
		!( cur->data->hdr.status & INFOD_ALIVE ))
		continue;
	    
	    // If we have a list of nodes checking if this node is
	    // part of the list
            //printf("Name %s\n", cur->name);
	    if(BN.machineListSize > 0  && !findNameInList(cur->name)) {
		cur->data->hdr.status &= ~(INFOD_ALIVE);
		continue;
	    }
	    
	    candidates++;
	}

    if(candidates == 0) {
	if(BN.verbose)
	    fprintf(stderr, "No candidates machines found, maybe machine list is not leagal\n");
	return 0;
    }
	  
    
    // Allocating space for all the candidates
    BN.nodesInfo = malloc(sizeof(nodeInfo_t) * candidates);
    if(!BN.nodesInfo)
	return 0;
    
    // Getting the candidates information
    idata_iter_rewind( iter );
    for(i=0 , cur = idata_iter_next(iter); cur != NULL ;
	cur = idata_iter_next( iter ) )
	{
	    // Only candiates will be alive
	    if( ( cur->valid == 0 ) ||
		!( cur->data->hdr.status & INFOD_ALIVE ))
		continue;

	    // Taking the information from the vector
	    node = cur->data;
	    ptr = node->data;
	    BN.nodesInfo[i].name = strdup(cur->name);
	    BN.nodesInfo[i].ip = node->hdr.IP;
	    BN.nodesInfo[i].load  = *((unsigned long*)(ptr + BN.loadVar->offset));
	    BN.nodesInfo[i].speed = *((unsigned long*)(ptr + BN.speedVar->offset));
	    BN.nodesInfo[i].ncpus = *((unsigned char*)(ptr + BN.ncpusVar->offset));

	    BN.nodesInfo[i].totalMem =
		    *(unsigned long*)	(ptr + BN.totalMemVar->offset );
	    BN.nodesInfo[i].totalMem /= (1024*1024)/page_sz;

	    BN.nodesInfo[i].freeMem =
		    *(unsigned long*)	(ptr + BN.freeMemVar->offset );
	    BN.nodesInfo[i].freeMem /= (1024*1024)/page_sz;

	    BN.nodesInfo[i].prioLevel =
		    *(unsigned short *)(ptr + BN.prioLevelVar->offset);
	    
	    // The results will be kept here 
	    BN.nodesInfo[i].assignedMemory = 0;
	    BN.nodesInfo[i].assignedProcesses = 0;
	    i++;
	    
	}
    BN.nodesNum = i;
    if(BN.debug) {
         printf("Candiates:\n");
         printNodeInfo(BN.nodesInfo, BN.nodesNum);
    }
    return 1;
}

/**
   Fixing the cluster nodes info to reflect that guest grid processes
   does not consume load and memory is free
**/ 
void fixNodesInfo() {
	int       i;

	for( i=0 ; i< BN.nodesNum ; i++ )
	{
		nodeInfo_t   *nodePtr = &BN.nodesInfo[i];
		
		if(MSX_IS_GRID_PRIO(nodePtr->prioLevel)) {
			nodePtr->load = 0;
			nodePtr->freeMem = nodePtr->totalMem * 0.9;
		}

	}
	
	if(BN.debug) {
		printf("After fixing nodes info:\n");
		printNodeInfo(BN.nodesInfo, BN.nodesNum);
	}
}

/**
   Applying the load constraints over our current retrieved information from
   infod.
**/
int applyNodesLC() {
	int       i;

	if(!BN.lc)
		return 0;
	
	for( i=0 ; i< BN.nodesNum ; i++ )
	{
		nodeLCInfo_t lcInfo;
		nodeInfo_t   *nodePtr = &BN.nodesInfo[i];
			
		if(!lcGetNodeLC(BN.lc, nodePtr->name, &lcInfo))
			continue;
		
		if(BN.debug) printf("Node %s has lc info\n", nodePtr->name);

		nodePtr->freeMem -= lcInfo.minMemory;
		if(nodePtr->freeMem < 0 )
			nodePtr->freeMem = 0;

		nodePtr->load = calcFutureLoad(nodePtr, lcInfo.minProcesses); 
	}
}


/** Find the best avilable node, according to cpu load considerations
    Infod is contacted and the vector is asked. According to the level
    requested the nodes are chooesn.

 @param  subst    A subset of pes we want to consider     
 @param  class    0 means local partition processes  > 1 means cluster + grid
                  processes
 @return The pe of the chosen node or 0 on error.
 */
int bestNodesOfAll()
{
    struct timeval  t;
    int             i;
    int             bestNodePos;
    
    gettimeofday( &t, NULL );
    srand( (t.tv_usec * getpid()) );
    
    // Choosing BN.procNum best nodes
    for(i=0; i<BN.procNum ; i++) {
	if(!chooseBestNode(&bestNodePos)) {
	    return 0;
	}
	BN.resultList[i] = BN.nodesInfo[bestNodePos];
	// Updating the best node properties to reflect the additional
	// process + memory which will be shortly assigned
	updateChosenNodeLoad(bestNodePos);
    }
    BN.resultListSize = BN.procNum;
    return 1;
}

void updateChosenNodeLoad(int bestNodePos) {
    BN.nodesInfo[bestNodePos].load = calcFutureLoad(&BN.nodesInfo[bestNodePos], 1);
    BN.nodesInfo[bestNodePos].freeMem -= BN.memory;
    
    BN.nodesInfo[bestNodePos].assignedProcesses += 1;
    BN.nodesInfo[bestNodePos].assignedMemory += BN.memory;
}

int calcFutureLoad(nodeInfo_t *node, int addedProcs) {
    int   futureLoad;
    futureLoad = node->load +
	    ((float)STD_SPEED/(float)node->speed)*
	    (((float)addedProcs*(float)100)/(float)node->ncpus);
    
    return futureLoad;
}

int chooseBestNode(int *bestNodePos)
{
    int             i, bestPos;
    unsigned long   smallest_speed = -1, cur_speed = -1;
    unsigned long   bestLoad = -1, cur_load = -1;
    int             pe = 0, num_best = 0, pos = 0;

    
    /* Get the best node, count the number of nodes with the same load */
    for( i=0 ; i< BN.nodesNum ; i++ )
    {
	
	int futureLoad = calcFutureLoad(&BN.nodesInfo[i], 1);

	if( (BN.nodesInfo[i].freeMem > BN.memory) &&
	    (( bestLoad == -1 ) || bestLoad > futureLoad ))
	{
	    bestLoad = futureLoad;
	    bestPos = i;
	}
    }
    
    // Should not happen but ...
    if( bestLoad == -1 ) {
	if(BN.verbose)
	    fprintf(stderr, "Could not find a suitable node (maybe too much memory required)\n");
	return 0;
    }
    
    /*
     * Get the number of nodes with load similiar to the the lowest load.
     */
    for( num_best = 0, i=0; i<BN.nodesNum ; i++) 
    {
	int futureLoad = calcFutureLoad(&BN.nodesInfo[i], 1);
	if( (BN.nodesInfo[i].freeMem > BN.memory) &&
	    (futureLoad - bestLoad) <= BN.equalLoadDiff )
	    num_best++;
    }

    // Taking a random node out of those who are in apropriate distance
    // from the best possible node
    pos = (rand()%num_best)+1;
    if(BN.verbose) fprintf(stderr, "Random pos %d num best %d\n",
			   pos , num_best);
    for( i=0; i<BN.nodesNum ; i++) 
    {
	int futureLoad = calcFutureLoad(&BN.nodesInfo[i], 1);
	
	if( (BN.nodesInfo[i].freeMem > BN.memory) &&
	    (futureLoad - bestLoad) <= BN.equalLoadDiff )
	    pos--;
	
	if( pos == 0 ) {
	    *bestNodePos = i;
	    break;
	}
    }
    
    return 1;
}


/****************************************************************************
 * Get the best machine from the infod according to load using the 2-choice
 * algorithm
 ***************************************************************************/
#define MAX_ITER (16)

/* int */
/* infod_2_choice( subst_pes_args_t* subst ) { */
	
/* 	idata_t *data = NULL; */
/* 	idata_iter_t *iter      = NULL; */
/* 	idata_entry_t     *cur  = NULL;  */
/* 	char              *desc = NULL; */
/* 	variable_map_t    *map  = NULL; */
/* 	var_t             *load = NULL; */
/* 	node_info_t *node = NULL; */
/* 	void        *ptr  = NULL; */
		
/* 	node_t first = 0, second = 0; */
/* 	unsigned long load1 = 0, load2 = 0, num = 0, i = 0, k = 0; */
/* 	struct timeval t; */

/* 	gettimeofday( &t, NULL ); */
/* 	srand( (t.tv_usec * getpid()) ); */
        
/* 	/\* Get the loads of all the machines *\/ */
/* 	if( subst ) */
/* 		data = infolib_subset_pes( NULL, MSX_INFOD_DEF_PORT, subst ); */
/* 	else */
/* 		data = infolib_all( NULL, MSX_INFOD_DEF_PORT ); */
	
/* 	if( !data || !data->num ) */
/* 		return 0; */
	
/* 	/\* Create the iterator *\/  */
/* 	if( !(iter = idata_iter_init( data ))) */
/* 		return 0; */

/* 	/\* get the position of the load *\/ */
/* 	if( !( desc = infolib_info_description( NULL, MSX_INFOD_DEF_PORT )) || */
/* 	    !( map  = create_info_mapping( desc )) || */
/* 	    !( load = get_var_desc( map , "load" ))) */
/* 		return 0; */
	
/* 	free( desc ); */

/* 	/\* get the number of active nodes *\/ */
/* 	idata_iter_rewind( iter ); */
/* 	for( num = 0, cur = idata_iter_next( iter ); (cur != NULL); */
/* 	     cur = idata_iter_next( iter )) { */
/* 		if( ( cur->valid != 0 ) && */
/* 		    ( cur->data->hdr.status & DS_MOSIX_UP )) */
/* 			num++; */
/* 	} */
	
/* 	if( num == 0 ) */
/* 		return 0; */
	
/* 	/\* get the load of the first choice *\/  */
/* 	for( k = 0; (k < MAX_ITER ) && ( first == 0 ); k++ ) { */
		
/* 		first = (rand() % num); */
/* 		idata_iter_rewind( iter ); */

/* 		for( i = 0, cur = idata_iter_next(iter); */
/* 		     (cur != NULL) && ( i< first ); */
/* 		     cur = idata_iter_next( iter )) { */
/* 			if( ( cur->valid == 0 ) || */
/* 			    !( cur->data->hdr.status & DS_MOSIX_UP )) */
/* 				continue; */
/* 			else */
/* 				i++; */
/* 		} */

/* 		if( (cur->valid == 1 ) && */
/* 		    (cur->data->hdr.status & DS_MOSIX_UP )) { */
/* 			node = cur->data; */
/* 			ptr  = node->data; */
/* 			load1 = *((unsigned long*)(ptr+load->offset)); */
/* 			first = cur->data->hdr.pe; */
/* 		} */
/* 		else */
/* 			first = 0; */
/* 	} */

/* 	/\* No node was found adequate *\/ */
/* 	if( first == 0 ) */
/* 		return 0; */
	
/* 	/\* only one active node *\/  */
/* 	if( num == 1 ) */
/* 		return first; */

/* 	/\* get the load of the second choice *\/ */
/* 	for( k = 0; */
/* 	     (k < MAX_ITER ) && ( ( second == 0 ) || (second == first )); */
/* 	     k++ ) { */
/* 		second = (rand() % num); */
/* 		idata_iter_rewind( iter ); */

/* 		for( i = 0, cur = idata_iter_next(iter); */
/* 		     (cur != NULL) && ( i< second ); */
/* 		     cur = idata_iter_next( iter )) { */
/* 			if( ( cur->valid == 0 ) || */
/* 			    !( cur->data->hdr.status & DS_MOSIX_UP )) */
/* 				continue; */
/* 			else */
/* 				i++; */
/* 		} */

/* 		if( (cur->valid == 1 ) && */
/* 		    (cur->data->hdr.status & DS_MOSIX_UP )) { */
/* 			node = cur->data; */
/* 			ptr  = node->data; */
/* 			load2 = *((unsigned long*)(ptr+load->offset)); */
/* 			second = cur->data->hdr.pe; */
		
/* 		} */
/* 		else */
/* 			second = 0; */
/* 	} */

/* 	if( second == 0 ) */
/* 		return first; */
	
/*        	if( abs( load1 - load2 ) < BN.equalLoadDiff ) { */
/* 		if( (rand()% 2)) */
/* 			return first; */
/* 		else */
/* 			return second; */

/* 	} */

/* 	return ( load1 < load2 )?first:second;  */

/* } */



void printNodeInfo(nodeInfo_t *infos, int size)
{
    int i;
    for (i=0; i<size ; i++) {
	    printf("Node %-10s l: %-5d  m:(%-5d / %-5d)  s: %-5d nc:%-3d level:%-5d\n",
		   infos[i].name, infos[i].load, infos[i].freeMem, infos[i].totalMem,
		   infos[i].speed, infos[i].ncpus, infos[i].prioLevel);
    }
}


int hostNamesEqual(char *a, char *b) {

     int aLen, bLen;
     int i;

     // Calculating the name until the first dot (.)
     for(i=0 ; a[i] ; i++) {
          if(a[i] == '.') {
               break;
          }
     }
     aLen = i;
     
     for(i=0 ; b[i] ; i++) {
          if(b[i] == '.') {
               break;
          }
     }
     bLen = i;

     if(aLen != bLen)
          return 0;
     if(strncmp(a, b, aLen) == 0)
          return 1;
     return 0;
          
}
int findNameInList(char *name) {
    int i;
    if(BN.machineListSize <= 0 )
	return 0;
    
    // Looking for name in the host list
    for(i=0 ; i<BN.machineListSize ; i++) {
         if(hostNamesEqual(BN.machineList[i], name)) {
              //printf("Name %s matches\n", name);
              return 1;
         }
    }
    return 0;
}


int buildMachineListFromStr(char *machineStr) {
    mon_hosts_t mh;
    int         i;
    
    mh_init(&mh);
    if(!mh_set(&mh, machineStr))
	return 0;
    if(BN.debug)
	mh_print(&mh);
    // Building the array of hosts names

    BN.machineListSize = mh_size(&mh);

    BN.machineList = malloc(sizeof(char*) * BN.machineListSize);
    for(i=0 ; i< BN.machineListSize ; i++) {
	//fprintf(stderr, "Size %d curr %s\n", mh_size(&mh),
	//mh_next(&mh));
	BN.machineList[i] = strdup(mh_next(&mh));
    }

    if(BN.debug)
	for(i=0 ; i< BN.machineListSize ; i++)
	    printf("List %d %10s\n", i, BN.machineList[i]);
    
    return 1;
    
}

int buildMachineListFromFile(char *machineFile) {

    FILE        *f = NULL;
    int          maxSize = 100, i;
    char         buff[513];
    char         machineName[50];
    char         rangeStr[512];
    mon_hosts_t  mh;
    
    
    if(!(f = fopen(machineFile, "r")))
            return 0;
    
    mh_init(&mh);
    
    while(fgets(buff, 512, f))
	if(buff[0] && buff[0] != '#' && buff[0] != '\n')
	{
	    buff[512] = '\0';
	    if (sscanf(buff, "%s", machineName) == 1)
	    {
		if(BN.debug)
		    printf("Got single name %s\n", machineName);
		
		if(!mh_add(&mh, machineName))
		{
		    if(BN.debug)
			printf("Failed setting host list from: %s\n", machineName);
		    goto failed;
		}
	    }
	    else {
		    if(BN.verbose)
			printf("Unknownd format\n");
                    goto failed;
	    }
	}

    BN.machineListSize = mh_size(&mh);
    
    BN.machineList = malloc(sizeof(char*) * BN.machineListSize);
    for(i=0 ; i< BN.machineListSize ; i++) {
	//fprintf(stderr, "Size %d curr %s\n", mh_size(&mh),
	//mh_next(&mh));
	BN.machineList[i] = strdup(mh_next(&mh));
    }
    
    if(BN.debug)
	for(i=0 ; i< BN.machineListSize ; i++)
	    printf("List %d %10s\n", i, BN.machineList[i]);

    mh_free(&mh);
    fclose(f);
    return 1;
    
 failed:
    if(f) fclose(f);
    mh_free(&mh);
    return 0;
}

/**
   Loading the load constraints information 
**/
void loadLC() {
	char        *lcBuff = NULL;
	int          lcFd;
	int          res;
	struct stat  statBuff;
	
	
	if(BN.lc)
		lcDestroy(BN.lc);


	if(stat(BN.lcFile, &statBuff) == -1) {
		if(errno == ENOENT) {
			// we start with an empty lc since it does not exists
			BN.lc = lcNew();
			if(BN.verbose) 
				fprintf(stderr, "No lc file %s found, creating new lc\n",
					BN.lcFile);
			return;
		}
		goto failed;
	}
	
	lcBuff = malloc(statBuff.st_size);
	if(!lcBuff)
		goto failed;
	
	lcFd = open(BN.lcFile, O_RDONLY);
	if(lcFd == -1) {
		goto failed;
	}
	
	res = read(lcFd, lcBuff, statBuff.st_size);
	if(res != statBuff.st_size) {
		goto failed;
	}
	
	BN.lc = buildLCFromBinary( lcBuff, statBuff.st_size);
	if(!BN.lc) {
		if(BN.verbose) fprintf(stderr, "Failed to build from binary (%d)\n",
				       (int)statBuff.st_size);
		goto failed;
	}
	lcVerify(BN.lc);
	if(BN.verbose)
		fprintf(stderr, "Loaded lc with %d hosts\n", lcHostNum(BN.lc));
	
	return;
 failed:
	if(BN.verbose) {
		fprintf(stderr, "Got failure in loadLc\n");
	}
	
	if(lcBuff) free(lcBuff);
	if(BN.lc) lcDestroy(BN.lc);
	BN.lc = NULL;
	return;
}

/**
   Updating the lc according to the choosen assignment
**/ 
int updateLC() {
	int    i;
	int    res;

	int    lcFd;
	int    lcBuffSize;
	char  *lcBuff;
	char   lcTmpPath[256];
	
// Need to go over each node and check if we need to insert it
	// to the LC file

	// Just a debug printout
	nodeInfo_t *nodePtr;

	if(BN.verbose)
		printf("\nUpdating LC:\n");
	for( i=0; i<BN.nodesNum ; i++) 
	{
		struct timeval duration;
		nodeLCInfo_t   lcInfo;
		
		duration.tv_usec = 0;
		duration.tv_sec = BN.timeLCVal;
		
		nodePtr = &BN.nodesInfo[i];
		if(BN.verbose && nodePtr->assignedProcesses > 0)
			printf("Node %s Assigned Proc %d  mem %d\n",
			       nodePtr->name, nodePtr->assignedProcesses,
			       nodePtr->assignedMemory);
		
		if(nodePtr->assignedProcesses > 0) {
			lcInfo.minMemory = nodePtr->assignedMemory;
			lcInfo.minProcesses = nodePtr->assignedProcesses;
			res = lcAddTimeConstraint(BN.lc, nodePtr->name,
					    NULL, &duration, &lcInfo);
			if(!res) {
				printf("Error updating load constraints info\n");
				return 0;
			}
		}
	}

	if(BN.verbose)
		lcPrintf(BN.lc);
	lcBuff = malloc(1024*16);
	if(!lcBuff) {
		fprintf(stderr, "Error allocating memory for lc\n");
		return 0;
	}

	lcBuffSize = writeLCAsBinary(BN.lc, lcBuff, 1024*16);
	if(lcBuffSize == 0 ) {
		fprintf(stderr, "Error while generating a binary lc\n");
		return 0;
	}
	
	sprintf(lcTmpPath, "%s.%d", LC_TMP_PREFIX, getpid());
	if(BN.debug) {
		printf("lc tmp name %s\n", lcTmpPath);
	}
	lcFd = open(lcTmpPath, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
	if(lcFd == -1) {
		fprintf(stderr, "Failed opening %s\n", lcTmpPath);
		return 0;
	}
	res = write(lcFd, lcBuff, lcBuffSize);
	if(res != lcBuffSize) {
		fprintf(stderr, "Error while writing tmp lc");
		return 0;
	}

	free(lcBuff);
	close(lcFd);

	res = rename(lcTmpPath, BN.lcFile);
	if(res == -1) {
		fprintf(stderr, "Error while renaming %s to %s\n",
			lcTmpPath, BN.lcFile);
		return 0;
	}
	
	
	return 1;
}
