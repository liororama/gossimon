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



/** @defgroup mapper The mapper module
 *  This is the mapper object module
 *
   Definitions and rules:

   The pe is 24 bits and we use 9bits to describe the cluster id (512) and 15
   bits to describe the inside cluster pe.

   By any means those number should not be hard coded and part of a macro
   PE_GET_CLUSTER_ID
   PE_GET_NODE_ID

   The partition id is also a max of node id to allow a maximum of one partition
   for each node is a fully pupolated cluster. so its max must be same as the
   node_id length.

   Frequent queries:

   mapper_node2xxxxx  Resolving information about a single node. So we need a
   fast way to find a single node information where the key is the pe

   mapper_xxxx2node   Finding the node number based on the address

   Data structure:

   Array of clusters, where each cluster points to a linked list of ranges of
   machine where all the machines in a single range has the same parameters
   (such as partition id)

   The array of clusters is sorted by the cluster_id so a specific cluster
   can be found using a binary search. If the requested cluster id is small
   we should decide to perform a linear search in that case.

   
   The linked list of ranges (partitions) inside a cluster is to be searched
   lineary.

   The mapper uses the distance_graph object for parsing the friend lines for
   setting different distances between the clusters or partition than then
   regular distance model (Described fully in distance_graph.h)
   
   
 *  @{
 */


#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Internal includes
#include <mem_cache.h>
#include <msx_error.h>

#include <msx_common.h>
#include <msx_debug.h>
#include <parse_helper.h>
#include <pe.h>
#include <distance_graph.h>


// Maximum number of ranges this map support
#define   DEFAULT_MAX_RANGES    (350)

/** range object struct
 * Contain all the range information
 */
typedef struct _range_entry
{
	int              partitionId;
	struct in_addr   base_addr;  ///< holds the basee address in host byte order
	int              base;
	int              count;   // Total nodes in range
	
	struct _range_entry *next;
} range_entry_t;

#define SAME_ZONE       (0)
#define DIFFERENT_ZONE  (-1)
#define NOT_SET_ZONE    (-2)

#define SAME_ZONE_STR         "same"
#define DIFFERENT_ZONE_STR    "not_same"


#include "mapper.h"


typedef struct _cluster_entry
{
	int              clusterId;
	int              nrNodes;    // Total nodes in cluster
	int              zone;       // The network zone the cluste belong to
	// If the zone is 0 then no proximity is used all are in the same
	// distance. IF -1 is used then all the nodes in this cluster are in a
	// different proximity, and commpression will always be used
	
	range_entry_t    *rangeList;
} cluster_entry_t;

/** mapper object struct
 * Contain all the mapper object needs 
 */
struct mapper_struct
{
	mem_cache_t      rangeCache;  // A cache object for the pe ranges 
	dist_graph_t     distGraph;
	int              defaultZone; // The default zone to use
	char             errorMsg[256];
	
	mapper_map_type  mapType;     // Type of map old/new
	
	int              nrClusters;  // Total number of clusters
	int              maxClusters; // Maximum number of allowed cluster
	int              nrNodes;        // Total nodes in map
	int              nrClusterNodes; // Total nodes in local cluster
	int              nrPartitionNodes;// Total nodes in local partition 
	
	int              nrRanges;   // Number of ip ranges this mapper contain
	pe_t             maxPe;      // Maximum pe in map
	
	int              myPe;
	int              myClusterId;
	int              myPartitionId;
	
	cluster_entry_t  *myClusterEnt;
	range_entry_t    *myRangeEnt;
	
	cluster_entry_t  *clusterArr; // Array of all clusters
};



struct mapper_iter_struct {
	struct mapper_struct  *map;
	mapper_iter_type_t      type;
	
	pe_t                   currNodeId;
	pe_t                   partitionId;
	int                    currClusterPos;
	cluster_entry_t       *currClusterEnt;
	range_entry_t         *currRangeEnt;
};


// A global map which currently points to the map we are working on;
mapper_t glob_map;

typedef enum
{
	MAP_VALID,
	MAP_NOT_VALID,
	MAP_SYNTAX_NOT_VALID,
} map_parse_status_t;


// Static functions declarations
static int set_as_text(const char *buff, int size);
static int set_as_xml(const char *buff, int size);
static int parse_range(char *range_ptr, int *b, char *hn, int *c);

static cluster_entry_t *get_cluster_entry(pe_t cluster_id);
static int is_cluster_exists(int cluster_id);
static void print_zone_info(char *buff);
static void print_cluster_entry(char *buff, cluster_entry_t *ce, mapper_print_t t);
static void print_range_entry(char *buff, range_entry_t *re, mapper_print_t t);
static int add_cluster_range(cluster_entry_t *ce, int part_id, int base,
			     char *host_name, int count);
static int set_clusters_zone(int zone_id, char *zone_ptr);
static int is_map_valid();
static pe_t resolve_my_pe_by_addr();
static int is_my_pe_in_map();
static int clusters_ips_intersect(cluster_entry_t *ce1, cluster_entry_t *ce2);
static int ranges_ip_intersect(range_entry_t *re1, range_entry_t *re2);
static int ranges_nodeId_intersect(range_entry_t *re1, range_entry_t *re2);
static int is_cluster_ranges_valid(cluster_entry_t *ce);

static void sort_cluster_arr();
static void update_general_info();
static int range_to_mosixnet(range_entry_t *re, pe_t cluster_id,
			     struct mosixnet *mnet, int min_guest_dist,
			     int max_guest_dist, int max_mig_dist);
static int set_mapper_node_info(cluster_entry_t *ce, range_entry_t *re,
				pe_t pe, mapper_node_info_t *node_info);
		
/** Init function
 * @param max_ranges   Number of maximum node ranges to support.
 *                     if equal 0 the default is used
 * @return An initialized mapper object
 */
mapper_t mapper_init(int max_ranges) {
	mapper_t map;
	
	
	// Taking care of max_ranges leagal values
	if(max_ranges < 0 )
		return 0;
	if(max_ranges == 0)
		max_ranges = DEFAULT_MAX_RANGES;
	if(max_ranges > MAX_NODES)
		max_ranges = MAX_NODES;

	// Allocating the struct
	if(!(map = malloc(sizeof(struct mapper_struct))))
		return NULL;

	map->mapType = MOSIX_MAP_UM;
	
	map->nrClusters = 0;
	map->nrNodes = 0;
	map->maxClusters = MAX_CLUSTERS;

	map->myPe = 0;
	map->myClusterId = 0;
	map->myPartitionId = 0;
	
	// Allocating the cluster array
	map->clusterArr = malloc(sizeof(cluster_entry_t) * map->maxClusters);
	if(!map->clusterArr) return NULL;
	
	bzero(map->clusterArr, sizeof(cluster_entry_t) * map->maxClusters);

	// Allocating the rangeCache object
	map->rangeCache = mem_cache_create(max_ranges, sizeof(range_entry_t));
	if(!map->rangeCache)
		return NULL;
	map->distGraph = dist_graph_init(NULL);
	if(!map->distGraph)
		return NULL;

	sprintf(map->errorMsg, "No error\n");
	return map;
}


mapper_t mapper_init_from_file(const char *path, pe_t my_pe)
{
	mapper_t    map = NULL;
	if(!(map = mapper_init(0)))
		return NULL;
	
	mapper_set_my_pe(map, my_pe);
	if(!mapper_set_from_file(map, path))
		return NULL;
	return map;
}

mapper_t mapper_init_from_mem(const char *map_buff, const int len, pe_t my_pe)
{
	mapper_t    map = NULL;
	if(!(map = mapper_init(0)))
		return NULL;
	
	mapper_set_my_pe(map, my_pe);
	if(!mapper_set_from_mem(map, map_buff, len))
		return NULL;
	return map;
}

mapper_t mapper_init_from_cmd(const char *map_cmd, pe_t my_pe) {

	mapper_t    map = NULL;
	if(!(map = mapper_init(0)))
		return NULL;
	
	mapper_set_my_pe(map, my_pe);
	if(!mapper_set_from_cmd(map, map_cmd))
		return NULL;
	return map;
}


