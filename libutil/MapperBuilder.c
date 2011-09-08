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
#include <pe.h>
//#include <distance_graph.h>

#include <Mapper.h>
#include <MapperInternal.h>

int mapper_range_init(mapper_range_t *mr, range_entry_t *re) {
	mr->baseIp.s_addr = htonl(re->r_base_addr.s_addr);
	mr->baseId = re->r_base;
	mr->count  = re->r_count;
	return 1;
};

int range_info_copy(mapper_range_info_t *dest, mapper_range_info_t *src)
{
	*dest = *src;
	return 1;
}

int cluster_info_copy(mapper_cluster_info_t *dest, mapper_cluster_info_t *src)
{
	// Cant do self copy
	if(dest == src)
		return 0;
	// Freeing the current description
	//if(dest->ci_desc) {
	//free(dest->ci_desc);
	//}

	*dest = *src;
	if(dest->ci_desc)
		dest->ci_desc = strdup(src->ci_desc);
	return 1;
}

void cluster_info_init(mapper_cluster_info_t *ci)
{
	ci->ci_desc = NULL;
}


void cluster_info_free(mapper_cluster_info_t *ci)
{
	if(ci->ci_desc) {
		free(ci->ci_desc);
                ci->ci_desc = NULL;
        }
	
}

/** Init function
 * @param max_ranges   Number of maximum node ranges to support.
 *                     if equal 0 the default is used
 * @return An initialized mapper object
 */
mapper_t mapperInit(int max_ranges) {
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

	map->nrClusters = 0;
	map->nrNodes = 0;
	map->maxClusters = MAPPER_MAX_CLUSTERS;

	map->myPe = 0;
	map->myIPSet = 0;
	map->myClusterId = 0;
	map->myClusterEnt = NULL;
	map->myRangeEnt = NULL;
	
	// Allocating the cluster array
	map->clusterArr = malloc(sizeof(cluster_entry_t) * map->maxClusters);
	if(!map->clusterArr) return NULL;
	
	bzero(map->clusterArr, sizeof(cluster_entry_t) * map->maxClusters);

	// Allocating the rangeCache object
	debug_lb(MAP_DEBUG, "Max ranges %d\n", max_ranges);
	map->rangeCache = mem_cache_create(max_ranges, sizeof(range_entry_t));
	if(!map->rangeCache)
		return NULL;

	//map->distGraph = dist_graph_init(NULL);
	//if(!map->distGraph)
	//return NULL;

	sprintf(map->errorMsg, "No error\n");
	return map;
}

/** Destroy a cluster entry
 * Destroy a cluster entry and free its memory
 * @param  cEnt  A pointer to the cluster entry to free
 * @return  0   on error
 * @return  1   If all went well
 */
int clusterEntryFree(cluster_entry_t *cEnt)
{
     if(!cEnt)
          return 0;
     if(cEnt->c_name)
          free(cEnt->c_name);
     if(cEnt->c_desc)
          free(cEnt->c_desc);
     cluster_info_free(&(cEnt->c_info));
     return 1;
}

/** Destroy the map object
 * Destroy the map object and free all its taken memory
 * @param  map  The mapper object to destroy
 * @return  0   on error
 * @return  1   If all went well
 */
int mapperDone(mapper_t map)
{
        
	if(!map)
		return 0;
	
	mem_cache_destroy(map->rangeCache);
	//dist_graph_done(map->distGraph);

        for(int i=0; i< map->nrClusters ; i++) {
                clusterEntryFree(&map->clusterArr[i]);
        }
        free(map->clusterArr);
	free(map);
	return 1;
}

/** Copy a map object
 * Duplicate a map object
 * @param  map  The mapper object to copy
 * @return  0   on error
 * @return  1   If all went well
 */
mapper_t mapperCopy(mapper_t map) {
	int      i;
	mapper_t newMap;

	if(!map)
		return NULL;
	newMap = mapperInit(0);

	// Going over each cluster and adding it and copying it
	for(i=0 ; i<map->nrClusters ; i++) {
		cluster_entry_t *ce = &(map->clusterArr[i]);
		cluster_entry_t *new_ce;
		range_entry_t   *re;

		debug_lb(MAP_DEBUG, "Adding cluste %s to new map\n", ce->c_name);
		
		if(!mapper_addCluster(newMap, ce->c_id, ce->c_name, &ce->c_info)) {
			debug_lr(MAP_DEBUG, "Error adding cluster %s to new map\n", ce->c_name);
			return 0;
		}
		new_ce = get_cluster_entry_by_name(newMap, ce->c_name);
		for(re = ce->c_rangeList ; re ; re=re->r_next)
		{
			// We need to convert since the mapper_addClusterAddrRange expect to get the address
			// in network order and inside the mapper the address is stored in host order
			struct in_addr  ipaddr = re->r_base_addr;
			ipaddr.s_addr = htonl(ipaddr.s_addr); 
			mapper_addClusterAddrRange(newMap, new_ce, re->r_base, &ipaddr,
						   re->r_count, &re->r_info);
		}
	}
	return newMap;
}


