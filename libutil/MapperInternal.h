/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


/*****************************************************************************
  Copyright (c) 2000, 2001, 2002, 2003 Amnon BARAK (amnon@MOSIX.org).
  All rights reserved.
  This software is subject to the MOSIX SOFTWARE LICENSE AGREEMENT.
  
  MOSIX $Id: MapperInternal.h,v 1.4 2006-04-10 16:27:36 lior Exp $
 
  THIS SOFTWARE IS PROVIDED IN ITS "AS IS" CONDITION, WITH NO WARRANTY
  WHATSOEVER. NO LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
  FROM THE USE OF THIS SOFTWARE WILL BE ACCEPTED.
 
  Author(s): Amar Lior
  Based on msx_map.h.
****************************************************************************/

#ifndef __MAPPER_INTERNAL_H
#define __MAPPER_INTERNAL_H





#include "Mapper.h"
#include <mem_cache.h>

/** range object struct
 * Contain all the range information
 */
typedef struct _range_entry
{
	struct in_addr   r_base_addr;  // holds the basee address in host byte order
	int              r_base;       // The possible node id (PE) of node
	int              r_count;      // Total nodes in range
	char             r_selected;  // For the selected iterator

	struct _range_entry *r_next;
	mapper_range_info_t r_info;

} range_entry_t;

typedef struct _cluster_entry
{
	int               c_id;      // Possible id of cluster
	char             *c_name;
	char             *c_desc;    // Possible free text description of cluster
	int               c_nrNodes; // Total nodes in cluster

	mapper_cluster_info_t c_info;
	
	range_entry_t    *c_rangeList; // The cluster ranges
} cluster_entry_t;

/** mapper object struct
 * Contain all the mapper object needs 
 */
struct mapper_struct
{
	mem_cache_t      rangeCache;  // A cache object for the pe ranges 
	//dist_graph_t     distGraph;
	char             errorMsg[256];
	
	int              nrClusters;  // Total number of clusters
	int              maxClusters; // Maximum number of allowed cluster
	int              nrNodes;        // Total nodes in map
	int              nrClusterNodes; // Total nodes in local cluster
	
	int              nrRanges;   // Number of ip ranges this mapper contain
	//pe_t             maxPe;      // Maximum pe in map
	
	int              myPe;
	int              myIPSet;
	struct           in_addr myIP;
	int              myClusterId;
	
	cluster_entry_t  *myClusterEnt;
	range_entry_t    *myRangeEnt;
	
	cluster_entry_t  *clusterArr; // Array of all clusters
};


// Copy one cluster info to the other (allocate memory if necessary)
int mapper_range_init(mapper_range_t *mr, range_entry_t *re);
int cluster_info_copy(mapper_cluster_info_t *dest, mapper_cluster_info_t *src);

int range_info_copy(mapper_range_info_t *dest, mapper_range_info_t *src);

cluster_entry_t *get_cluster_entry_by_name(mapper_t map, const char *clusterName);

int mapper_addCluster(mapper_t map, int id, char *name, mapper_cluster_info_t *ci);

int mapper_addClusterAddrRange(mapper_t map, cluster_entry_t *ce,
			       int base, struct in_addr *ip, int count,
			       mapper_range_info_t *ri);
int mapper_addClusterHostRange(mapper_t map, cluster_entry_t *ce,
			       int base, char *host_name, int count,
			       mapper_range_info_t *ri);

char *getMapSourceBuff(const char *buff, int size, map_input_type type, int *new_size);
int  isMapValid(mapper_t map);
void mapperFinalizeBuild(mapper_t map);


#endif