/** Destroy the map object
 * Destroy the map object and free all its taken memory
 * @param  map  The mapper object to destroy
 * @return  0   on error
 * @return  1   If all went well
 */
int mapper_done(mapper_t map)
{
	if(!map)
		return 0;

	mem_cache_destroy(map->rangeCache);
	dist_graph_done(map->distGraph);
	free(map->clusterArr);
	free(map);
	return 1;
}

/** Returning the map type
 */
mapper_map_type mapper_get_map_type(mapper_t map)
{
	if(!map)
		return MOSIX_MAP_NOT_VALID;
	return map->mapType;
}

/** Printing a human readable error message to buff. This function should be
    called after a bad
 */
char *mapper_error(mapper_t map)
{
	if(!map) {
		return NULL;
	}
	return map->errorMsg;
}


/** Setting the pe of the local node
 * This bypass finding the local address of the node and looking for it in the
 * map. This is used when the node primary ip is not in the map (its
 * secondary is)
 *
 * @param  map      A mapper object
 * @param  my_pe    The local pe to set
 * @return 0 On error (if the pe is already set)
 * @return 1 On succes
 */
int mapper_set_my_pe(mapper_t map, pe_t my_pe)
{
	if(!map)
		return 0;
	// Not setting my pe if it is already set.
	if(map->myPe)
		return 0;

	map->myPe = my_pe;
	return 1;
}

pe_t mapper_get_my_pe(mapper_t map)
{
	if(!map) return 0;
	return map->myPe;
}

pe_t mapper_get_my_cluster_id(mapper_t map)
{
	if(!map) return 0;
	return map->myClusterId;
}
pe_t mapper_get_my_partition_id(mapper_t map)
{
	if(!map) return 0;
	return map->myPartitionId;
}

/* Section sizes */
int mapper_get_node_num_at_dist(mapper_t map, int dist)
{
	node_id_t         from, to;
	cluster_entry_t  *ce;
	range_entry_t    *re;
	int               i, nodes = 0, range_dist;
	
	if(!map) return 0;
	if(dist < SAME_PARTITION_MAX_DIST || dist > INFINITY_DIST)
		return 0;	

	node_id_set(&to, map->myClusterId, map->myPartitionId);
	for(i=0 ; i < map->nrClusters ; i++) {
		ce = &(map->clusterArr[i]);
		for(re = ce->rangeList ; re ; re=re->next)
		{
			node_id_set(&from, ce->clusterId, re->partitionId);
			range_dist = dist_graph_get_dist(map->distGraph, &from, &to);
			if(range_dist <= dist) 
				nodes += re->count;
		}
	}
	return nodes;
}

int mapper_get_total_node_num(mapper_t map) {
	if(!map) return 0;
	return map->nrNodes;
}
int mapper_get_cluster_node_num(mapper_t map)
{
	if(!map) return 0;
	return map->nrClusterNodes;
}
int mapper_get_partition_node_num(mapper_t map)
{
	if(!map) return 0;
	return map->nrPartitionNodes;
}

/** Return the number of ip ranges this mapper containes. This is usfull to do
 * before calling mapper_get_kernel_map so the size of the needed buffer can be
 * calculated in advace
 @param map  Mapper object
 @return Number of ip ranges
 */
int mapper_get_ranges_num(mapper_t map)
{
	if(!map)
		return 0;
	return map->nrRanges;
}
/** Returning the maximal pe
 */
pe_t mapper_get_max_pe(mapper_t map)
{
	if(!map)
		return 0;
	return map->maxPe;
}



int mapper_set_from_cmd(mapper_t map, const char *map_cmd)
{
	FILE *cmd_output = NULL;
	char *buff = NULL, *ptr;
	int   res = 0;
	int   step_size = 2048;
	int   size = step_size;   
	int   read_size = 0;

	debug_lg(MAP_DEBUG, "Setting from cmd: %s\n", map_cmd);
	if(!(buff = malloc(size))) {
		debug_lr(MAP_DEBUG, "Error allocating memory for command output\n");
		goto failed;
	}
	if(!(cmd_output = popen(map_cmd, "r")))
	{
		debug_lr(MAP_DEBUG, "popen failed for %s\n", map_cmd);
		goto failed;
	}

	ptr = buff;
	while(1) {
		res = fread(ptr, 1, step_size, cmd_output);
		if(res == step_size) {
			size += step_size;
			if(!(buff = (char *) realloc(buff, size))) {
				debug_lr(MAP_DEBUG, "Error reallocating cmd buff\n");
				goto failed;
			}
			ptr+= strlen(ptr);
		}
		else
			break;
		read_size += res;
	}
	
	res = mapper_set_from_mem(map, buff, read_size);
	debug_lg(MAP_DEBUG, "Setting from cmd res =  %d\n", res);
	
 failed:
	pclose(cmd_output);
	free(buff);
	return res;
}


/** Setting the mapper from a file. The whole file is read into memory
    and then parsed.
 @param map       The mapper object
 @param filename  File name to use
 @return  0 on error 1 on succes
*/
int mapper_set_from_file(mapper_t map, const char* filename)
{
	int fd = -1;
	struct stat s;
	char *buff = NULL;
	int   res = 0;
	
	if(stat(filename, &s) == -1)
		goto failed;

	if(!(buff = malloc(s.st_size+1)))
		goto failed;
	
	if((fd = open(filename, O_RDONLY)) == -1)
	goto failed;
    
	if(read(fd, buff, s.st_size) != s.st_size)
		goto failed;
	
	buff[s.st_size]='\0';
	
	res = mapper_set_from_mem(map, buff, s.st_size+1);
	
 failed:
	free(buff);
	close(fd);
	return res;
}


/** Setting the mapper from a memory buff
 * @param map       The mapper object
 * @param map_buff  The buffer holding the map
 * @param len       Lenght of buffer
 * @return 0 on error 1 on succes
 */
int mapper_set_from_mem(mapper_t map, const char* map_buff, const int len)
{
	int    res;
	pe_t   my_pe;
	glob_map = map;

	//First parsing the memory as text if not syntax valid then as xml
	res = set_as_text(map_buff, len);
	if(res == MAP_SYNTAX_NOT_VALID)
		res = set_as_xml(map_buff, len);
	if(res == MAP_NOT_VALID)
		return 0;

	// Checking map validity
	if(!is_map_valid())
		return 0;
	// After calling this the cluster arr is sorted by cluster id so we
	// can use binary sort on it. And also the ranges are sorted by PE
	sort_cluster_arr();
	
	// Taking care of the local node pe
	if(map->myPe == 0)
	{
		my_pe = resolve_my_pe_by_addr();
		if(my_pe == 0)
			return 0;
		map->myPe = my_pe;
	} else if(map->myPe > 0) {
		if(!is_my_pe_in_map()) 
			return 0;
	}
	else {
		// This is for cases where we got negative pe which should be
		// only for syntactiacl testing of the map
		return 1;
	}
	// The map is valid and the local node is there so we update all the
	// general information such as total number of nodes and so on.
	update_general_info();
	return 1;
}

// Coying the first line from data to buff up to buff len characters.
// If no line after buff_len characters than we force a line.
// We assume that line seperator is "\n" and that data is null terminated
// We return a pointer to the next characters after the first newline we detect
// in data.
char *buff_get_line(char *buff, int buff_len, char *data)
{
    int size_to_copy;
    char *ptr;
    
    if(buff_len < 1)
	return NULL;

    // Skeeping spaces at the start
    while(isspace(*data))
	    data++;
    
    ptr = index(data, '\n');
    if(!ptr)
    {
	// we assume that data is one line and we copy it to buff
	strncpy(buff,  data, buff_len -1);
	buff[buff_len - 1] = '\0';
	return NULL;
    }

    // we found a line and we copy it
        
    size_to_copy = ptr - data;
    if(size_to_copy >= buff_len)
	size_to_copy = buff_len -1 ;
    memcpy(buff, data, size_to_copy);
    buff[size_to_copy] = '\0';
    return (++ptr);
}