int mapper_addCluster(mapper_t map, int id, char *name, mapper_cluster_info_t *ci)
{
	int  i;
	cluster_entry_t *ent;
	
	if(map->nrClusters >= map->maxClusters) {
		debug_lr(MAP_DEBUG, "Too many clusters in mapper\n");
		return 0;
	}
	
	// Checking that the cluster is not already there
	ent = get_cluster_entry_by_name(map, name);
	if(ent)
		return 0;
	
	
	i = map->nrClusters;
	map->nrClusters ++;

	if(id >=  0)
		map->clusterArr[i].c_id = id;
	else 
		map->clusterArr[i].c_id = 1+i;

	map->clusterArr[i].c_name = strdup(name);
	map->clusterArr[i].c_nrNodes = 0;
	map->clusterArr[i].c_rangeList = NULL;
        map->clusterArr[i].c_desc = NULL;
	cluster_info_copy(&map->clusterArr[i].c_info, ci);
	return 1;
}


int mapper_addClusterAddrRange(mapper_t map, cluster_entry_t *ce,
			       int base, struct in_addr *ip, int count,
			       mapper_range_info_t *ri)
{
	range_entry_t   *re, *re_ptr;
	struct in_addr  ipaddr = *ip;
	// Once we have the ip we check for wrapping 
	// Also converting to host order so we can perform calculations
	ipaddr.s_addr = ntohl(ipaddr.s_addr);
	if(ipaddr.s_addr + count < ipaddr.s_addr) {
		debug_lr(MAP_DEBUG, "Range has ip wrapping\n");
		return 0;
	}
	
	// Getting a free range entry
	re = mem_cache_alloc(map->rangeCache);
	if(!re) {
		debug_lr(MAP_DEBUG, "Not enough ranges in cache\n");
		return 0;
	}

	// Setting the fields inside
	//re->partitionId = part_id;
	re->r_base = base;
	re->r_count = count;
	re->r_base_addr = ipaddr;
	re->r_next = NULL;
	range_info_copy(&re->r_info, ri);

	// Adding to the list at the end
	if(!ce->c_rangeList)
		ce->c_rangeList = re;
	else {
		for(re_ptr = ce->c_rangeList ; re_ptr->r_next ; re_ptr = re_ptr->r_next);
		re_ptr->r_next = re;
	}
	debug_lg(MAP_DEBUG, "Adding range %s %d to %s\n", inet_ntoa(*ip), count, ce->c_name);
	return 1;
}
 

int mapper_addClusterHostRange(mapper_t map, cluster_entry_t *ce,
			       int base, char *host_name, int count,
			       mapper_range_info_t *ri)
{
	struct hostent  *hostent;
	struct in_addr ipaddr;

	if(PE_CLUSTER_ID(base)) {
		debug_lr(MAP_DEBUG, "Base number is too big (contain cluster id bits)\n");
		return 0;
	}
	// Resolving the hostname/ipaddress
	// FIXME move this check elsewhere
	if ((hostent = gethostbyname(host_name)) == NULL) {
		if (inet_aton(host_name, &ipaddr) == 0)
		{
			//herror(host_name);
			debug_lr(MAP_DEBUG, "Error host name is not resolvable\n");
			return 0;
		}
	} else {
		bcopy(hostent->h_addr, &(ipaddr.s_addr), hostent->h_length);
		// FIXME Why we have this line here ?????
		//ipaddr = ntohl(ipaddr);
	}
	return mapper_addClusterAddrRange(map, ce, base, &ipaddr, count, ri);
}


