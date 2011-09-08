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

//#include <common.h>
#include <msx_debug.h>
#include <parse_helper.h>
#include <checksum.h>

#include <pe.h>
#include <distance_graph.h>

#include "Mapper.h"
#include "MapperInternal.h"

struct mapper_iter_struct {
	struct mapper_struct  *map;
	mapper_iter_type_t     type;
	
	pe_t                   currNodeId;
	int                    currClusterPos;
	cluster_entry_t       *currClusterEnt;
	range_entry_t         *currRangeEnt;
};

struct mapper_cluster_iter_struct {
	struct mapper_struct  *map;
	int                    currClusterIndex;
	char                   clusterName[MAPPER_MAX_CLUSTER_NAME];
};


// Range iterator for the writers

struct mapper_range_iter_struct {
	struct mapper_struct  *map;
	cluster_entry_t       *ce;
	range_entry_t         *re;
};

typedef enum
{
	MAP_VALID,
	MAP_NOT_VALID,
	MAP_SYNTAX_NOT_VALID,
} map_parse_status_t;



static cluster_entry_t *get_cluster_entry(mapper_t map, pe_t cluster_id);
//static int is_cluster_exists(mapper_t map, int cluster_id);
static void print_cluster_entry(char *buff, cluster_entry_t *ce, mapper_print_t t);
static void print_range_entry(char *buff, range_entry_t *re, mapper_print_t t);
//static int add_cluster_range(cluster_entry_t *ce, int part_id, int base,
//			     char *host_name, int count);

//static int set_clusters_zone(int zone_id, char *zone_ptr);
//static int is_map_valid();
//static pe_t resolve_my_pe_by_addr();
//static int is_my_pe_in_map();
//static int clusters_ips_intersect(cluster_entry_t *ce1, cluster_entry_t *ce2);
//static int ranges_ip_intersect(range_entry_t *re1, range_entry_t *re2);
//static int ranges_nodeId_intersect(range_entry_t *re1, range_entry_t *re2);
//static int is_cluster_ranges_valid(cluster_entry_t *ce);

//static void sort_cluster_arr();
//static void update_general_info();
static int set_mapper_node_info(cluster_entry_t *ce, range_entry_t *re,
				pe_t pe, mapper_node_info_t *node_info);
		

/** Printing a human readable error message to buff. This function should be
    called after a bad
 */
char *mapperError(mapper_t map)
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
int mapperSetMyPe(mapper_t map, pe_t my_pe)
{
	if(!map)
		return 0;
	// Not setting my pe if it is already set.
	if(map->myPe)
		return 0;

	map->myPe = my_pe;
	return 1;
}

pe_t mapperGetMyPe(mapper_t map)
{
	if(!map) return 0;
	return map->myPe;
}

int mapperSetMyIP(mapper_t map, struct in_addr *ip)
{
	int              i, found=0;
	cluster_entry_t *ce;
	range_entry_t   *re;
	in_addr_t        ipaddr = ntohl(ip->s_addr);
	
	if(!map)
		return 0;
	for(i=0 ; i < map->nrClusters && found==0 ; i++) {
		ce = &(map->clusterArr[i]);
		for(re = ce->c_rangeList ; re && found ==0 ; re=re->r_next)
		{
			if(ipaddr >= re->r_base_addr.s_addr &&
			   ipaddr < re->r_base_addr.s_addr + re->r_count)
			{
				debug_lb(MAP_DEBUG, "Found my IP in map\n");
				found = 1;
				break;
			}
		}	
	}
	if(!found)
		return 0;

	map->myPe = re->r_base + (ipaddr - re->r_base_addr.s_addr);
	map->myIPSet = 1;
	map->myIP = *ip;
	mapperFinalizeBuild(map);
	return 1;
}

int mapperGetMyIP(mapper_t map, struct in_addr *ip)
{
	if(!map || !map->myIPSet)
		return 0;
	*ip = map->myIP;
	return 1;
}

pe_t mapper_get_my_cluster_id(mapper_t map)
{
	if(!map) return 0;
	return map->myClusterId;
}


/* Section sizes */
/* int mapper_get_node_num_at_dist(mapper_t map, int dist) */
/* { */
/* 	node_id_t         from, to; */
/* 	cluster_entry_t  *ce; */
/* 	range_entry_t    *re; */
/* 	int               i, nodes = 0, range_dist; */
	