/** Setting the map from a buffer, treating it as  txt formatted (not xml)
 * @param buff     The buffer containing the map
 * @param size     Size of buffer
 * @return a map_parse_status_t enum to indicate the result
 */
int set_as_text(const char *buff, int size)
{
    int linenum=0;
    char *buff_cur_pos = NULL;
    int current_cluster_id = 0;
    int current_part_id = 0;
    cluster_entry_t *cluster_ptr = NULL;
    int base, count;
    char *tmp_ptr;
    int saw_cluster_def = 0;
    int map_is_new = 0;
    int map_is_old = 0;

    int saw_zone_def = 0;
    int default_zone = DIFFERENT_ZONE;
    int zone_id;
    const char *edge_prefix;
    char line_buff[200];
    char host_name[80];
    char default_zone_str[10];

    buff_cur_pos = (char*) buff;
    if(!buff_cur_pos)
	    return MAP_NOT_VALID;
    
    edge_prefix = dist_graph_get_prefix(glob_map->distGraph);
    
    while( buff_cur_pos && !(*buff_cur_pos == '\0') )
    {
	linenum++;
	buff_cur_pos = buff_get_line(line_buff, 200, buff_cur_pos);

	//debug_ly(MAP_DEBUG, "Processing line: '%s'\n", line_buff);
	// Skeeing comments in map
	if(line_buff[0] == '#')
	{
		debug_lg(MAP_DEBUG, "\t\tcomment\n");
		continue;
	}
	// Skeeping empty lines
	if(strlen(line_buff) == 0)
		continue;

	if(sscanf(line_buff, "cluster=%d", &current_cluster_id) == 1) {
		int i;
		//debug_lg(MAP_DEBUG,"\t\tfound cluster %d\n", current_cluster_id);
		if(map_is_old) {
			debug_lr(MAP_DEBUG, "Can not mix old map with new map\n");
			sprintf(glob_map->errorMsg, "Can not mix old map format with new one\n");
			return MAP_SYNTAX_NOT_VALID;
		}

		// First check if the cluster already exists if yes then error
		if(is_cluster_exists(current_cluster_id)) {
			debug_lr(MAP_DEBUG, "Cluster %d already exists\n",
				 current_cluster_id);
			sprintf(glob_map->errorMsg, "Cluster %d already exists\n",
				current_cluster_id);
			return MAP_NOT_VALID;
		}
		// This cluster id is unique
		if(glob_map->nrClusters == glob_map->maxClusters)
		{
			debug_lr(MAP_DEBUG, "There are too many clusters\n");
			return MAP_NOT_VALID;
		}
		// Getting index for the new cluster
		i = glob_map->nrClusters++;
		cluster_ptr = &glob_map->clusterArr[i]; 
		cluster_ptr->clusterId = current_cluster_id;
		cluster_ptr->rangeList = NULL;
		cluster_ptr->zone = NOT_SET_ZONE;
		saw_cluster_def = 1;
		map_is_new = 1;
	}
	else if(sscanf(line_buff, "part=%d", &current_part_id) == 1) {
		char *range_ptr;
		//debug_lg(MAP_DEBUG, "\t\tfound part %d in cluster %d\n",
		// current_part_id, current_cluster_id);
		// Checking if the map is old
		if(map_is_old) {
			debug_lr(MAP_DEBUG, "Can not mix old MOSIX map with new map format\n");
			return MAP_SYNTAX_NOT_VALID;
		}
		// Checking that there was a cluster definition seen before
		if(!saw_cluster_def) {
			debug_lr(MAP_DEBUG, "In new map there must be a cluster"
				 " section before partition section\n");
			return MAP_SYNTAX_NOT_VALID;
		}
		// Parsing the range (whatever starts after the :)
		range_ptr = index(line_buff, ':');
		if(!range_ptr) {
			debug_lr(MAP_DEBUG, "\t\t found part but no :\n");
			return MAP_SYNTAX_NOT_VALID;
		}
		range_ptr++;
		while(isspace(*range_ptr)) range_ptr++; 
		if(*range_ptr == '\0')
		{
			debug_lr(MAP_DEBUG, "\t\tEmpty range\n");
			return MAP_SYNTAX_NOT_VALID;
		}
		//debug_lg(MAP_DEBUG,"\t\tpart range is: '%s'\n", range_ptr);
		if(!parse_range(range_ptr, &base, host_name, &count))
			return MAP_SYNTAX_NOT_VALID;

		// If the range is valid adding it to the cluster
		if(!add_cluster_range(cluster_ptr, current_part_id,
				      base, host_name, count))
			return MAP_NOT_VALID; 

	}
	// Dealing with friends 
	else if ((tmp_ptr = strstr(line_buff, edge_prefix))) {
		if(!dist_graph_add_edge_from_str(glob_map->distGraph, line_buff))
		{
			debug_lr(MAP_DEBUG, "Edge line is not leagal\n");
			return MAP_NOT_VALID;
		}
	}
	// Dealing with proximity zones default value
	else if(sscanf(line_buff, "default_zone=%s", default_zone_str) == 1)
	{
		if(saw_zone_def) {
			debug_lr(MAP_DEBUG, "default_zone definition must come"
				 " before any zone definition\n");
			return MAP_SYNTAX_NOT_VALID;
		}
		if(!strcmp(default_zone_str, SAME_ZONE_STR)) {
			default_zone = SAME_ZONE;
		}
		else if(!strcmp(default_zone_str, DIFFERENT_ZONE_STR)) {
			default_zone = DIFFERENT_ZONE;
		}
		else {
			debug_lr(MAP_DEBUG, "default_zone value is not valid: %s\n",
				 default_zone_str);
			return MAP_SYNTAX_NOT_VALID;
		}
	}
	// Dealing with zone definition
	else if(sscanf(line_buff, "zone=%d", &zone_id) == 1)
	{
		char *zone_ptr;
		saw_zone_def = 1;
			// Parsing the range (whatever starts after the :)
		zone_ptr = index(line_buff, ':');
		if(!zone_ptr) {
			debug_lr(MAP_DEBUG, "\t\t found zone but no :\n");
			return MAP_SYNTAX_NOT_VALID;
		}
		zone_ptr++;
		while(isspace(*zone_ptr)) zone_ptr++; 
		if(*zone_ptr == '\0')
		{
			debug_lr(MAP_DEBUG, "\t\tEmpty zone\n");
			return MAP_SYNTAX_NOT_VALID;
		}
		debug_lg(MAP_DEBUG,"\t\tzone is: '%s'\n", zone_ptr);
		if(!set_clusters_zone(zone_id, zone_ptr))
			return MAP_SYNTAX_NOT_VALID;
	}
	// Dealing with old format mosix map
	else if (sscanf(line_buff, "%d %s %d", &base, host_name, &count) == 3) {
		debug_lb(MAP_DEBUG, "Found old mosix map entry\n");
		if(map_is_new) {
			debug_lr(MAP_DEBUG, "Can not mix old mosix map with new map format\n");
			return MAP_SYNTAX_NOT_VALID;
		}
		// If the range is valid adding it to the cluster (the single cluster we have)
		// Doing the initialization part only in the first time the map become old
		cluster_ptr = &glob_map->clusterArr[0]; 
		if(!map_is_old) {
			glob_map->nrClusters = 1;
			cluster_ptr->clusterId = 0;
			cluster_ptr->rangeList = NULL;
		}
		if(!add_cluster_range(cluster_ptr, 0,
				      base, host_name, count))
		{
			return MAP_NOT_VALID; 
		}
		map_is_old = 1;
		glob_map->mapType = MOSIX_MAP_PM;
	}
	else {
		line_buff[strlen(line_buff)] = '\0';
		sprintf(glob_map->errorMsg, "Bad line <%s>\n", line_buff);
		return MAP_SYNTAX_NOT_VALID;
	}
    }

    // Setting the default zone at the end
    glob_map->defaultZone = default_zone;
    
    return MAP_VALID;
}

/** Setting clusters zone information.
 * @param zone_id    Integer value of the zone
 * @param zone_ptr   A string containing space seperated list of clusters
 *                   for example "c1 c2 c3" or just "1 2 3"
 * @return 0 if there is some kind of error like invalid cluster id or invalid
 *         zone string. 1 if all is ok
 */
