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



/**  @addtogroup distance_graph 
 *
 *

   
 *  @{
 */



#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <pe.h>
#include <parse_helper.h>
#include <msx_error.h>
#include <msx_debug.h>
#include <distance_graph.h>

#define MAX_EDGES  (100)

typedef enum
{
	OP_ZERO_DISTANCE,
	OP_INF_DISTANCE, 
	OP_CLUSTER_UNION,
	OP_NOOP,
} edge_operator_t;

struct edge_type_operator
{
	char              opStr[10]; // Should be enough
	edge_operator_t   opType;    // Operator type
};

struct edge_type_operator valid_op[] =
{ {"=>",    OP_ZERO_DISTANCE},
  {"U=>",   OP_CLUSTER_UNION},
  {"inf=>", OP_INF_DISTANCE},
  {"end",   OP_NOOP},
};

/*typedef enum
{
	PART_TO_PART,
	CLUSTER_TO_CLUSTER,
	PART_TO_CLUSTER,
	CLUSTER_TO_PART,
	ALL_CLUSTER_PARTS_TO_PART,
} special_edge_type_t;
*/
typedef struct special_edge
{
	node_id_t           se_from;
	node_id_t           se_to;

	edge_operator_t     se_type;    // Operator type se_type;
	int                 se_length;
} special_edge_t;

struct dist_graph
{
	int              maxEdges;
	int              nrEdges;
	char            *rulePrefix;
	char            *rulePrefixCopy;
	special_edge_t  *specialEdgeArr;
};

// A global graph object which currently points to the graph in use
// It is the responsibility of the interface function to set this.
dist_graph_t glob_graph; 

char glob_buff[1024];

// Static functions declaration
static int parse_edge_rule(char *rule, edge_operator_t *op, char *from, char *to);
static int validate_edge_rule(edge_operator_t op, node_id_t *from, node_id_t *to);
static int validate_edge_addition(special_edge_t *new);
static int get_default_dist(node_id_t *from, node_id_t *to);
static int get_edge_distance(special_edge_t *e);
static int update_distance(int dist, int new_dist);
static int get_default_dist(node_id_t *from, node_id_t *to);
static int distance_is_final(int dist);
static const char *get_edge_type_str(edge_operator_t type);
static void special_edge_sprintf(char *buff, special_edge_t *e);


/** Creating a new distance graph object
 * @param rule_prefix   The prefix string to the rule line
 * @return A new distance graph object
 */
dist_graph_t dist_graph_init(char *rule_prefix)
{
	dist_graph_t dg;
	
	if(!(dg = malloc(sizeof(struct dist_graph))))
		return NULL;
	
	if(!(dg->specialEdgeArr = calloc(MAX_EDGES, sizeof(special_edge_t))))
	{
		free(dg);
		return NULL;
	}
	
	dg->maxEdges = MAX_EDGES;
	dg->nrEdges = 0;

	// Setting the prefix of the edge rule
	if(!rule_prefix)
		rule_prefix = DEFAULT_RULE_PREFIX;
	dg->rulePrefix = strdup(rule_prefix);
	dg->rulePrefixCopy = strdup(rule_prefix);
	return dg;
}
void dist_graph_done(dist_graph_t graph)
{
	if(!graph)
		return;

	free(graph->specialEdgeArr);
	free(graph);
}

/** Returning the prefix of an edge rule so other object can identify
 * apropriate lines and send them to the graph object 
 * @return A pointer to a statically allocated buffer which contain a
 *         copy of the prefix. Modifying this buffer will not effect
 *         the value of the next call to the function
*/

const char *dist_graph_get_prefix(dist_graph_t graph)
{
	strcpy(graph->rulePrefixCopy, graph->rulePrefix);
	return graph->rulePrefixCopy;
}

/** Printing the graph special edges into a buffer. The output is a valid input
 * to dist_graph_add_edge_from_str (if fed line by line);
 * @param  buff    Pointer to a buffer to use
 * @param  graph   Dist graph object to print
 * @return A pointer to the buffer (given as the argument)
 */