/* 	if(!map) return 0; */
/* 	if(dist < SAME_PARTITION_MAX_DIST || dist > INFINITY_DIST) */
/* 		return 0;	 */

/* 	//node_id_set(&to, map->myClusterId, map->myPartitionId); */
/* 	for(i=0 ; i < map->nrClusters ; i++) { */
/* 		ce = &(map->clusterArr[i]); */
/* 		for(re = ce->c_rangeList ; re ; re=re->r_next) */
/* 		{ */
/* 			//node_id_set(&from, ce->clusterId, re->partitionId); */
/* 			range_dist = dist_graph_get_dist(map->distGraph, &from, &to); */
/* 			if(range_dist <= dist)  */
/* 				nodes += re->r_count; */
/* 		} */
/* 	} */
/* 	return nodes; */
/* } */

int mapperTotalNodeNum(mapper_t map) {
	if(!map) return 0;
	return map->nrNodes;
}
int mapperClusterNodeNum(mapper_t map)
{
	if(!map) return 0;
	return map->nrClusterNodes;
}


/** Return the number of ip ranges this mapper containes. This is usfull to do
 * before calling mapper_get_kernel_map so the size of the needed buffer can be
 * calculated in advace
 @param map  Mapper object
 @return Number of ip ranges
 */
int mapperRangesNum(mapper_t map)
{
	if(!map)
		return 0;
	return map->nrRanges;
}
/** Return the number of clusters this mapper contain
 @param map  Mapper object
 @return Number of clusters
 */
int mapperClusterNum(mapper_t map) {
        if(!map)
                return 0;
        return map->nrClusters;
}




/* int is_cluster_exists(mapper_t map, int cluster_id) */
/* { */
/* 	int i; */
	
/* 	for(i=0; i<map->maxClusters ; i++) */
/* 		if(map->clusterArr[i].c_id == cluster_id) */
/* 			return 1; */
/* 	return 0; */
/* } */
 
 
void mapperPrint(mapper_t map, mapper_print_t t)
{
	return mapperFPrint(stdout, map, t);
}

void mapperFPrint(FILE *f, mapper_t map, mapper_print_t t)
{
	char *buff;

	// should be enough for all maps
	buff = (char*)malloc(4096);
	if(!buff) {
		fprintf(f, "Not enough memory for printing map\n");
		return;
	}
	mapperToString(buff, map, t);
	fprintf(f, "%s", buff);
	free(buff);
}

void mapperToString(char *buff, mapper_t map, mapper_print_t t)
{
	int i;
	char *ptr = buff;
	
	if(!buff)
		return;
	
	if(!map) {
		sprintf(buff, "Map is null\n");
		return;
	}
	
	switch(t) {
	    case PRINT_TXT:
	    case PRINT_TXT_PRETTY:
	    {
		    for(i=0 ; i<map->nrClusters ; i++) {
			    print_cluster_entry(ptr, &map->clusterArr[i], t);
			    ptr+= strlen(ptr);
		    }
		    ptr+= strlen(ptr);
				   
		    //dist_graph_sprintf(ptr, map->distGraph);
	    }
	    break;
	    
	    // Printing the distance map of this node, all the deges that
	    // goes into this node
	    /* case PRINT_TXT_DIST_MAP: */
/* 	    { */
/* 		    node_id_t        from, to; */
/* 		    //pe_t             curr_partition; */
/* 		    int              i; */
/* 		    cluster_entry_t *ce; */
/* 		    range_entry_t   *re; */
		    
/* 		    //node_id_set(&to, map->myClusterId, map->myPartitionId); */
		    
/* 		    for(i=0 ; i < map->nrClusters ; i++) { */
/* 			    ce = &(map->clusterArr[i]); */
/* 			    for(re = ce->rangeList ; re ; re=re->next) */
/* 			    { */
/* 				    int dist; */
/* 				    // We need only the Partition to partitions */
/* 				    if(re->partitionId == curr_partition) */
/* 					    continue; */
/* 				    curr_partition = re->partitionId; */

/* 				    // Getting the distance */
/* 				    node_id_set(&from, ce->clusterId, re->partitionId); */
/* 				    dist = dist_graph_get_dist(map->distGraph, &from, &to); */
/* 				    sprintf(ptr, "c%d:p%d   %d\n", */
/* 					    ce->clusterId, re->partitionId, dist); */
/* 				    ptr+= strlen(ptr); */
/* 			    }	 */
/* 		    } */
/* 	    } */
/* 	    break; */

	    
	    case PRINT_XML:
	    {
		    sprintf(buff, "XML printing is not supported yet\n");
	    }
	    break;
	    default:
		    sprintf(buff, "Unsupported mapper_print_t: %d\n", t);
	}
}