static int set_clusters_zone(int zone_id, char *zone_ptr)
{
	char *cluster_ptr;
	char cluster_buff[81];

	trim_spaces(zone_ptr);
	while((cluster_ptr = strsep(&zone_ptr, " \t\n")))
	{
		pe_t cluster_id;
		cluster_entry_t *ce;

		strncpy(cluster_buff, cluster_ptr, 80);
		debug_ly(MAP_DEBUG, "Got cluster str %s\n", cluster_buff);
		if(!get_cluster_id(cluster_buff, &cluster_id))
		{
			debug_lr(MAP_DEBUG, "Cluster string is not valid: %s\n",
				 cluster_buff);
			return 0;
		}
		if(!(ce = get_cluster_entry(cluster_id)))
		{
			debug_lr(MAP_DEBUG, "ClusterId is not in list: %d\n",
				 cluster_id);
			return 0;
		}
		ce->zone = zone_id;
	}
	return 1;
}


static 
int parse_range(char *range_ptr, int *b, char *hn, int *c)
{
  int base;
  int count;
  char host_name[80];
  
  if(sscanf(range_ptr, "%d %s %d", &base, host_name, &count) == 3)
  {
	  // Verifying the base and count
	  if(base < 0 || count < 0)
	  {
		  debug_lr(MAP_DEBUG, "Range with negative base or count (%d, %d)\n",
			   base, count);
		  return 0;
	  }
	  if(base + count - 1 >= MAX_NODES)
	  {
		  debug_lr(MAP_DEBUG, "Table Overflow \n");
		  return 0;
	  }
	  debug_lg(MAP_DEBUG, "\t\t Base: %d Host: %s  Count: %d\n",
		   base, host_name, count);
	  
  }
  
  *b = base;
  *c = count;
  strcpy(hn, host_name);
  return 1;
}

static int add_cluster_range(cluster_entry_t *ce, int part_id, int base,
			     char *host_name, int count)
{
	range_entry_t   *re, *re_ptr;
	struct hostent  *hostent;

	struct in_addr ipaddr;

	if(PE_CLUSTER_ID(base)) {
		debug_lr(MAP_DEBUG, "Base number is too big (contain cluster id bits)\n");
		return 0;
	}
	// Resolving the hostname/ipaddress
	// FIXME move this check elsewhere
	if ((hostent = gethostbyname(host_name)) == NULL) {
		if (inet_aton(host_name, &ipaddr) == -1)
		{
			herror(host_name);
			debug_lr(MAP_DEBUG, "Error host name is not resolvable\n");
			return 0;
		}
	} else {
		bcopy(hostent->h_addr, &(ipaddr.s_addr), hostent->h_length);
		// FIXME Why we have this line here ?????
		//ipaddr = ntohl(ipaddr);
	}

	// Once we have the ip we check for wrapping 
	ipaddr.s_addr = ntohl(ipaddr.s_addr);
	if(ipaddr.s_addr + count < ipaddr.s_addr) {
		debug_lr(MAP_DEBUG, "Range has ip wrapping\n");
		return 0;
	}
	
	// Getting a free range entry
	re = mem_cache_alloc(glob_map->rangeCache);
	if(!re) {
		debug_lr(MAP_DEBUG, "Not enough ranges in cache\n");
		return 0;
	}

	// Setting the fields inside
	re->partitionId = part_id;
	re->base = base;
	re->count = count;
	re->base_addr = ipaddr;
	re->next = NULL;
	
	// Adding to the list at the end
	if(!ce->rangeList)
		ce->rangeList = re;
	else {
		for(re_ptr = ce->rangeList ; re_ptr->next ; re_ptr = re_ptr->next);
		re_ptr->next = re;
	}
	return 1;
}

int is_cluster_exists(int cluster_id)
{
	int i;
	
	for(i=0; i<glob_map->maxClusters ; i++)
		if(glob_map->clusterArr[i].clusterId == cluster_id)
			return 1;
	return 0;
}
 
int set_as_xml(const char *buff, int size) {
	return MAP_NOT_VALID;
}
 
void mapper_printf(mapper_t map, mapper_print_t t)
{
	return mapper_fprintf(stdout, map, t);
}

void mapper_fprintf(FILE *f, mapper_t map, mapper_print_t t)
{
	char *buff;

	// should be enough for all maps
	buff = (char*)malloc(4096);
	if(!buff) {
		fprintf(f, "Not enough memory for printing map\n");
		return;
	}
	mapper_sprintf(buff, map, t);
	fprintf(f, "%s", buff);
	free(buff);
}

void mapper_sprintf(char *buff, mapper_t map, mapper_print_t t)
{
	int i;
	char *ptr = buff;
	
	if(!buff)
		return;
	
	if(!map) {
		sprintf(buff, "Map is null\n");
		return;
	}
	glob_map = map;
	
	switch(t) {
	    case PRINT_TXT:
	    case PRINT_TXT_PRETTY:
	    {
		    for(i=0 ; i<map->nrClusters ; i++) {
			    print_cluster_entry(ptr, &map->clusterArr[i], t);
			    ptr+= strlen(ptr);
		    }
		    print_zone_info(ptr);
		    ptr+= strlen(ptr);
				   
		    dist_graph_sprintf(ptr, map->distGraph);
	    }
	    break;
	    
	    // Printing the distance map of this node, all the deges that
	    // goes into this node
	    case PRINT_TXT_DIST_MAP:
	    {
		    node_id_t        from, to;
		    pe_t             curr_partition;
		    int              i;
		    cluster_entry_t *ce;
		    range_entry_t   *re;
		    
		    node_id_set(&to, map->myClusterId, map->myPartitionId);
		    
		    for(i=0 ; i < map->nrClusters ; i++) {
			    ce = &(map->clusterArr[i]);
			    curr_partition = -1;
			    for(re = ce->rangeList ; re ; re=re->next)
			    {
				    int dist;
				    // We need only the Partition to partitions
				    if(re->partitionId == curr_partition)
					    continue;
				    curr_partition = re->partitionId;

				    // Getting the distance
				    node_id_set(&from, ce->clusterId, re->partitionId);
				    dist = dist_graph_get_dist(map->distGraph, &from, &to);
				    sprintf(ptr, "c%d:p%d   %d\n",
					    ce->clusterId, re->partitionId, dist);
				    ptr+= strlen(ptr);
			    }	
		    }
	    }
	    break;
	    case PRINT_XML:
	    {
		    sprintf(buff, "XML printing is not supported yet\n");
	    }
	    break;
	    default:
		    sprintf(buff, "Unsupported mapper_print_t: %d\n", t);
	}
}

/** Printing the zone information to the buffer
 * @param buff    Buffer to print to
 */
static void print_zone_info(char *buff)
{
	int     i;
	char   *ptr = buff;

	sprintf(ptr, "default_zone=%s\n",
		glob_map->defaultZone == SAME_ZONE ?
		SAME_ZONE_STR : DIFFERENT_ZONE_STR);
	ptr += strlen(ptr);
	
	for (i=0 ; i<glob_map->nrClusters ; i++)
	{
		cluster_entry_t *ce = &glob_map->clusterArr[i];
		sprintf(ptr, "zone=%d : c%d\n", ce->zone, ce->clusterId);
		ptr += strlen(ptr);
	}
}

static void print_cluster_entry(char *buff, cluster_entry_t *ce, mapper_print_t t)
{
	range_entry_t *re;
	char *ptr = buff;
	struct in_addr;
	
	if(t == PRINT_TXT_PRETTY) {
		sprintf(ptr, "ClusterID: %d NRnodes: %d\n",
			ce->clusterId, ce->nrNodes);
	}
	else if (t == PRINT_TXT) {
		sprintf(ptr, "cluster=%d\n", ce->clusterId);
	}
		
	ptr += strlen(ptr);
	for(re = ce->rangeList ; re ; re = re->next) {
		print_range_entry(ptr, re, t);
		ptr += strlen(ptr);
	}
}

