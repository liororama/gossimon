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

/**@defgroup distance_graph The distance_graph module

*  The distance graph object is responsible for managing the DIRECTED distance
* matrix of cluster:partition. Its main usage is to find the current distance
* between two partitions (in the same cluster or not).

 Graph model:
 The map file create a 3 distance level Tree. Where distances are
 0 between nodes of the same partitions
 2 between nodes of different partitions but same cluster
 4 between nodes of different clusters

  

                     <G>
                 1  /   \  1
                   /     \
                  /       \
		<C1>	  <C2>
	       1 /\ 1	 1 /\ 1
                /  \      /  \
	      <P1> <P2> <P1> <P2> 
            0 / \ 0
	     /   \
	   <n1> <n2>
 The friend keyword can be used to set the distances between nodes to
 something else then the default distance.
 The friend is working in a certain direction if the two directions are needed
 then the two directions must be specified.
 
 friend Ci:Pl  => Cj:Pk   ->Distance is 0 between Ci:Pl and Cj:Pk, all the
                            nodes of Pk think the nodes of Pl are in the same
			    partition.
 
 friend Ci:*   => Cj:Pk   ->Distance is 0 between all the partitions of Ci to
                            the Cj:Pk
 
 friend Ci     => Cj      ->Distance is 0 between the clusters which means
                            that distance is 2 between any two partitions in
			    the clusters
 friend Ci:Pl U=> Cj:Pk   ->Distance is 2 between Ci:Pl and Cj:Pk, so Cj:Pk
                            thinks it is in the same cluster as Ci:Pl			      

 friend Ci    U=> Cj:Pk   ->Distance is 1 between Ci and Cj:Pk. This means
                            that Cj:Pk think all the partitions of Ci are
			    in the same cluster as Pk
 friend Ci:Pl U=> Cj      ->Distance is 1 between Ci:Pl and Cj (The
                            partitions of Cj think Ci:Pl is in the same
			    cluster
			      
 friend Ci  inf=>Cj       ->Distance between cluster is infinity. Even though
                            the regular distance is 4 between clusters node now
			    it is set up to infinity

*  @{
*/


#ifndef __DISTANCE_GRAPH
#define __DISTANCE_GRAPH

#include <msx_common.h>
#include <pe.h>

#define DEFAULT_RULE_PREFIX  "friend"

typedef struct dist_graph *dist_graph_t;

dist_graph_t dist_graph_init(char *rule_prefix);
void dist_graph_done(dist_graph_t graph);
const char *dist_graph_get_prefix(dist_graph_t graph);
char *dist_graph_sprintf(char *buff, dist_graph_t graph);

int dist_graph_add_edge_from_str(dist_graph_t graph, char *edge_str);
void dist_graph_optimize(dist_graph_t graph);


/// Infinity distance for the inf=> rule
#define INFINITY_DIST  (100000)
#define INVALID_DIST   (INFINITY_DIST + 1)
#define MAX_KNOWN_DIST (INFINITY_DIST - 1)


// Default distances in the graph model
#define SAME_PARTITION_DIST       (0)
#define SAME_PARTITION_MAX_DIST   (0)
#define SAME_CLUSTER_DIST         (2)
#define SAME_CLUSTER_MAX_DIST     (2)
#define DIFFERENT_CLUSTER_DIST    (4)
#define SAME_GRID_MAX_DIST        (4)

int dist_graph_get_dist(dist_graph_t graph, node_id_t *from, node_id_t *to);

int dist_graph_inc_dist(int dist);
int dist_graph_dec_dist(int dist);

#endif /* __DISTANCE_GRAPH */
/** @} */ // end of distance_graph module