static
char *mapperGetCmdOutput(const char *map_cmd, int *res_size)
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

		if(res == 0)
			break;
		read_size += res;
		
		if(read_size == size) {
			size += step_size;
			if(!(buff = (char *) realloc(buff, size))) {
				debug_lr(MAP_DEBUG, "Error reallocating cmd buff\n");
				goto failed;
			}
			ptr+= strlen(ptr);
		}
	}
	
	debug_lg(MAP_DEBUG, "Setting from cmd read_size =  %d\n", read_size);
	*res_size = read_size;
	pclose(cmd_output);
	buff[read_size]='\0';
	return buff;

 failed:
	pclose(cmd_output);
	free(buff);
	return NULL;;
}


/** Setting the mapper from a file. The whole file is read into memory
    and then parsed.
 @param map       The mapper object
 @param filename  File name to use
 @return  0 on error 1 on succes
*/
static
char *mapperReadFile(const char* filename, int *res_size)
{
	int fd = -1;
	struct stat s;
	char *buff = NULL;
//	int   res = 0;
	
	if(stat(filename, &s) == -1)
		goto failed;

	if(!(buff = malloc(s.st_size+1)))
		goto failed;
	
	if((fd = open(filename, O_RDONLY)) == -1)
	goto failed;
    
	if(read(fd, buff, s.st_size) != s.st_size)
		goto failed;
	
	buff[s.st_size]='\0';

	close(fd);
	*res_size = s.st_size+1;
	return buff;

 failed:
	free(buff);
	close(fd);
	return NULL;
}


char *getMapSourceBuff(const char *buff, int size, map_input_type type, int *new_size) {
	char *res_buff;
	switch(type) {
	    case INPUT_MEM:
		    *new_size = size;
		    res_buff = strdup((char *)buff);
		    break;
	    case INPUT_FILE:
		    res_buff = mapperReadFile(buff, new_size);
		    break;
	    case INPUT_FILE_BIN:
		    res_buff = mapperReadFile(buff, new_size);
		    if(res_buff)
			    (*new_size)--;
		    break;
	    case INPUT_CMD:
		    res_buff = mapperGetCmdOutput(buff, new_size);
		    break;
	    default:
		    return NULL;
	}
	return res_buff;
}


static int
ranges_ip_intersect(range_entry_t *re1, range_entry_t *re2)
{
	//debug_lb(MAP_DEBUG, " %u %u %u %u\n",
	// re1->r_base_addr.s_addr, re1->r_count, re2->r_base_addr.s_addr, re2->r_count);
	
	if( (re1->r_base_addr.s_addr <= re2->r_base_addr.s_addr &&
	     re1->r_base_addr.s_addr + re1->r_count > re2->r_base_addr.s_addr) ||
	    (re2->r_base_addr.s_addr <= re1->r_base_addr.s_addr &&
	     re2->r_base_addr.s_addr + re2->r_count > re1->r_base_addr.s_addr))
	{
		debug_lr(MAP_DEBUG, "ranges IP intersect\n");
		return 1;
	}
	return 0;
}

static int
clusters_ips_intersect(cluster_entry_t *ce1, cluster_entry_t *ce2)
{
	range_entry_t *re1;
	range_entry_t *re2; 
	
	for(re1 = ce1->c_rangeList ; re1 ; re1 = re1->r_next)
		for(re2 = ce2->c_rangeList ; re2 ; re2 = re2->r_next)
			if(ranges_ip_intersect(re1, re2))
			{
				debug_lr(MAP_DEBUG, "c %d intersect with c %d\n",
					 ce1->c_id, ce2->c_id);
				return 1;
			}
	return 0;
}
	
static int
ranges_nodeId_intersect(range_entry_t *re1, range_entry_t *re2)
{
	if( (re1->r_base <= re2->r_base && re1->r_base + re1->r_count > re2->r_base) ||
	    (re2->r_base <= re1->r_base  && re2->r_base + re2->r_count > re1->r_base))
	{
		debug_lr(MAP_DEBUG, "ranges nodeID intersect \n");
		return 1;
	}
	return 0;
}

// Now we need to check if the internal cluster ranges are valid
static int
is_cluster_ranges_valid(cluster_entry_t *ce)
{
	range_entry_t *re1, *re2;
	
	for(re1 = ce->c_rangeList ; re1 ; re1 = re1->r_next)
		for(re2 = re1->r_next ; re2 ; re2 = re2->r_next)
			if(ranges_ip_intersect(re1, re2) ||
			   ranges_nodeId_intersect(re1,re2))
				return 0;
	return 1;
}

