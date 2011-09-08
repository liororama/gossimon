/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __MAPPER_H
#define __MAPPER_H



#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <msx_common.h>
#include <pe.h>

// Maximum number of supported clusters in the mapper
#define   MAPPER_MAX_CLUSTERS    (256)
// Maximum number of ranges this map support
#define   DEFAULT_MAX_RANGES    (350)
// Maximum size of cluster name
#define   MAPPER_MAX_CLUSTER_NAME (256)
#define   MAPPER_MAX_PARTNER_NAME  MAPPER_MAX_CLUSTER_NAME

typedef struct mapper_struct *mapper_t;

typedef struct mapper_node_info
{
	pe_t             ni_pe;
	struct in_addr   ni_addr;
} mapper_node_info_t;

typedef struct _mapper_range {
	struct in_addr   baseIp;  // holds the basee address in host byte order
	int              baseId;       // The possible node id (PE) of node
	int              count;      // Total nodes in range
} mapper_range_t;

typedef struct mapper_range_info {
	char             ri_core;
	char             ri_participate;
	char             ri_proximate;
} mapper_range_info_t;

typedef struct mapper_cluster_info {

	char            *ci_desc;
	int              ci_prio;    // Possible cluster priority/Distance
	                             // where lower is better
	char             ci_cango;
	char             ci_cantake;
	char             ci_canexpend;
} mapper_cluster_info_t;

typedef enum {
	INPUT_MEM,
	INPUT_FILE,
	INPUT_FILE_BIN,
	INPUT_CMD,
} map_input_type;

mapper_t mapperInit(int max_ranges);
int mapperDone(mapper_t map);

mapper_t mapperCopy(mapper_t map);

int mapperSetMyPe(mapper_t map, pe_t my_pe);
pe_t mapperGetMyPe(mapper_t map);

int mapperSetMyIP(mapper_t map, struct in_addr *ip);
int mapperGetMyIP(mapper_t map, struct in_addr *ip);


typedef enum 
{
	PRINT_XML,
	PRINT_TXT,
	PRINT_TXT_PRETTY,
	PRINT_TXT_DIST_MAP
} mapper_print_t;

/* Printing of map */
#include <stdio.h>
void mapperPrint(mapper_t map, mapper_print_t t);
void mapperFPrint(FILE *f, mapper_t map, mapper_print_t t);
void mapperToString(char *buff, mapper_t map, mapper_print_t t);

/* Getting the erroneous line/s in the last map set or a specific error message */
char *mapperError(mapper_t map);

/* Signatures of map */
uint32_t mapperGetCRC32(mapper_t map);

/* Map Iterators */

//------- The nodes iterator type. --------------
typedef enum {
	MAP_ITER_ALL,        // Iterate over all nodes in map
	MAP_ITER_CLUSTER,    // Iterate only over local cluster nodes
	MAP_ITER_SELECTED,   // Iterate only over selected nodes
} mapper_iter_type_t;

typedef struct mapper_iter_struct *mapper_iter_t;

mapper_iter_t mapperIter(mapper_t map, mapper_iter_type_t iter_type);

// Obtain iterator over the nodes of a given cluster
mapper_iter_t mapperClusterNodesIter(mapper_t map, const char *clusterName);
void  mapperIterDone(mapper_iter_t iter);
int mapperNext(mapper_iter_t iter, mapper_node_info_t *node_info);

//------- The clusters iterator -------------------
typedef struct mapper_cluster_iter_struct *mapper_cluster_iter_t;
mapper_cluster_iter_t mapperClusterIter(mapper_t map);
void mapperClusterIterDone(mapper_cluster_iter_t iter);
int mapperClusterIterNext(mapper_cluster_iter_t iter, char **clusterName);
int mapperClusterIterRewind(mapper_cluster_iter_t iter);



typedef struct mapper_range_iter_struct *mapper_range_iter_t;
mapper_range_iter_t mapperRangeIter(mapper_t map, const char *clusterName );
void mapperRangeIterDone(mapper_range_iter_t iter);
int mapperRangeIterNext(mapper_range_iter_t iter,
			mapper_range_t *mr, mapper_range_info_t *ri);
int mapperRangeIterRewind(mapper_range_iter_t iter);


/* Queries on the mapper  */

int mapper_node2host(mapper_t map, mnode_t num, char *hostname);
int mapper_node2addr(mapper_t map, mnode_t num, in_addr_t *ip);
int mapper_node2cluster_id(mapper_t map, mnode_t num, int *cluster_id);

int mapper_hostname2node(mapper_t map, const char* hostname, mnode_t* num);
int mapper_addr2node(mapper_t map, struct in_addr *ip, mnode_t *num);

int mapper_node_at_pos(mapper_t map, int pos, mapper_node_info_t *ninfo);
int mapper_node_at_cluster_pos(mapper_t map, int pos, mapper_node_info_t *ninfo);
int mapper_node_at_dist_pos(mapper_t map, int pos, int dist, mapper_node_info_t *ninfo);


/* Cluster related queries */
int mapper_addr2cluster(mapper_t map, struct in_addr *ip, char *cluster);
int mapper_hostname2cluster(mapper_t map, char  *hostname, char *cluster);

int mapper_setClusterInfo(mapper_t map, const char *clusterName,
			  mapper_cluster_info_t *ci);
int mapper_getClusterInfo(mapper_t map, const char *clusterName,
			  mapper_cluster_info_t *ci);
// Should be called on the cluster info returned by getClusterInfo
void cluster_info_init(mapper_cluster_info_t *ci);
void cluster_info_free(mapper_cluster_info_t *ci);


/* Section sizes */
int mapperTotalNodeNum(mapper_t map);
int mapperClusterNodeNum(mapper_t map);
int mapperClusterNum(mapper_t map);
//int mapper_get_node_num_at_dist(mapper_t map, int dist);

int mapperRangesNum(mapper_t map);

/* Range handling */
int mapper_get_range_str(mapper_t map, char *buff, int size);
int mapper_get_cluster_range_str(mapper_t map, char *buff, int size);


/* Getting the distance between the given node to the mapper node */
int mapper_node_dist(mapper_t map, pe_t node_pe);			  

/* Getting the maximum distance between any two nodes */
int mapper_get_max_dist(mapper_t map);


#endif