static void print_range_entry(char *buff, range_entry_t *re, mapper_print_t t)
{
	struct in_addr in;

	in = re->base_addr;
	in.s_addr = htonl(in.s_addr);
	if(t == PRINT_TXT_PRETTY) 
		sprintf(buff,"\t\tpartition: %d base: %d  ip: %s  count %d\n",
			re->partitionId, re->base, inet_ntoa(in),
			re->count);
	else if(t == PRINT_TXT)
		sprintf(buff, "part=%d : %d %s %d\n",
			re->partitionId, re->base, inet_ntoa(in),
			re->count);
}

// FIXME take care of cases where my ip(my first ip) is not in the table
static pe_t resolve_my_pe_by_addr()
{
	char              hostname[MAXHOSTNAMELEN+1];
	struct hostent   *hostent;
	struct in_addr    myaddr;
	int               i, found;
	pe_t              pe = 0;
	
	hostname[MAXHOSTNAMELEN] = '\0';
	if (gethostname(hostname, MAXHOSTNAMELEN) < 0) {
		debug_lr(MAP_DEBUG, "gethostname() failed\n");
		return 0;
	}
	if ((hostent = gethostbyname(hostname)) == NULL)
	{
		if (inet_aton(hostname, &myaddr) == -1) {
			debug_lr(MAP_DEBUG, "gethostbyname() faild\n");
			return 0;
		}
	} else {
		bcopy(hostent->h_addr, &myaddr.s_addr, hostent->h_length);
		//myaddr = ntohl(myaddr);
	}

	myaddr.s_addr = ntohl(myaddr.s_addr);
	
	// This node ip was resolved, now we search the configuration for
	// ourself
	// Iterarting over all the clusters
	for (i=0, found=0 ; i<glob_map->nrClusters ; i++)
	{
		cluster_entry_t *ce = &glob_map->clusterArr[i];
		range_entry_t *re;

		// Going all over the current cluster ranges
		for(re = ce->rangeList ; re != NULL ; re = re->next)
		{
			if((myaddr.s_addr >= re->base_addr.s_addr) &&
			   (myaddr.s_addr < (re->base_addr.s_addr + re->count)))
			{
				debug_lg(MAP_DEBUG, "Found my ip in table\n");
				pe = PE_SET(ce->clusterId, re->base + myaddr.s_addr - re->base_addr.s_addr);
				found = 1;
				break;
			}
		}
		if(found == 1) break;
	}
	
	if(!found) {
		struct in_addr addr;
		addr.s_addr = htonl(myaddr.s_addr);
		debug_lr(MAP_DEBUG, "My ip address (%s) is not in map\n",
			 inet_ntoa(addr));
		return 0;
	}
	return pe;
}

static int is_my_pe_in_map()
{
	int               i;
	pe_t              pe = glob_map->myPe;
	pe_t              cid = PE_CLUSTER_ID(pe);
	pe_t              nid = PE_NODE_ID(pe);
	
	debug_ly(MAP_DEBUG, "Looking for my pe in map (%d)\n", pe);
	for (i=0 ; i<glob_map->nrClusters ; i++)
	{
		cluster_entry_t *ce = &glob_map->clusterArr[i];
		range_entry_t *re;

		if(cid != ce->clusterId)
			continue;
		
		// Going all over the current cluster ranges
		for(re = ce->rangeList ; re != NULL ; re = re->next)
		{
			if(nid >= re->base && nid < re->base + re->count)
			{
				debug_lg(MAP_DEBUG, "Found my pe in table (%d, %d)\n",
					 cid, nid);
				return 1;
			}
		}
	}
	sprintf(glob_map->errorMsg, "My pe %d is not in map", pe);
	return 0;
}

/*
   Checking if the map is valid:
     o  The same ip can not appear in two clusters
     o  In the same cluster no intersection is allowed at all
*/
static int is_map_valid()
{
	cluster_entry_t *ce, *ce2;
	int  i, j;

	// Checking that the map is not empty (empty map is not valid)
	if(glob_map->nrClusters == 0 || !glob_map->clusterArr[0].rangeList)
		return 0;
	
	// Checking that the same ip does not exists in two clusters
	for(i=0 ; i< glob_map->nrClusters ; i++)
	{
		ce = &(glob_map->clusterArr[i]);
		for(j=i+1 ; j< glob_map->nrClusters ; j++)
		{
			ce2 = &(glob_map->clusterArr[j]);
			if(clusters_ips_intersect(ce, ce2))
				return 0;
		}
	}
	// Now checking each cluster seperatly
	for(i=0 ; i< glob_map->nrClusters ; i++)
	{
		ce = &(glob_map->clusterArr[i]);
		if( !is_cluster_ranges_valid(ce) )
			return 0;
	}
	return 1;
}

static int
clusters_ips_intersect(cluster_entry_t *ce1, cluster_entry_t *ce2)
{
	range_entry_t *re1;
	range_entry_t *re2; 
	
	for(re1 = ce1->rangeList ; re1 ; re1 = re1->next)
		for(re2 = ce2->rangeList ; re2 ; re2 = re2->next)
			if(ranges_ip_intersect(re1, re2))
			{
				debug_lr(MAP_DEBUG, "c %d intersect with c %d\n",
					 ce1->clusterId, ce2->clusterId);
				return 1;
			}
	return 0;
}

static int
ranges_ip_intersect(range_entry_t *re1, range_entry_t *re2)
{
	//debug_lb(MAP_DEBUG, " %u %u %u %u\n",
	// re1->base_addr.s_addr, re1->count, re2->base_addr.s_addr, re2->count);
	
	if( (re1->base_addr.s_addr <= re2->base_addr.s_addr &&
	     re1->base_addr.s_addr + re1->count > re2->base_addr.s_addr) ||
	    (re2->base_addr.s_addr <= re1->base_addr.s_addr &&
	     re2->base_addr.s_addr + re2->count > re1->base_addr.s_addr))
	{
		debug_lr(MAP_DEBUG, "ranges IP intersect partId: %d with partId %d\n",
			 re1->partitionId, re2->partitionId);
		return 1;
	}
	return 0;
}
	
static int
ranges_nodeId_intersect(range_entry_t *re1, range_entry_t *re2)
{
	if( (re1->base <= re2->base && re1->base + re1->count > re2->base) ||
	    (re2->base <= re1->base  && re2->base + re2->count > re1->base))
	{
		debug_lr(MAP_DEBUG, "ranges nodeID intersect partId: %d with partId %d\n",
			 re1->partitionId, re2->partitionId);
		return 1;
	}
	return 0;
}

// Now we need to check if the internal cluster ranges are valid
static int
is_cluster_ranges_valid(cluster_entry_t *ce)
{
	range_entry_t *re1, *re2;
	
	for(re1 = ce->rangeList ; re1 ; re1 = re1->next)
		for(re2 = re1->next ; re2 ; re2 = re2->next)
			if(ranges_ip_intersect(re1, re2) ||
			   ranges_nodeId_intersect(re1,re2))
				return 0;
	return 1;
}

/** Using bubble sort to sort the list since we assume it is almost sorted
 * or compleately sorted in most cases
 * @param ce   Pointer to the cluster entry to sort
 */
static void sort_cluster_ranges(cluster_entry_t *ce)
{
	range_entry_t *re1, *re2;
	range_entry_t tmp, *re2next;

	//mapper_printf(glob_map, PRINT_TXT);
	
	for(re1 = ce->rangeList ; re1 ; re1 = re1->next)
	{
		for(re2 = ce->rangeList;  re2 != re1; re2 = re2->next)
		{
			// re2next must exists since re1 is there.
			re2next = re2->next;
			// We need to swap the ranges
			if(re2->base > re2next->base) {
				// Swapping only values of the structs without
				// the value of the next pointer
				tmp = *re2next;
				*re2next = *re2;
				re2next->next = tmp.next;
				*re2 = tmp;
				re2->next = re2next;
			}
		}
	}
}
/// Comparison function for qsort
static int sort_by_clusterId(const void *a, const void *b)
{
	cluster_entry_t *ca = (cluster_entry_t *)a;
	cluster_entry_t *cb = (cluster_entry_t *)b;

	if(ca->clusterId < cb->clusterId)
		return -1;
	if (ca->clusterId == cb->clusterId)
		return 0;
	return 1;
}