/*
   Checking if the map is valid:
     o  The same ip can not appear in two clusters
     o  In the same cluster no intersection is allowed at all
*/
int isMapValid(mapper_t map)
{
	cluster_entry_t *ce, *ce2;
	int  i, j;

	// Checking that the map is not empty (empty map is not valid)
	if(map->nrClusters == 0 || !map->clusterArr[0].c_rangeList)
		return 0;
	
	// Checking that the same ip does not exists in two clusters
	for(i=0 ; i< map->nrClusters ; i++)
	{
		ce = &(map->clusterArr[i]);
		for(j=i+1 ; j< map->nrClusters ; j++)
		{
			ce2 = &(map->clusterArr[j]);
			if(clusters_ips_intersect(ce, ce2))
				return 0;
		}
	}
	// Now checking each cluster seperatly
	for(i=0 ; i< map->nrClusters ; i++)
	{
		ce = &(map->clusterArr[i]);
		if( !is_cluster_ranges_valid(ce) )
			return 0;
	}
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

	//mapper_printf(map, PRINT_TXT);
	
	for(re1 = ce->c_rangeList ; re1 ; re1 = re1->r_next)
	{
		for(re2 = ce->c_rangeList;  re2 != re1; re2 = re2->r_next)
		{
			// re2next must exists since re1 is there.
			re2next = re2->r_next;
			// We need to swap the ranges
			if(re2->r_base > re2next->r_base) {
				// Swapping only values of the structs without
				// the value of the next pointer
				tmp = *re2next;
				*re2next = *re2;
				re2next->r_next = tmp.r_next;
				*re2 = tmp;
				re2->r_next = re2next;
			}
		}
	}
}
/// Comparison function for qsort
static int sort_by_clusterId(const void *a, const void *b)
{
	cluster_entry_t *ca = (cluster_entry_t *)a;
	cluster_entry_t *cb = (cluster_entry_t *)b;

	if(ca->c_id < cb->c_id)
		return -1;
	if (ca->c_id == cb->c_id)
		return 0;
	return 1;
}

/** Using qsort to sort the cluster array and then each cluster ranges by pe
 * order
 */
static void sort_cluster_arr(mapper_t map)
{
	int i;
	qsort(map->clusterArr, map->nrClusters,
	      sizeof(cluster_entry_t), sort_by_clusterId);
	
	for(i=0 ; i<map->nrClusters ; i++)
	{
		sort_cluster_ranges(&(map->clusterArr[i]));
	}
}

/** Updating general information of the mapper object
 * Information such as the total number of nodes, the number of nides in each
 * cluster and so on.
 */
void mapperFinalizeBuild(mapper_t map)
{
	int              i,tnodes = 0;
	cluster_entry_t *ce;
	range_entry_t   *re;
	//pe_t             my_cluster_id;
	//pe_t             my_node_id;

	in_addr_t        ipaddr;
	
	sort_cluster_arr(map);
	
	//my_cluster_id = PE_CLUSTER_ID(map->myPe);
	//my_node_id = PE_NODE_ID(map->myPe);

	map->nrRanges = 0;
	//map->maxPe = 0;
	map->myClusterId = 0;
	map->myClusterEnt = NULL;
	map->myRangeEnt = NULL;

	if(map->myIPSet) 
		ipaddr = ntohl(map->myIP.s_addr);
	
	for(i=0 ; i<map->nrClusters ; i++) {
		int cluster_nodes = 0;
		ce = &map->clusterArr[i];
		for(re = ce->c_rangeList ; re ; re=re->r_next)
		{
			// Calculating max pe;
			//if(re->r_base + re->r_count -1 > map->maxPe)
			//map->maxPe = re->r_base + re->r_count -1;
			
			if(map->myIPSet && ipaddr >= re->r_base_addr.s_addr &&
			   ipaddr < re->r_base_addr.s_addr + re->r_count)
			{
				map->myClusterEnt = ce;
				map->myRangeEnt = re;
			}
			cluster_nodes += re->r_count;
			map->nrRanges++;
		}
		ce->c_nrNodes = cluster_nodes;
		tnodes += cluster_nodes;
	}
	map->nrNodes = tnodes;
	
	// Updating the nrClusterNodes (the partition id is now known)
	if(map->myClusterEnt) {
		map->nrClusterNodes = 0;
		for(re = map->myClusterEnt->c_rangeList ; re ; re = re->r_next) {
			map->nrClusterNodes += re->r_count;
		}
	}
}