char *dist_graph_sprintf(char *buff, dist_graph_t graph)
{
	int   i;
	char *ptr = buff;

	glob_graph = graph;
	
	if(!buff)
		return NULL;
	if(!graph) {
		sprintf(buff, "Dist Graph is NULL\n");
		return buff;
	}
	
	for(i=0 ; i<graph->nrEdges ; i++) {
		special_edge_sprintf(ptr, &graph->specialEdgeArr[i]);
		ptr+= strlen(ptr);
		sprintf(ptr, "\n");
		ptr+= strlen(ptr);
	}
	return buff;
}

/** Adding a special edge to the graph from the given string
 * All the strings should be of the form"
 * RULE_PREFIX  from_node  edge_type  to_node
 * for example:
 * friend  c1:p1 => c2:p2
 * @param graph     The graph object
 * @param edge_str  A string to parse which contain the new edge definition
 */
int dist_graph_add_edge_from_str(dist_graph_t graph, char *edge_str)
{
	char *ptr;
	node_id_t from, to;
	edge_operator_t   op_type;
	char from_str[80], to_str[80];
	special_edge_t new_edge;
	
	if(!graph) return 0;

	glob_graph = graph;
	// First making sure that the prefix is there
	if(strncasecmp(graph->rulePrefix, edge_str, strlen(graph->rulePrefix)) != 0)
	{
		debug_lr(MAP_DEBUG, "Rule does not start with a valid prefix\n");
		return 0;
	}

	// ptr now points just after the rule prefix
	ptr = edge_str + strlen(graph->rulePrefix);

	// Finding the edge_type operator and the two nodes str
	if(!parse_edge_rule(ptr, &op_type, from_str, to_str))
		return 0;
	// Finindg the two nodes and making sure they are in a valid form
	if(!parse_node_str(from_str, &from) || !parse_node_str(to_str, &to))
		return 0;
	// Validating the edge. To see if we can accept it in our current
	// distance model
	if(!validate_edge_rule(op_type, &from, &to))
		return 0;

	// Validating the table with the new edge, making sure no confilcts are
	// created with the addition
	new_edge.se_from = from;
	new_edge.se_to = to;
	new_edge.se_type = op_type;
	
	if(!validate_edge_addition(&new_edge))
		return 0;
	
	// All is OK, adding edge to the special edges array.
	// Extending the array if it is full
	if(graph->nrEdges == graph->maxEdges)
	{
		int new_max;
		special_edge_t *ptr;
		debug_lr(MAP_DEBUG, "No more space for new edges\n");

		new_max = graph->maxEdges;
		ptr = realloc(graph->specialEdgeArr, new_max);
		if(!ptr) {
			debug_lr(MAP_DEBUG, "Sorry, but can not extend edges array\n");
			return 0;
		}
		graph->specialEdgeArr = ptr;
		graph->maxEdges = new_max;
	}

	// Adding the new rule
	graph->specialEdgeArr[graph->nrEdges++] = new_edge;
	return 1;
}
 
static
int parse_edge_rule(char *rule, edge_operator_t *op_type,
		    char *from, char *to)
{
	struct edge_type_operator *opptr;
	char *ptr, *max_len_match_ptr = NULL;
	int pos, max_len_pos, max_len_match = 0;
	char *from_ptr, *to_ptr;

	// Working on temporary buffer
	strcpy(glob_buff, rule);
	
	debug_lg(MAP_DEBUG, "parsing rule: %s\n", rule);
	for(opptr = valid_op, max_len_pos=pos=0; opptr->opType != OP_NOOP ;
	    opptr++, pos++)
	{
		if((ptr = strstr(glob_buff, opptr->opStr)))
		{
			int len = strlen(opptr->opStr);
			// There might be two operators in the same line
			if(len == max_len_match) {
				debug_lr(MAP_DEBUG,
					 "Found two operators in the same length");
				return 0;
			}
			// We take the longest operator found
			if(len > max_len_match) {
				max_len_match_ptr = ptr;
				max_len_match = len;
				max_len_pos = pos;
			}
		}
	}
	// Checking if an operator was found or not
	if(!max_len_match) {
		debug_lr(MAP_DEBUG, "No operator was found in rule");
		return 0;
	}

	*op_type = valid_op[max_len_pos].opType;

	to_ptr = max_len_match_ptr + max_len_match;
	to_ptr = eat_spaces(to_ptr);
	trim_spaces(to_ptr);
	strcpy(to, to_ptr);
	
	*max_len_match_ptr = '\0';
	from_ptr = eat_spaces(glob_buff);
	trim_spaces(from_ptr);
	strcpy(from, from_ptr);

	debug_ly(MAP_DEBUG, "Rule: op: '%d'  from: '%s' to: '%s'\n",
		 *op_type, from, to);
	return 1;
}