/** Using qsort to sort the cluster array and then each cluster ranges by pe
 * order
 */
static void sort_cluster_arr()
{
	int i;
	qsort(glob_map->clusterArr, glob_map->nrClusters,
	      sizeof(cluster_entry_t), sort_by_clusterId);
	
	for(i=0 ; i<glob_map->nrClusters ; i++)
	{
		sort_cluster_ranges(&(glob_map->clusterArr[i]));
	}
}

/** Updating general information of the mapper object
 * Information such as the total number of nodes, the number of nides in each
 * cluster and so on.
 */
static void update_general_info()
{
	int              i,tnodes = 0;
	cluster_entry_t *ce;
	range_entry_t   *re;
	pe_t             my_cluster_id;
	pe_t             my_node_id;
	
	my_cluster_id = PE_CLUSTER_ID(glob_map->myPe);
	my_node_id = PE_NODE_ID(glob_map->myPe);

	glob_map->nrRanges = 0;
	glob_map->maxPe = 0;
	glob_map->myClusterId = 0;
	glob_map->myPartitionId = 0;
	glob_map->myClusterEnt = NULL;
	glob_map->myRangeEnt = NULL;
	
	for(i=0 ; i<glob_map->nrClusters ; i++) {
		int cluster_nodes = 0;
		ce = &glob_map->clusterArr[i];

		// Fixing the zone info for clusters with NOT_SET_ZONE
		if(ce->zone == NOT_SET_ZONE)
			ce->zone = glob_map->defaultZone;
		
		for(re = ce->rangeList ; re ; re=re->next)
		{
			// Calculating max pe;
			if(re->base + re->count -1 > glob_map->maxPe)
				glob_map->maxPe = re->base + re->count -1;

			// Finding my cluster partition....
			if(my_cluster_id == ce->clusterId &&
			   my_node_id >= re->base &&
			   my_node_id <  re->base + re->count)
			{
				glob_map->myClusterId = my_cluster_id;
				glob_map->myPartitionId = re->partitionId;
				glob_map->myClusterEnt = ce;
				glob_map->myRangeEnt = re;
			}
			cluster_nodes += re->count;
			glob_map->nrRanges++;
		}
		ce->nrNodes = cluster_nodes;
		tnodes += cluster_nodes;
	}
	glob_map->nrNodes = tnodes;
	
	// Updating the nrPartitionNodes (the partition id is now known)
	glob_map->nrPartitionNodes = 0;
	glob_map->nrClusterNodes = 0;
	for(re = glob_map->myClusterEnt->rangeList ; re ; re = re->next) {
		if(re->partitionId == glob_map->myPartitionId)
			glob_map->nrPartitionNodes += re->count;
		glob_map->nrClusterNodes += re->count;
	}
	
}

/* All the queries here */
/** Returning the cluster entry for the cluster_id
 * @parm  cluster_id   Cluster id to get entry for
 * @return Pointer to a valid cluster_entry_t if cluster_id is on mapper
 * @return NULL on error (cluster id is not in mapper)
 * @todo Use binary search here
*/
static cluster_entry_t *get_cluster_entry(pe_t cluster_id)
{
	cluster_entry_t *ce;
	int i;

	for(i=0 ; i < glob_map->nrClusters ; i++) {
		ce = &(glob_map->clusterArr[i]);
		if(ce->clusterId == cluster_id)
			return ce;
	}
	return NULL;
}

/** Returning the range entry for a given cluster_id and node_id
   @param  ce       Cluster entry to search node in
   @param  node_id  Node id to search
   @return Pointer to the node range_entry_t
   @return NULL on error (cant find node id in cluster)
 */
static range_entry_t *get_range_entry(cluster_entry_t *ce, pe_t node_id)
{
	range_entry_t   *re;
	
	for(re = ce->rangeList ; re ; re=re->next)
	{
		if(node_id >= re->base && node_id < re->base + re->count)
			return re;
	}
	return NULL;
}


/** Translating PE to IP address. The address is in host byte order
 * @param map   Mapper object
 * @param pe    PE to resolve
 * @hostname    Pointer to a in_addr_t variable
 * @return 0 on error and 1 on success
 */
int mapper_node2addr(mapper_t map, mnode_t pe, in_addr_t *ip) {
	cluster_entry_t  *ce;
	range_entry_t    *re;
	in_addr_t         addr;
	
	if (!map) return 0;
	glob_map = map;
	
	if(!(ce = get_cluster_entry(PE_CLUSTER_ID(pe))))
		return 0;
	if(!(re = get_range_entry(ce, PE_NODE_ID(pe))))
		return 0;
	addr = re->base_addr.s_addr;
	addr += PE_NODE_ID(pe) - re->base;
	*ip = htonl(addr);
	return 1;
}

/** Translating PE to host name
 * @param map   Mapper object
 * @param pe    PE to resolve
 * @hostname    Pointer to a buffer where the result can be placed in
 * @return 0 on error and 1 on success
 */
int mapper_node2host(mapper_t map, mnode_t pe, char *hostname)
{
	cluster_entry_t  *ce;
	range_entry_t    *re;
	struct in_addr    addr;
	char             *tmp_hostname;
	
	if (!map) return 0;
	glob_map = map;

	if(!(ce = get_cluster_entry(PE_CLUSTER_ID(pe))))
		return 0;
	if(!(re = get_range_entry(ce, PE_NODE_ID(pe))))
		return 0;

	addr = re->base_addr;
	addr.s_addr += PE_NODE_ID(pe) - re->base;
	addr.s_addr = htonl(addr.s_addr);
	tmp_hostname =  inet_ntoa(addr);

	debug_lb(MAP_DEBUG, "Got '%s'\n", tmp_hostname);
	
	strncpy(hostname, tmp_hostname, strlen(tmp_hostname)+1);
	return 1;
}

/** Translating PE to its cluster id
 * @param map           Mapper object
 * @param pe            PE to translate
 * @param cluster_id    Pointer to a cluster_id variable
 * @return 0 on error and 1 on success
 */
int mapper_node2cluster_id(mapper_t map, mnode_t pe, int *cluster_id)
{
	if(!map) return 0;
	*cluster_id = PE_CLUSTER_ID(pe);
	return 1;
}

/** Translating PE to its partition id
 * @param map         Mapper object
 * @param pe          PE to translate
 * @param part_id     Pointer to an pe_t
 * @return 0 on error and 1 on success, and store the partition_id in the
 *         part_id
 */
int mapper_node2partition_id(mapper_t map, mnode_t pe, int *part_id) {
	cluster_entry_t  *ce;
	range_entry_t    *re;
	
	if (!map) return 0;
	glob_map = map;
	
	if(!(ce = get_cluster_entry(PE_CLUSTER_ID(pe))))
		return 0;
	if(!(re = get_range_entry(ce, PE_NODE_ID(pe))))
		return 0;
	*part_id = re->partitionId;
	return 1;
}

/** Translating ip address to pe.
 * @param  map     Map object to use
 * @param *ip      Poninter to the ip address
 * @param *pe      Pointer to the result pe
 * @return 0 on error and 1 on success
 */
int mapper_addr2node(mapper_t map, struct in_addr *ip, mnode_t *pe)
{
	int              i;
	cluster_entry_t *ce;
	range_entry_t   *re;
	in_addr_t        ipaddr = ntohl(ip->s_addr);
	
	if(!map)
		return 0;
	for(i=0 ; i < map->nrClusters ; i++) {
		ce = &(map->clusterArr[i]);
		for(re = ce->rangeList ; re ; re=re->next)
		{
			if(ipaddr >= re->base_addr.s_addr &&
			   ipaddr < re->base_addr.s_addr + re->count)
			{
				pe_t node_id;
				node_id = re->base + (ipaddr - re->base_addr.s_addr);
				*pe = PE_SET(ce->clusterId, node_id);
				return 1;
			}
		}	
	}
	return 0;
}

/** Translating hostname to PE
 * @param  map         Map object to use
 * @param  hostname    Buffer containing the host name to translate
 * @param *pe          Pointer to the resulting pe
 * @return 0 on error and 1 on success
 */