static void print_cluster_entry(char *buff, cluster_entry_t *ce, mapper_print_t t)
{
	range_entry_t *re;
	char *ptr = buff;
	struct in_addr;
	
	if(t == PRINT_TXT_PRETTY) {
		sprintf(ptr,
                        "ClusterID: %-5d Name: %-15s NRnodes: %-3d Prio %-4d\n"
                        "                cango %-1d, cantake %-1d canexpend %-1d\n",  
			ce->c_id, ce->c_name, ce->c_nrNodes, ce->c_info.ci_prio,
                        ce->c_info.ci_cango, ce->c_info.ci_cantake, ce->c_info.ci_canexpend);
	}
	else if (t == PRINT_TXT) {
		sprintf(ptr, "cluster=%d\n", ce->c_id);
	}
		
	ptr += strlen(ptr);
	for(re = ce->c_rangeList ; re ; re = re->r_next) {
		print_range_entry(ptr, re, t);
		ptr += strlen(ptr);
	}
}

static void print_range_entry(char *buff, range_entry_t *re, mapper_print_t t)
{
	struct in_addr in;

	in = re->r_base_addr;
	in.s_addr = htonl(in.s_addr);
	if(t == PRINT_TXT_PRETTY) 
		sprintf(buff,"\t\tbase: %d  ip: %s  count %d\n",
			re->r_base, inet_ntoa(in),
			re->r_count);
	else if(t == PRINT_TXT)
		sprintf(buff, "%d %s %d\n",
			re->r_base, inet_ntoa(in),
			re->r_count);
}

// FIXME take care of cases where my ip(my first ip) is not in the table
/* static pe_t resolve_my_pe_by_addr() */
/* { */
/* 	char              hostname[MAXHOSTNAMELEN+1]; */
/* 	struct hostent   *hostent; */
/* 	struct in_addr    myaddr; */
/* 	int               i, found; */
/* 	pe_t              pe = 0; */
	
/* 	hostname[MAXHOSTNAMELEN] = '\0'; */
/* 	if (gethostname(hostname, MAXHOSTNAMELEN) < 0) { */
/* 		debug_lr(MAP_DEBUG, "gethostname() failed\n"); */
/* 		return 0; */
/* 	} */
/* 	if ((hostent = gethostbyname(hostname)) == NULL) */
/* 	{ */
/* 		if (inet_aton(hostname, &myaddr) == -1) { */
/* 			debug_lr(MAP_DEBUG, "gethostbyname() faild\n"); */
/* 			return 0; */
/* 		} */
/* 	} else { */
/* 		bcopy(hostent->h_addr, &myaddr.s_addr, hostent->h_length); */
/* 		//myaddr = ntohl(myaddr); */
/* 	} */

/* 	myaddr.s_addr = ntohl(myaddr.s_addr); */
	
/* 	// This node ip was resolved, now we search the configuration for */
/* 	// ourself */
/* 	// Iterarting over all the clusters */
/* 	for (i=0, found=0 ; i<glob_map->nrClusters ; i++) */
/* 	{ */
/* 		cluster_entry_t *ce = &glob_map->clusterArr[i]; */
/* 		range_entry_t *re; */

/* 		// Going all over the current cluster ranges */
/* 		for(re = ce->rangeList ; re != NULL ; re = re->next) */
/* 		{ */
/* 			if((myaddr.s_addr >= re->base_addr.s_addr) && */
/* 			   (myaddr.s_addr < (re->base_addr.s_addr + re->count))) */
/* 			{ */
/* 				debug_lg(MAP_DEBUG, "Found my ip in table\n"); */
/* 				pe = PE_SET(ce->clusterId, re->base + myaddr.s_addr - re->base_addr.s_addr); */
/* 				found = 1; */
/* 				break; */
/* 			} */
/* 		} */
/* 		if(found == 1) break; */
/* 	} */
	
