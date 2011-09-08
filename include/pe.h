/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __UMOSIX_PE__
#define __UMOSIX_PE__

// The pe is 24 bits and we use 9 for cluster_id and 15 for node is inside the
// cluster


#define PE_SIZE            (24)
#define CLUSTER_ID_SIZE    (9)
#define NODE_ID_SIZE       (PE_SIZE - CLUSTER_ID_SIZE)

#define CLUSTER_ID_BITS    ((1<<(CLUSTER_ID_SIZE)) -1)
#define NODE_ID_BITS       ((1<<(NODE_ID_SIZE)) -1)

// The Maximal PE
#define   MAX_PE            ((1<< PE_SIZE) -1)
// Maximum number of cluster which is 
#define   MAX_CLUSTERS      ((1<< CLUSTER_ID_SIZE) -1)
// Maximum number of nodes in a single cluster
#define   MAX_NODES         ((1<< NODE_ID_SIZE) -1 )

// Maximum number of partitions which is the same as maximum number of nodes
// in a cluster
#define   MAX_PARTITIONS    (MAX_NODES)


#define ONLY_CLUSTER   (MAX_PARTITIONS + 10)
#define ALL_PARTITIONS (MAX_PARTITIONS + 11)



/* obtain the cluster ID from the pe */
#define PE_CLUSTER_ID(pe) (((pe) >> NODE_ID_SIZE) & (CLUSTER_ID_BITS))
/* obtain the id of the machine in the local cluster (the regular pe) */
#define PE_NODE_ID(pe) ((pe) & NODE_ID_BITS)
/* creating a pe from cluster id and node id */
#define PE_SET(cid,nid) ((((cid) & CLUSTER_ID_BITS) << NODE_ID_SIZE) | PE_NODE_ID(nid))

/* Creating a ownner (pe) from cluster id and partition id to identify owner*/
#define OWNER_SET(cid,pid) ((((cid) & CLUSTER_ID_BITS) << NODE_ID_SIZE) | PE_NODE_ID(pid))

typedef unsigned int pe_t;
typedef pe_t mnode_t;


typedef struct _node_id
{
	pe_t  clusterId;
	pe_t  partitionId;
} node_id_t;

inline void node_id_set(node_id_t *n, pe_t cluster_id, pe_t partitionId);
void node_id_to_str(node_id_t *n, char *str);
int node_id_equal(node_id_t *n1, node_id_t *n2);
int same_cluster_nodes(node_id_t *n1, node_id_t *n2);
int cluster_only_node(node_id_t *n);



#define CLUSTER_PART_SEP_CHR ':'
#define ALL_PARTITIONS_CHR   '*'


int parse_node_str(char *str, node_id_t *node);
int get_cluster_id(char *str, pe_t *val);
int get_partition_id(char *str, pe_t *val);

/** Format of 12121 or 1:1 or c1:n2 is acceptable.
    resulting pe is a real pe
 */
int parse_pe_str(char *pe_str, pe_t *pe);

#endif /* __UMOSIX_PE__ */