/** Validating an edge rule, checking that the rule is according to the distance
 * model we are working with.
 * @param op     The edge operator
 * @param from   The source node
 * @param to     The Destination node
 * @return 0 if edge rule is not valid otherwise 1
 */
static int validate_edge_rule(edge_operator_t op, node_id_t *from,
			      node_id_t *to)
{
	// Not allowing inside cluster edges (or from the cluster to
	// itself
	if(from->clusterId == to->clusterId)
	{
		debug_lr(MAP_DEBUG,
			 "No edges are allowed inside the cluster\n"); 
		return 0;
	}
	// Allowing inf=> rule only between clusters
	if((op == OP_INF_DISTANCE) &&
	   (from->partitionId != ONLY_CLUSTER ||
	    to->partitionId != ONLY_CLUSTER))
	{
		debug_lr(MAP_DEBUG,
			 "Infinity edge is allowed only between cluster\n");
		return 0;
	}

	if(op == OP_CLUSTER_UNION &&
	   cluster_only_node(from) && cluster_only_node(to))
	{
		debug_lr(MAP_DEBUG,
			 "An edge between cluster can be only a direct one\n");
		return 0;
	}
	
	return 1;
}

/** Checking if two special edges are equal. Only the source and dest nodes
 * Are checked
 * @param *e1   Pointer to special edge 1
 * @param *e2   Pointer to special edge 2
 * @return 0 if edges are not equal, 1 if yes.
 */
static int edge_equal(special_edge_t *e1, special_edge_t *e2)
{
	if(node_id_equal(&e1->se_from, &e2->se_from) &&
	   node_id_equal(&e1->se_to, &e2->se_to))
		return 1;
	return 0;
}
/** Validating the addition of a new edge to the graph before the addition takes
 * place.
 @param *new  Pointer to a special_edge_t structure containing the edge info
 @return 0 if the addition creates a problem or 1 if the addition is OK
*/
static int validate_edge_addition(special_edge_t *new)
{
	int i;

	for(i=0 ; i<glob_graph->nrEdges ; i++)
	{
		special_edge_t *e = &glob_graph->specialEdgeArr[i];
		if(edge_equal(e, new)) {
			debug_lr(MAP_DEBUG,
				 "The new edge is equal to edge num %d\n",i);
			return 0;
		}
		if((new->se_type == OP_INF_DISTANCE ||
		   e->se_type == OP_INF_DISTANCE) &&
		   (same_cluster_nodes(&e->se_from, &new->se_from) &&
		    same_cluster_nodes(&e->se_to, &new->se_to)))
		{
			debug_lr(MAP_DEBUG,
				 "Inf edge is not allowd toghether with other edges\n");
			return 0;
		}
		
	}
	return 1;
}

/** Calculating the distance between two nodes in the graph. The nodes are
 * an exact node (c:p)
 * @param graph   Graph object
 * @param from    The source node (must be a (c:p) node)
 * @paraq to      The destination node (must be a (c:p) node)
 * @return The distance between the two nodes as an integer. Distance of -1
 *         reflects an error
 */
int dist_graph_get_dist(dist_graph_t graph, node_id_t *from, node_id_t *to)
{
	int i;
	int d = INVALID_DIST, tmp_d;
	
	// Making sure the nodes are c:p
	if(cluster_only_node(from) || cluster_only_node(to)) {
		debug_lr(MAP_DEBUG, "Both from and to must be (c:p) nodes\n");
		return -1;
	}
	
	if(!graph) return -1;
	glob_graph = graph;

	// Scanning the special edges to find matching edges for the from and to
	for(i=0 ; i<glob_graph->nrEdges ; i++)
	{
		special_edge_t *e = &glob_graph->specialEdgeArr[i];

		// Unrelated rules (a common case should be first)
		if(!same_cluster_nodes(&e->se_from, from)||
		   !same_cluster_nodes(&e->se_to, to))
			continue;
		
		// In the same cluster and there is a specific side which is
		// which is different c1:p2 -> c2:p1  vs c1:p1 -> c2:p3
		if(!cluster_only_node(&e->se_from) &&
		   !node_id_equal(&e->se_from, from))
			continue;

		// c1->c2:p1 vs c1:p1->c2:p3
		if(!cluster_only_node(&e->se_to) &&
		   !node_id_equal(&e->se_to, to))
			continue;
	
		tmp_d = get_edge_distance(e);
		d = update_distance(d, tmp_d);
		if(distance_is_final(d))
			break;
	}
	
	// We found a specific edge regarding this src and dest
	if(d != INVALID_DIST)
		return d;
	// If no match was found we return the default distance
	return get_default_dist(from, to);
}