/* 	if(!found) { */
/* 		struct in_addr addr; */
/* 		addr.s_addr = htonl(myaddr.s_addr); */
/* 		debug_lr(MAP_DEBUG, "My ip address (%s) is not in map\n", */
/* 			 inet_ntoa(addr)); */
/* 		return 0; */
/* 	} */
/* 	return pe; */
/* } */

/* static int is_my_pe_in_map() */
/* { */
/* 	int               i; */
/* 	pe_t              pe = glob_map->myPe; */
/* 	pe_t              cid = PE_CLUSTER_ID(pe); */
/* 	pe_t              nid = PE_NODE_ID(pe); */
	
/* 	debug_ly(MAP_DEBUG, "Looking for my pe in map (%d)\n", pe); */
/* 	for (i=0 ; i<glob_map->nrClusters ; i++) */
/* 	{ */
/* 		cluster_entry_t *ce = &glob_map->clusterArr[i]; */
/* 		range_entry_t *re; */

/* 		if(cid != ce->clusterId) */
/* 			continue; */
		
/* 		// Going all over the current cluster ranges */
/* 		for(re = ce->rangeList ; re != NULL ; re = re->next) */
/* 		{ */
/* 			if(nid >= re->base && nid < re->base + re->count) */
/* 			{ */
/* 				debug_lg(MAP_DEBUG, "Found my pe in table (%d, %d)\n", */
/* 					 cid, nid); */
/* 				return 1; */
/* 			} */
/* 		} */
/* 	} */
/* 	sprintf(glob_map->errorMsg, "My pe %d is not in map", pe); */
/* 	return 0; */
/* } */



/* All the queries here */
cluster_entry_t *get_cluster_entry_by_name(mapper_t map, const char *clusterName)
{
	cluster_entry_t *ce;
	int i;

	for(i=0 ; i < map->nrClusters ; i++) {
		ce = &(map->clusterArr[i]);
		if(strcmp(ce->c_name, clusterName) == 0)
			return ce;
	}
	return NULL;
}



/** Returning the cluster entry for the cluster_id
 * @parm  cluster_id   Cluster id to get entry for
 * @return Pointer to a valid cluster_entry_t if cluster_id is on mapper
 * @return NULL on error (cluster id is not in mapper)
 * @todo Use binary search here
*/

