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
#include <string.h>
#include <ctype.h>

#include <msx_error.h>
#include <msx_debug.h>
#include <parse_helper.h>
#include <pe.h>

/** Setting the node_id_t variable from the cluster id and partition id
   @param *n             Pointer to node_id_t struct
   @param cluster_id     The cluster id to use
   @param partition_id   The partition id to use
*/
inline void node_id_set(node_id_t *n, pe_t cluster_id, pe_t partition_id)
{
	n->clusterId = cluster_id;
	n->partitionId = partition_id;
}

/** Transform the node_id to a string
   @param  *n     Pointer to node_id_t variable
   @return *str   Pointer to a char buff
 */
void node_id_to_str(node_id_t *n, char *str)
{
	char *ptr = str;
	sprintf(ptr, "c%d", n->clusterId);
	ptr += strlen(ptr);
	
	if(cluster_only_node(n))
		return;
	sprintf(ptr, "%c", CLUSTER_PART_SEP_CHR);
	ptr += strlen(ptr);
	if(n->partitionId == ALL_PARTITIONS)
		sprintf(ptr, "%c", ALL_PARTITIONS_CHR);
	else
		sprintf(ptr, "p%d", n->partitionId);
}

/**
   @param
   @return
 */
int node_id_equal(node_id_t *n1, node_id_t *n2)
{
	if((n1->clusterId == n2->clusterId) &&
	   ((n1->partitionId == n2->partitionId) ||
	    (n1->partitionId == ALL_PARTITIONS) ||
	    (n2->partitionId == ALL_PARTITIONS)))
		return 1;
	return 0;
}

/**
   @param
   @return
 */
int same_cluster_nodes(node_id_t *n1, node_id_t *n2)
{
	if(n1->clusterId == n2->clusterId)
		return 1;
	return 0;
}

/**
   @param
   @return
 */
int cluster_only_node(node_id_t *n)
{
	if(n->partitionId == ONLY_CLUSTER)
		return 1;
	return 0;
}

/** Extracting the cluster id from a string. Trimming and eating white spaces
 * is done. The string can start with 'c' which is skeepd.
 * @param str  The cluster id string
 * @param val  The cluster id value
 * @return 0 on error 1 on succes
 */
int get_cluster_id(char *str, pe_t *val)
{
	char *ptr = str;;
	char *end_ptr;
	pe_t v;
	
	ptr = eat_spaces(ptr);
	trim_spaces(ptr);

	debug_lg(MAP_DEBUG, "clusterID str: '%s'\n", ptr);

	// Skeeping the primary C or c
	if(tolower(*ptr) == 'c')
		ptr++;
	// Converting to numbers
	v = strtol(ptr, &end_ptr, 0);
	if(*end_ptr != '\0') {
		debug_lr(MAP_DEBUG,
			 "The cluster string was not made only of numbers\n");
		return 0;
	}
	*val = v;
	return 1;
}

/** Extracting the partition id from a string and putting it in val.
 * The partition id can start with p and can be the special value "*"
 * which means all the partitions
 *
 * @param str  The partition id string
 * @param val  The partition id value (+ the special value ALL_PARTITIONS)
 * @return 0 on error or 1 on succes
 */
int get_partition_id(char *str, pe_t *val)
{
	char *ptr;
	char *end_ptr;
	pe_t v;
	
	ptr = eat_spaces(str);
	trim_spaces(str);

	debug_lg(MAP_DEBUG, "PartitionID str: '%s'\n", ptr);

	// Skeeping the primart C or P
	if(tolower(*ptr) == 'p')
		ptr++;

	// Cheking if the partition is the ALL_PARTITIONS
	if(strlen(ptr) == 1 && *ptr == ALL_PARTITIONS_CHR)
	{
		*val = ALL_PARTITIONS;
		return 1;
	}
	// Converting to numbers
	v = strtol(ptr, &end_ptr, 0);
	if(*end_ptr != '\0') {
		debug_lr(MAP_DEBUG,
			 "The partition string was not made only of numbers '%s'\n", ptr);
		return 0;
	}
	*val = v;
	return 1;
}
/** Extracting the partition id from a string and putting it in val.
 * The partition id can start with p and can be the special value "*"
 * which means all the partitions
 *
 * @param str  The partition id string
 * @param val  The partition id value (+ the special value ALL_PARTITIONS)
 * @return 0 on error or 1 on succes
 */
int get_node_id(char *str, pe_t *val)
{
	char *ptr;
	char *end_ptr;
	pe_t v;
	
	ptr = eat_spaces(str);
	trim_spaces(str);

	// Skeeping the primart N
	if(tolower(*ptr) == 'n')
		ptr++;
	
	// Converting to numbers
	v = strtol(ptr, &end_ptr, 0);
	if(*end_ptr != '\0')
		return 0;
	*val = v;
	return 1;
}


/** Parsing a node string to find the cluster and the partition
 * The format sould be cluster_id:partition_id where cluster_id
 * and partition_id is a positive number
 * @param str   The node string
 * @param node  Pointer to the resulting node id struct
 * @return 0 on error 1 on success (and the resulting node is
 *         in the node struct
*/
int parse_node_str(char *str, node_id_t *node)
{
	char *ptr;
	char *cluster_ptr, *partition_ptr;
	char buff[81];
	
	strncpy(buff, str, 80);
	// Finding the seperator
	if(!(ptr = strchr(buff, CLUSTER_PART_SEP_CHR)))
	{
		debug_lr(MAP_DEBUG,
			 "Seperator of cluster partition was not found\n");
		if(!get_cluster_id(buff, &node->clusterId)) {
			debug_lr(MAP_DEBUG, "No valid cluster\n");
			return 0;
		}
		// We have a valid cluster but no partition
		node->partitionId = ONLY_CLUSTER;
		return 1;
	}
	// Getting the two number string (cluster,part) 
	partition_ptr = ptr + 1;
	*ptr = '\0';
	cluster_ptr = buff;
	
	if(!get_cluster_id(cluster_ptr, &node->clusterId) ||
	   !get_partition_id(partition_ptr, &node->partitionId))
	{
		debug_lr(MAP_DEBUG, "Error parsing cluster or partition\n");
		return 0;
	}
	return 1;
}


int parse_pe_str(char *pe_str, pe_t *pe)
{
	node_id_t node;
       	char *ptr;
	char *endptr;
	char *cluster_ptr, *node_ptr;
	char buff[81];
	
	
	strncpy(buff, pe_str, 80);
	buff[80] = '\0';
	
	// No seperator was found we treat the pe as a single number
	// and return it as the pe
	if(!(ptr = strchr(buff, CLUSTER_PART_SEP_CHR)))
	{
		long int tmp_pe;

		tmp_pe = strtol(pe_str, &endptr, 10);
		if(*endptr != '\0')
			return 0;
		*pe = tmp_pe;
		return 1;
	}
	// Getting the two number string (cluster, node) 
	node_ptr = ptr + 1;
	*ptr = '\0';
	cluster_ptr = buff;
	
	if(!get_cluster_id(cluster_ptr, &node.clusterId) ||
	   !get_node_id(node_ptr, &node.partitionId))
	{
		debug_lr(MAP_DEBUG, "Error parsing cluster or node\n");
		return 0;
	}
	// Combining the cluster + node toghether 
	*pe = PE_SET(node.clusterId, node.partitionId);
	return 1;
}