/** Calculating the edge distance
 * @param *e  Pointer to a special_edge_t struct describing the edge
 * @return The calculated distance
 */
static int get_edge_distance(special_edge_t *e)
{
	node_id_t *from = &e->se_from;
	node_id_t *to = &e->se_to;
	switch(e->se_type) {
	    case OP_ZERO_DISTANCE:
		    if(!cluster_only_node(from) && !cluster_only_node(to))
			    return SAME_PARTITION_DIST;
		    if(cluster_only_node(from) && cluster_only_node(to))
			    return SAME_CLUSTER_DIST;
		    break;
	    case OP_CLUSTER_UNION:
		    return SAME_CLUSTER_DIST;
	    case OP_INF_DISTANCE:
		    return INFINITY_DIST;
	    default:
		    return INVALID_DIST;
	}
	return INVALID_DIST; // Just to prevent a compilation warning
}
/** Updating the distance given a new distance. Actually this is like a min
 * function but it is not a real max
 * @param dist       The original distance
 * @param new_dist   The new distance
 * @return The updated distance (one of the dist,new_dist)
*/
static int update_distance(int dist, int new_dist)
{
	// INFINITY_DIST is always the most important
	if(new_dist == INFINITY_DIST || dist  == SAME_PARTITION_DIST)
		return  INFINITY_DIST;
	if(new_dist < dist)
		return new_dist;
	return dist;
}

/** Checking if the distance is a final one that can not be override by any
 * other edge. For example INF is always final. Same as 0 distance
 * @param dist   The distance to check
 * @return 0 if distance is not final 1 if yes
*/
static int distance_is_final(int dist)
{
	if(dist == INFINITY_DIST || dist == SAME_PARTITION_DIST)
		return 1;
	return 0;
}

/** Calculating the default distance between two nodes
 * @param *from  Pointer to the from node_id_t
 * @param *to    Pointer to the to node_id_T
 * @return The default distance between the two nodes
*/
static int get_default_dist(node_id_t *from, node_id_t *to)
{
	if(node_id_equal(from, to))
		return SAME_PARTITION_DIST;
	if(same_cluster_nodes(from, to))
		return SAME_CLUSTER_DIST;
	return DIFFERENT_CLUSTER_DIST;
}

/** Transforming from edge type to a printable string ,
 * If the type is not a valid one the string returned will be that of the
 * OP_NOOP one
 @param type  The type to convert into a string
 @return A constant pointer to the op string
 */
static const char *get_edge_type_str(edge_operator_t type)
{
	struct edge_type_operator *op_ptr = valid_op;

	for(; op_ptr->opType != OP_NOOP ; op_ptr++)
		if(type == op_ptr->opType)
			break;
	return op_ptr->opStr;
}


static void special_edge_sprintf(char *buff, special_edge_t *e)
{
	char from_str[50];
	char to_str[50];
	
	node_id_to_str(&e->se_from, from_str);
	node_id_to_str(&e->se_to, to_str);
	
	sprintf(buff, "%s %s %s %s", 
		dist_graph_get_prefix(glob_graph), from_str, get_edge_type_str(e->se_type),
		to_str);
}

int dist_graph_inc_dist(int dist) {
	if(dist < SAME_PARTITION_DIST)
		return SAME_PARTITION_DIST;
	if(dist >= MAX_KNOWN_DIST)
		return MAX_KNOWN_DIST;
	return dist + 2;
}

int dist_graph_dec_dist(int dist) {
	if(dist <= SAME_PARTITION_DIST)
		return SAME_PARTITION_DIST;
	if(dist > MAX_KNOWN_DIST)
		return MAX_KNOWN_DIST;
	return dist - 2;
}

/** @} */ // end of distance_graph module