static cluster_entry_t *get_cluster_entry(mapper_t map, pe_t cluster_id)
{
	cluster_entry_t *ce;
	int i;

	for(i=0 ; i < map->nrClusters ; i++) {
		ce = &(map->clusterArr[i]);
		if(ce->c_id == cluster_id)
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
	
	for(re = ce->c_rangeList ; re ; re=re->r_next)
	{
		if(node_id >= re->r_base && node_id < re->r_base + re->r_count)
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
	
	if(!(ce = get_cluster_entry(map, PE_CLUSTER_ID(pe))))
		return 0;
	if(!(re = get_range_entry(ce, PE_NODE_ID(pe))))
		return 0;
	addr = re->r_base_addr.s_addr;
	addr += PE_NODE_ID(pe) - re->r_base;
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

	if(!(ce = get_cluster_entry(map, PE_CLUSTER_ID(pe))))
		return 0;
	if(!(re = get_range_entry(ce, PE_NODE_ID(pe))))
		return 0;

	addr = re->r_base_addr;
	addr.s_addr += PE_NODE_ID(pe) - re->r_base;
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
		for(re = ce->c_rangeList ; re ; re=re->r_next)
		{
			if(ipaddr >= re->r_base_addr.s_addr &&
			   ipaddr < re->r_base_addr.s_addr + re->r_count)
			{
				pe_t node_id;
				node_id = re->r_base + (ipaddr - re->r_base_addr.s_addr);
				*pe = PE_SET(ce->c_id, node_id);
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

/* int mapper_node_at_dist_pos(mapper_t map, int pos, int dist, mapper_node_info_t *ninfo) */
/* { */
/* 	int               i,nodes_seen = 0; */
/* 	int               range_dist; */
/* 	node_id_t         from, to; */
/* 	cluster_entry_t  *ce; */
/* 	range_entry_t    *re; */

/* 	if(!map) */
/* 		return 0; */
/* 	if(dist < SAME_PARTITION_MAX_DIST || dist > INFINITY_DIST) */
/* 		return 0; */

/* 	node_id_set(&to, map->myClusterId, map->myPartitionId); */
/* 	for(i=0 ; i < map->nrClusters ; i++) */
/* 	{ */
/* 		ce = &(map->clusterArr[i]); */
/* 		for(re = ce->rangeList ; re ; re=re->next) */
/* 		{ */
/* 			node_id_set(&from, ce->clusterId, re->partitionId); */
/* 			range_dist = dist_graph_get_dist(map->distGraph, &from, &to); */
/* 			if(range_dist > dist) */
/* 				continue; */
/* 			if(pos >= nodes_seen && pos <= (nodes_seen + re->count)) */
/* 			{ */
/* 				ninfo->ni_addr.s_addr = */
/* 					re->base_addr.s_addr + (pos - nodes_seen); */
/* 				ninfo->ni_pe = PE_SET(ce->clusterId, re->base + (pos - nodes_seen)); */
/* 				ninfo->ni_partid = re->partitionId; */
/* 				return 1; */
/* 			} */
/* 			nodes_seen += re->count; */
/* 		}	 */
/* 	} */
/* 	return 0; */
/* } */

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
		for(re = ce->c_rangeList ; re ; re=re->r_next)
		{
			if(pos >= nodes_seen && pos <= (nodes_seen + re->r_count))
			{
				ninfo->ni_addr.s_addr =
					re->r_base_addr.s_addr + (pos - nodes_seen);
				ninfo->ni_pe = re->r_base + (pos - nodes_seen);
				return 1;
			}
			nodes_seen += re->r_count;
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
	for(re = ce->c_rangeList ; re ; re=re->r_next)
	{
		if(pos >= nodes_seen && pos <= (nodes_seen + re->r_count))
		{
			ninfo->ni_addr.s_addr =
				re->r_base_addr.s_addr + (pos - nodes_seen);
			ninfo->ni_pe = re->r_base + (pos - nodes_seen);
			
			return 1;
		}
		nodes_seen += re->r_count;
	}
	return 0;
}

/** Given a node ip getting its cluster name
 * @param  map         Map object to use
 * @param  hostname    Buffer containing the host name to translate
 * @param *pe          Pointer to the resulting pe
 * @return 0 on error and 1 on success
 */
int mapper_hostname2cluster(mapper_t map, char  *hostname, char *cluster) {

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
	return mapper_addr2cluster(map, &ipaddr, cluster);
}

int mapper_addr2cluster(mapper_t map, struct in_addr *ip, char *cluster) {
	int              i;
	cluster_entry_t *ce;
	range_entry_t   *re;
	in_addr_t        ipaddr = ntohl(ip->s_addr);
	
	if(!map)
		return 0;
	for(i=0 ; i < map->nrClusters ; i++) {
		ce = &(map->clusterArr[i]);
		for(re = ce->c_rangeList ; re ; re=re->r_next)
		{
			if(ipaddr >= re->r_base_addr.s_addr &&
			   ipaddr < re->r_base_addr.s_addr + re->r_count)
			{
				strcpy(cluster, ce->c_name);
				return 1;
			}
		}	
	}
	return 0;
}

int mapper_setClusterInfo(mapper_t map, const char *clusterName,
			  mapper_cluster_info_t *ci)
{
	cluster_entry_t *ce;
	
	if(!map)
		return 0;
		       
	if(!(ce = get_cluster_entry_by_name(map, clusterName)))
		return 0;
	
	cluster_info_free(&ce->c_info);
	cluster_info_copy(&ce->c_info, ci);
	return 1;
}

// The returned ci is temporary since is 
int mapper_getClusterInfo(mapper_t map, const char *clusterName,
			    mapper_cluster_info_t *ci)
{
	cluster_entry_t *ce;

	if(!map)
		return 0;
	
	if(!(ce = get_cluster_entry_by_name(map, clusterName)))
		return 0;
	cluster_info_copy(ci, &ce->c_info);
	return 1;
}


/* /\** Creating iterator based on distance. its need to be fix  */
/*  *\/ */
/* mapper_iter_t mapper_dist_iter_init(mapper_t map, int dist) { */
/* 	if(!map) */
/* 		return NULL; */

/* 	// FIXME iterator is not correct; */
/* 	if(dist < SAME_PARTITION_MAX_DIST || dist > INFINITY_DIST) */
/* 		return NULL; */
/* 	if(dist <= SAME_PARTITION_MAX_DIST) */
/* 		return mapper_iter_init(map, MAP_ITER_PARTITION); */
/* 	if(dist <= SAME_CLUSTER_MAX_DIST) */
/* 		return mapper_iter_init(map, MAP_ITER_CLUSTER); */
/* 	//if(dist <= MAX_KNOWN_DIST) */
/* 	//      return mapper_iter_init(map, MAP_ITER_ALL); */
/* 	return mapper_iter_init(map, MAP_ITER_ALL); */
/* } */

/** Creating a new iterator over the map

*/
mapper_iter_t mapperIter(mapper_t map, mapper_iter_type_t iter_type)
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
		    iter->currRangeEnt = map->clusterArr[0].c_rangeList;
		    break;
	    case MAP_ITER_CLUSTER:
		    iter->currClusterEnt = map->myClusterEnt;
		    iter->currRangeEnt = map->myClusterEnt->c_rangeList;
		    break;
	    default:
		    return NULL;
		    
	}

	iter->currNodeId = iter->currRangeEnt->r_base;
	return iter;
}

mapper_iter_t mapperClusterNodesIter(mapper_t map, const char *clusterName) {
	mapper_iter_t      iter;
	cluster_entry_t   *ce;
	
	if(!map)
		return NULL;

	ce = get_cluster_entry_by_name(map, clusterName);
	if(!ce)
		return NULL;
	
	if(!(iter = malloc(sizeof(struct mapper_iter_struct))))
		return NULL;
	
	iter->map = map;
	iter->type = MAP_ITER_CLUSTER;


	iter->currClusterEnt = ce;
	iter->currRangeEnt = ce->c_rangeList;

	iter->currNodeId = iter->currRangeEnt->r_base;
	return iter;

}


void mapperIterDone(mapper_iter_t iter) {
	free (iter);
}

int mapperNext(mapper_iter_t iter, mapper_node_info_t *node_info)
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
	if(node_id >= re->r_base && node_id < (re->r_base + re->r_count))
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
		    if(re->r_next) {
			    iter->currRangeEnt = re->r_next;
			    iter->currNodeId = re->r_next->r_base;	    
		    }
		    else {
			    iter->currClusterPos++;
			    if(iter->currClusterPos < iter->map->nrClusters)
			    {
				    iter->currClusterEnt++;
				    iter->currRangeEnt = iter->currClusterEnt->c_rangeList;
				    iter->currNodeId = iter->currRangeEnt->r_base;
			    }
			    
		    }
		    break;
	    case MAP_ITER_CLUSTER:
		    if(re->r_next) {
			    iter->currRangeEnt = re->r_next;
			    iter->currNodeId = re->r_next->r_base;	    
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

mapper_cluster_iter_t mapperClusterIter(mapper_t map)
{
	mapper_cluster_iter_t iter;
	
	if(!map)
		return NULL;
	if(!(iter = malloc(sizeof(struct mapper_cluster_iter_struct))))
		return NULL;
	
	iter->map = map;
	iter->currClusterIndex = 0;
	return iter;
}


void mapperClusterIterDone(mapper_cluster_iter_t iter) {
	free(iter);
}

int mapperClusterIterNext(mapper_cluster_iter_t iter, char **clusterName) {
	if(!iter)
		return 0;
	
	int index = iter->currClusterIndex;

	if(index < iter->map->nrClusters) {
		strcpy(iter->clusterName, iter->map->clusterArr[index].c_name);
		*clusterName = iter->clusterName;
		iter->currClusterIndex++;
		return 1;
	}
	return 0;
}

int mapperClusterIterRewind(mapper_cluster_iter_t iter) {
	if(!iter)
		return 0;

	iter->currClusterIndex = 0;
	return 1;
}


mapper_range_iter_t mapperRangeIter(mapper_t map, const char *clusterName )
{
	mapper_range_iter_t iter;
	
	if(!map || !clusterName) return NULL;

	cluster_entry_t *ce = get_cluster_entry_by_name(map, clusterName);
	if(!ce) return NULL;

	if(!(iter = malloc(sizeof(struct mapper_range_iter_struct))))
		return NULL;
	
	iter->map = map;
	iter->ce = ce;
	iter->re = ce->c_rangeList;

	return iter;
}
	
void mapperRangeIterDone(mapper_range_iter_t iter) {
	if(iter)
		free(iter);
}
int mapperRangeIterNext(mapper_range_iter_t iter,
			mapper_range_t *mr, mapper_range_info_t *ri)
{
	if(!iter || !mr || !ri)
		return 0;
	
	if(iter->re) {
		range_info_copy(ri, &(iter->re->r_info));
		mapper_range_init(mr, iter->re);
		iter->re = iter->re->r_next;
		return 1;
	}
	return 0;
}
int mapperRangeIterRewind(mapper_range_iter_t iter) {
	if(!iter)
		return 0;
	iter->re = iter->ce->c_rangeList;
	return 1;
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
	if(node_id < re->r_base || node_id >= (re->r_base + re->r_count))
		return 0;
	pos = node_id - re->r_base;
	ni->ni_addr.s_addr =  re->r_base_addr.s_addr + pos;
	// The ip is converted to network order
	ni->ni_addr.s_addr = htonl(ni->ni_addr.s_addr);
	ni->ni_pe = PE_SET(ce->c_id, node_id);
	return 1;
}

/** Getting the distance from the given node to the local node (the one the
    mapper is about.
 @param map       Mapper object
 @param node_pe   Pe of node to check
 @return INVALID_DIST on error and distance between the nodes on success
 */

/* int mapper_node_dist(mapper_t map, pe_t node_pe) */
/* { */
/* 	node_id_t        from, to; */
/* 	cluster_entry_t  *ce; */
/* 	range_entry_t    *re; */

/* 	ce = get_cluster_entry(map, PE_CLUSTER_ID(node_pe)); */
/* 	if(!ce) return INVALID_DIST; */
/* 	re = get_range_entry(ce, PE_NODE_ID(node_pe)); */
/* 	if(!re) return INVALID_DIST; */
	
/* 	node_id_set(&to, map->myClusterId, map->myPartitionId); */
/* 	node_id_set(&from, ce->clusterId, re->partitionId); */
/* 	return dist_graph_get_dist(map->distGraph, &from, &to); */
/* } */

/* /\** Returning the maximal distance between two nodes */
/*     (the largest distance that is not INFINITY_DIST) */
/*     @param map   Mapper object */
/*     @return maximal distance  */
/*  *\/ */
/* int mapper_get_max_dist(mapper_t map) */
/* { */
/* 	// FIXME currently we only support one level grid */
/* 	return SAME_GRID_MAX_DIST + 2; */
/* } */

static 
int addr_cmp(const void *A, const void *B)
{
        struct in_addr *a = (struct in_addr *) A;
        struct in_addr *b = (struct in_addr *) B;

        return a->s_addr - b->s_addr;
}
/** Getting the mapper crc32 checksum so we can compare 2 clusters.
 *  The crc32 is done over a sorted array of all the mapper nodes.
 *  so attributes such as proximate get lost
 * @param *map      Mapper object
 * @return crc32 checksum of the mapper
*/
unsigned int mapperGetCRC32(mapper_t map)
{
        int                n;
        struct in_addr    *addr_arr;
        mapper_node_info_t ni;
        mapper_iter_t      iter;

        if(!map)
                return 0;
        
        n = mapperTotalNodeNum(map);
        if(n <=0 )
                return 0;

        addr_arr = malloc(n*sizeof(struct in_addr));
	if(!addr_arr)
		return 0;
	
	iter = mapperIter(map, MAP_ITER_ALL);
	if(!iter) {
		free(addr_arr);
		return 0;
	}
        int i = 0;
        while(mapperNext(iter, &ni)) {
                addr_arr[i++] = ni.ni_addr;
        }
        mapperIterDone(iter);
        qsort(addr_arr, n, sizeof(struct in_addr), addr_cmp);
        unsigned int crc = crc32(addr_arr, n * sizeof(struct in_addr));
        free(addr_arr);
        return crc;
}


/** @} */ // end of mapper module