int mapper_hostname2node(mapper_t map, const char* hostname, mnode_t *pe)
{
	struct hostent  *hostent;
	struct in_addr   ipaddr;

	if(!map)
		return 0;
	
	if ((hostent = gethostbyname(hostname)) == NULL) {
		if (inet_aton(hostname, &ipaddr) == -1) {
			debug_lr(MAP_DEBUG, "Error host name is not resolvable\n");
			return 0;
		}
	} else {
		bcopy(hostent->h_addr, &ipaddr.s_addr, hostent->h_length);
	}
	//ipaddr.s_addr = ntohl(ipaddr.s_addr);
	return mapper_addr2node(map, &ipaddr, pe);
}

int mapper_node_at_dist_pos(mapper_t map, int pos, int dist, mapper_node_info_t *ninfo)
{
	int               i,nodes_seen = 0;
	int               range_dist;
	node_id_t         from, to;
	cluster_entry_t  *ce;
	range_entry_t    *re;

	if(!map)
		return 0;
	if(dist < SAME_PARTITION_MAX_DIST || dist > INFINITY_DIST)
		return 0;

	node_id_set(&to, map->myClusterId, map->myPartitionId);
	for(i=0 ; i < map->nrClusters ; i++)
	{
		ce = &(map->clusterArr[i]);
		for(re = ce->rangeList ; re ; re=re->next)
		{
			node_id_set(&from, ce->clusterId, re->partitionId);
			range_dist = dist_graph_get_dist(map->distGraph, &from, &to);
			if(range_dist > dist)
				continue;
			if(pos >= nodes_seen && pos <= (nodes_seen + re->count))
			{
				ninfo->ni_addr.s_addr =
					re->base_addr.s_addr + (pos - nodes_seen);
				ninfo->ni_pe = PE_SET(ce->clusterId, re->base + (pos - nodes_seen));
				ninfo->ni_partid = re->partitionId;
				return 1;
			}
			nodes_seen += re->count;
		}	
	}
	return 0;
}

int mapper_node_at_pos(mapper_t map, int pos, mapper_node_info_t *ninfo)
{
	int               i,nodes_seen = 0;
	cluster_entry_t  *ce;
	range_entry_t    *re;

	if(!map)
		return 0;
	
	for(i=0 ; i < map->nrClusters ; i++)
	{
		ce = &(map->clusterArr[i]);
		for(re = ce->rangeList ; re ; re=re->next)
		{
			if(pos >= nodes_seen && pos <= (nodes_seen + re->count))
			{
				ninfo->ni_addr.s_addr =
					re->base_addr.s_addr + (pos - nodes_seen);
				ninfo->ni_pe = PE_SET(ce->clusterId, re->base + (pos - nodes_seen));
				ninfo->ni_partid = re->partitionId;
				return 1;
			}
			nodes_seen += re->count;
		}	
	}
	return 0;
}
int mapper_node_at_cluster_pos(mapper_t map, int pos, mapper_node_info_t *ninfo)
{
	int               nodes_seen = 0;
	cluster_entry_t  *ce;
	range_entry_t    *re;
	
	ce = map->myClusterEnt;
	for(re = ce->rangeList ; re ; re=re->next)
	{
		if(pos >= nodes_seen && pos <= (nodes_seen + re->count))
		{
			ninfo->ni_addr.s_addr =
				re->base_addr.s_addr + (pos - nodes_seen);
			ninfo->ni_pe = PE_SET(ce->clusterId, re->base + (pos - nodes_seen));
			ninfo->ni_partid = re->partitionId;
			return 1;
		}
		nodes_seen += re->count;
	}	
	return 0;
}

int mapper_node_at_partition_pos(mapper_t map, int pos, mapper_node_info_t *ninfo)
{
	int                      nodes_seen = 0;
	cluster_entry_t  *ce;
	range_entry_t    *re;
	
	ce = map->myClusterEnt;
	for(re = ce->rangeList ; re ; re=re->next)
	{
		if(re->partitionId != map->myPartitionId)
			continue;
		if(pos >= nodes_seen && pos <= (nodes_seen + re->count))
		{
			ninfo->ni_addr.s_addr =
				re->base_addr.s_addr + (pos - nodes_seen);
			ninfo->ni_pe = PE_SET(ce->clusterId, re->base + (pos - nodes_seen));
			ninfo->ni_partid = re->partitionId;
			return 1;
		}
		nodes_seen += re->count;
	}	
	return 0;	
}

/** Producing a map for the MOSIX kernel. The map is a collection of
 * struct mosixnet entries. Acorrding the the distance between the local
 * partition and the other partition and the type of map requested the value
 * of the owner field is set. The max_guest_dist and min_guest_dist define
 * The guest circle. Nodes with distance larger then max_guest_dist are
 * not allowd to access this node
 *
 @param map             Mapper object
 @param buff            A memory buffer to hold the kernel map
 @param size            The size of the buffer (in mosixnet entries)
 @param min_guest_dist  The minimal distance that define an outsider (guest)
 @param max_guest_dist  The maximal distance that a guest can have
 @param max_mig_dist    The maximal distance local processes can reach

 @return The size of the kernel map entries. If the buffer is not enough
         0 is returned
*/
int mapper_get_kernel_map(mapper_t map, struct mosixnet *conf, int size,
			  int min_guest_dist, int max_guest_dist,
			  int max_mig_dist)
{
	int              i, pos;
	cluster_entry_t *ce;
	range_entry_t   *re;
	struct mosixnet tmp_mnet;
	
	// Sanity
	if(!map || !conf || size <= 0)
		return 0;

	glob_map = map;
	
	pos = 0;
	for(i=0 ; i < map->nrClusters ; i++)
	{
		ce = &(map->clusterArr[i]);
		for(re = ce->rangeList ; re ; re=re->next)
		{
			// Checking we have enough memory (just for safty)
			if( (pos +1) > size )
			{
				debug_lr(MAP_DEBUG,
					 "Not enough memory for producing kernel map\n");
				return  0;
			}
			// Trying to convert to mosixnet. IF failed this enrty
			// is skiped (can happen in infinity distance)
			if(!range_to_mosixnet(re, ce->clusterId, &tmp_mnet,
					      min_guest_dist, max_guest_dist,
					      max_mig_dist))
			{
				debug_lr(MAP_DEBUG, "skipping entry\n");
				continue;
			}
			conf[pos++] = tmp_mnet;
		}
	}
	return pos;
}
/** Converting a mapper range struct to a mosixnet struct according to the
 * type t
 *
 * Well all the ownership logic is here. The owner is placed in the mosixnet
 * according to the distance. A guest is with owner=2 and non guest with
 * owner = 1;
 * 
 *
 @param *re     Pointer to the range_entry_t
 @param *mnet   Pointer to the mosixnet struct
 @param  max_known_dist  The maximal distance which the range can be if it is
                         to be added to the map
 @param  guest_dist  The distance that define an outsider.

 @return 0 on Error or when the mosixnet should not exists (like in cases
         where the distance in infinity) and 1 on success (the mosixnet
	 entry should be used
 */ 
