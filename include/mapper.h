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


/* New mosix map format for supporting grids/partitions

   XML version:
   -------------
   <grid>

      <cluster id="num">
         <part id="num">
	    <start="num"/>
	    <ip="..."/> | <hostname="name"/>
	    <number = "num"/>
	 </part>
	 <part id="num">
	    <start="num"/>
	    <ip="..."/> | <hostname="name"/>
	    <number = "num"/>
	 </part>
      </cluster>	 

      <cluster id="num">
         <part id="num">
	    <start="num"/>
	    <ip="..."/> | <hostname="name"/>
	    <number = "num"/>
	 </part>
	 <part id="num">
	    <start="num"/>
	    <ip="..."/> | <hostname="name"/>
	    <number = "num"/>
	 </part>
      </cluster>	 
	 
      <friends>
         <friend>
	    <from cluster="str"  partition="str"/>
	    <to cluster="str"  partition="str"/>
	 </friend>   
	 <friend>
	    <from cluster="str"  partition="str"/>
	    <to cluster="str"  partition="str"/>
	 </friend>   
      </friend> 	 

    </grid>

   Text version:
   --------------
   # Is a remark
   cluster=id
   part=id : start ip/hostname number
   part=id : start ip/hostname number

   cluster=id
   part=id : start ip/hostname number
   part=id : start ip/hostname number

   # one to one relations. A partition extention or loaning
   friends cluster_id1:part_id1  => cluster_id2:part_id2
   friend cluster_id1:part_id1 <=> cluster_id2:part_id2

   # Many to one relation. Extending a cluster to a partition
   friend cluster_id1           => cluster_id2:part_id2

   # Unification of two clusters into one.
   friend : cluster_id1  U=> cluster_id2

   # Example of zone definition
   # Clusters not in any zone will use the default zone which can be to always
   # compress or never compress

   #
   # default_zone=same
   #
   # default_zone=not_same
   #
   # If not specified then not_same is used;

   # The id value must be > 0
   zone id : ci cj ck cl

   zone id : ca cb cc
   zone id : cf ch cp
   
*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <msx_common.h>
#include <pe.h>

typedef struct mapper_struct *mapper_t;

typedef struct mapper_node_info
{
	pe_t             ni_pe;
	pe_t             ni_partid;
	struct in_addr   ni_addr;
} mapper_node_info_t;

enum MapperBuilderTypeT {
	MB_USERVIEW,
	MB_MOSIX_MAP,
	MB_BINARY_CONFIG,
	MB_SETPE_OUTPUT,
};



mapper_t mapper_init(int max_ranges);

mapper_t mapper_init_from_file(const char *path, pe_t my_pe);
mapper_t mapper_init_from_mem(const char *map_buff, const int len, pe_t my_pe);
mapper_t mapper_init_from_cmd(const char *map_cmd, pe_t my_pe);

int mapper_set_from_file(mapper_t map, const char* path);
int mapper_set_from_mem(mapper_t map, const char* map_buff, const int len);
int mapper_set_from_cmd(mapper_t map, const char *map_cmd);

int mapper_done(mapper_t map);

typedef enum {
	MOSIX_MAP_NOT_VALID,
	MOSIX_MAP_UM,
	MOSIX_MAP_PM,
} mapper_map_type;

mapper_map_type mapper_get_map_type(mapper_t map);

typedef enum 
{
	PRINT_XML,
	PRINT_TXT,
	PRINT_TXT_PRETTY,
	PRINT_TXT_DIST_MAP
} mapper_print_t;

/* Printnting of map */
#include <stdio.h>
void mapper_printf(mapper_t map, mapper_print_t t);
void mapper_fprintf(FILE *f, mapper_t map, mapper_print_t t);
void mapper_sprintf(char *buff, mapper_t map, mapper_print_t t);

/* Getting the erroneous line/s in the last map set or a specific error message */
char *mapper_error(mapper_t map);

/* Signature of map */
void mapper_get_signature(mapper_t *map);

/* Map Iterator */

// The iterator type. 
typedef enum {
	MAP_ITER_ALL,        // Iterate over all nodes in map
	MAP_ITER_CLUSTER,    // Iterate only over local cluster nodes
	MAP_ITER_PARTITION,   // Iterate only over local partition nodes
} mapper_iter_type_t;

typedef struct mapper_iter_struct *mapper_iter_t;

mapper_iter_t mapper_iter_init(mapper_t map, mapper_iter_type_t iter_type);
mapper_iter_t mapper_dist_iter_init(mapper_t map, int dist);
void  mapper_iter_done(mapper_iter_t iter);
int mapper_next(mapper_iter_t iter, mapper_node_info_t *node_info);


/* Queries on the mapper  */
int mapper_set_my_pe(mapper_t map, pe_t my_pe);
pe_t mapper_get_my_pe(mapper_t map);
pe_t mapper_get_my_cluster_id(mapper_t map);
pe_t mapper_get_my_partition_id(mapper_t map);

int mapper_node2host(mapper_t map, mnode_t num, char *hostname);
int mapper_node2addr(mapper_t map, mnode_t num, in_addr_t *ip);
int mapper_node2cluster_id(mapper_t map, mnode_t num, int *cluster_id);
int mapper_node2partition_id(mapper_t map, mnode_t num, int *part_id);

int mapper_hostname2node(mapper_t map, const char* hostname, mnode_t* num);
int mapper_addr2node(mapper_t map, struct in_addr *ip, mnode_t *num);

int mapper_node_at_pos(mapper_t map, int pos, mapper_node_info_t *ninfo);
int mapper_node_at_cluster_pos(mapper_t map, int pos, mapper_node_info_t *ninfo);
int mapper_node_at_partition_pos(mapper_t map, int pos, mapper_node_info_t *ninfo);
int mapper_node_at_dist_pos(mapper_t map, int pos, int dist, mapper_node_info_t *ninfo);

/* Section sizes */
int mapper_get_total_node_num(mapper_t map);
int mapper_get_cluster_node_num(mapper_t map);
int mapper_get_partition_node_num(mapper_t map);
int mapper_get_node_num_at_dist(mapper_t map, int dist);

int mapper_get_ranges_num(mapper_t map);
pe_t mapper_get_max_pe(mapper_t map);

/* Range handling */
int mapper_get_range_str(mapper_t map, char *buff, int size);
int mapper_get_cluster_range_str(mapper_t map, char *buff, int size);
int mapper_get_partition_range_str(mapper_t map, char *buff, int size);

/* Getting the configuration for the MOSIX kernel */
int mapper_get_kernel_map(mapper_t map, struct mosixnet *buff, int size,
			  int min_guest_dist, int max_guest_dist,
			  int max_mig_dist);

/* Getting the distance between the given node to the mapper node */
int mapper_node_dist(mapper_t map, pe_t node_pe);			  

/* Getting the maximum distance between any two nodes */
int mapper_get_max_dist(mapper_t map);


#endif