static int
range_to_mosixnet(range_entry_t *re, pe_t cluster_id,
		  struct mosixnet *mnet, int min_guest_dist, int max_guest_dist,
		  int max_mig_dist)
{
	node_id_t from, to;
	int d;
	char outsider = 1;
	char dontgo = 0;
	char donttake = 0;
	char proximate=0;
	cluster_entry_t *ce;
	struct in_addr addr;
	
	// Calculating the outsider 
	node_id_set(&from, cluster_id, re->partitionId);
	node_id_set(&to, glob_map->myClusterId, glob_map->myPartitionId);
	
	d = dist_graph_get_dist(glob_map->distGraph, &from, &to);
	if(d == INVALID_DIST)
	{
		debug_lr(MAP_DEBUG, "Got invalid dist\n");
		return 0;
	}
	if(!(ce = get_cluster_entry(cluster_id)))
	{
		debug_lr(MAP_DEBUG, "Can not get cluster entry\n");
		return 0;	
	}

	
	// Calculating the outsider, dontgo, donttake
	outsider = (d >= min_guest_dist || d > max_mig_dist) ? 1 : 0;
	donttake = (d > max_guest_dist)  ? 1 : 0;
	dontgo   = (d > max_mig_dist)    ? 1 : 0;

	// Calculating the proximate
	// Not same zone value 
	if(glob_map->myClusterEnt->zone == DIFFERENT_ZONE)
		proximate = 0;
	else if(ce->zone != glob_map->myClusterEnt->zone)
	{
		proximate = 0;
	}
	else
		proximate = 1;
	
	// Setting the mosixnet 
	mnet->base     = PE_SET(ce->clusterId, re->base);
	mnet->x        = AF_INET;
	mnet->n        = re->count;
	mnet->ip       = htonl(re->base_addr.s_addr);
	mnet->outsider = outsider;
	mnet->dontgo   = dontgo;
	mnet->donttake = donttake;
	mnet->proximate= proximate;
	mnet->owner    = OWNER_SET(cluster_id, re->partitionId);

	addr.s_addr = mnet->ip;
	debug_lg(MAP_DEBUG, "dist %d, base=%d, n=%d, ip=%s, o=%d, dg=%d, dt=%d, p=%d, owner= %d\n",
		 d,
		 mnet->base, 
		 mnet->n,        
		 inet_ntoa(addr),
		 mnet->outsider,
		 mnet->dontgo,
		 mnet->donttake,
		 mnet->proximate,
		 mnet->owner);    

	return 1;
}

/** Creating iterator based on distance. its need to be fix 
 */
mapper_iter_t mapper_dist_iter_init(mapper_t map, int dist) {
	if(!map)
		return NULL;

	// FIXME iterator is not correct;
	if(dist < SAME_PARTITION_MAX_DIST || dist > INFINITY_DIST)
		return NULL;
	if(dist <= SAME_PARTITION_MAX_DIST)
		return mapper_iter_init(map, MAP_ITER_PARTITION);
	if(dist <= SAME_CLUSTER_MAX_DIST)
		return mapper_iter_init(map, MAP_ITER_CLUSTER);
	//if(dist <= MAX_KNOWN_DIST)
	//      return mapper_iter_init(map, MAP_ITER_ALL);
	return mapper_iter_init(map, MAP_ITER_ALL);
}

/** Creating a new iterator over the map

*/
mapper_iter_t mapper_iter_init(mapper_t map, mapper_iter_type_t iter_type)
{
	mapper_iter_t iter;
	
	if(!map)
		return NULL;
	if(!(iter = malloc(sizeof(struct mapper_iter_struct))))
		return NULL;

	iter->map = map;
	iter->type = iter_type;
	switch(iter_type) {
	    case MAP_ITER_ALL:
		    iter->currClusterEnt = map->clusterArr;
		    iter->currClusterPos = 0;
		    iter->currRangeEnt = map->clusterArr[0].rangeList;
		    break;
	    case MAP_ITER_CLUSTER:
		    iter->currClusterEnt = map->myClusterEnt;
		    iter->currRangeEnt = map->myClusterEnt->rangeList;
		    break;
	    case MAP_ITER_PARTITION:
	    {
		    cluster_entry_t  *ce;
		    range_entry_t    *re;
		    // We search until the first range from out partition is
		    // found. And then we start
		    ce = map->myClusterEnt;
		    for(re = ce->rangeList ; re ; re=re->next)
			    if(re->partitionId == map->myPartitionId)
				    break;
		    iter->currClusterEnt = map->myClusterEnt;
		    iter->currRangeEnt = re;;
	    }
	    break;
	    default:
		    return NULL;
		    
	}

	iter->currNodeId = iter->currRangeEnt->base;
	iter->partitionId = map->myPartitionId;
	return iter;
}


void mapper_iter_done(mapper_iter_t iter) {
	free (iter);
}

int mapper_next(mapper_iter_t iter, mapper_node_info_t *node_info)
{
	cluster_entry_t  *ce;
	range_entry_t    *re;
	pe_t              node_id;
	
	if(!iter)
		return 0;

	// The sign to stop iteration
	if(!iter->currNodeId)
		return 0;
	
	ce = iter->currClusterEnt;
	re = iter->currRangeEnt;
	// The pe we should return
	node_id = iter->currNodeId;
	// Still inside the current range we return the pe and move to the next
	if(node_id >= re->base && node_id < (re->base + re->count))
	{
		iter->currNodeId++;
		set_mapper_node_info(ce, re, node_id, node_info);
		return node_id;
	}
	// Out of the range need to find next one (or stop)
	// We have move ranges in the same cluster

	// Default case : no more clusters
	iter->currNodeId = 0;
	switch (iter->type) {
	    case MAP_ITER_ALL:
		    if(re->next) {
			    iter->currRangeEnt = re->next;
			    iter->currNodeId = re->next->base;	    
		    }
		    else {
			    iter->currClusterPos++;
			    if(iter->currClusterPos < iter->map->nrClusters)
			    {
				    iter->currClusterEnt++;
				    iter->currRangeEnt = iter->currClusterEnt->rangeList;
				    iter->currNodeId = iter->currRangeEnt->base;
			    }
			    
		    }
		    break;
	    case MAP_ITER_CLUSTER:
		    if(re->next) {
			    iter->currRangeEnt = re->next;
			    iter->currNodeId = re->next->base;	    
		    }
		    break;
	    case MAP_ITER_PARTITION:
		    while(re->next){
			    if(re->next->partitionId == iter->partitionId) {
				    iter->currRangeEnt = re->next;
				    iter->currNodeId = re->next->base;
				    break;
			    }
			    else
				    re = re->next;
		    }
		    break;
	default:
		    return 0;
	}

	// Building the node_info if we have a valid pe to give
	if(!iter->currNodeId)
		return 0;
	
	set_mapper_node_info(iter->currClusterEnt, iter->currRangeEnt,
			     iter->currNodeId, node_info);
	// For the next stage
	iter->currNodeId++;
	return (iter->currNodeId - 1);
}

/** Setting a mapper_node_info struct.
 * @param *ce       A cluster entry object the node id should belong to
 * @param *re       A range object the node id belong to
 * @param node_id   The requested node id
 * @param *ni       The resulting node id structure
 * @return 0 on error 1 on success
*/
static int set_mapper_node_info(cluster_entry_t *ce, range_entry_t *re,
			    pe_t node_id, mapper_node_info_t *ni)
{
	int pos;
	
	if(!ce || !re || !node_id || !ni)
		return 0;
	// node_id is somehow not inside the range :-(
	if(node_id < re->base || node_id >= (re->base + re->count))
		return 0;
	pos = node_id - re->base;
	ni->ni_addr.s_addr =  re->base_addr.s_addr + pos;
	ni->ni_pe = PE_SET(ce->clusterId, node_id);
	ni->ni_partid = re->partitionId;
	return 1;
}

/** Getting the distance from the given node to the local node (the one the
    mapper is about.
 @param map       Mapper object
 @param node_pe   Pe of node to check
 @return INVALID_DIST on error and distance between the nodes on success
 */
int mapper_node_dist(mapper_t map, pe_t node_pe)
{
	node_id_t        from, to;
	cluster_entry_t  *ce;
	range_entry_t    *re;

	ce = get_cluster_entry(PE_CLUSTER_ID(node_pe));
	if(!ce) return INVALID_DIST;
	re = get_range_entry(ce, PE_NODE_ID(node_pe));
	if(!re) return INVALID_DIST;
	
	node_id_set(&to, map->myClusterId, map->myPartitionId);
	node_id_set(&from, ce->clusterId, re->partitionId);
	return dist_graph_get_dist(map->distGraph, &from, &to);
}

/** Returning the maximal distance between two nodes
    (the largest distance that is not INFINITY_DIST)
    @param map   Mapper object
    @return maximal distance 
 */
int mapper_get_max_dist(mapper_t map)
{
	// FIXME currently we only support one level grid
	return SAME_GRID_MAX_DIST + 2;
}
/** @} */ // end of mapper module
