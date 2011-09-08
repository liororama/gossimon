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

/******************************************************************************
 *    File: gossip_algo.c
 *
 * Contain different gossip algorithms, that infod implement.
 * Each algorithm is made from 2 functions:
 * 1) an init function which might be different for each algorithm
 * 2) step function, which perform the gossip step
 *****************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <msx_debug.h>
#include <msx_error.h>
#include <infoVec.h>
#include <infod.h>


struct infodGossipAlgo  *getGossipAlgoByName(char *name);
struct infodGossipAlgo  *getGossipAlgoByIndex(int index);


int gossipAlgoRegular_init(void **gossipData) {
	*gossipData = NULL;
	return 1;
}

int gossipAlgoRegular_step(ivec_t vec, void *gossip_data,
			   struct gossipAction *ga)
{
	static int only_alive = 0;
	static int full_flg   = 1;

	if( infoVecRandomNode(vec, only_alive, &(ga->randIP)) == 0) 
		return 0;

	/* prepare the message */
	ga->msgData = infoVecGetWindow( vec, &(ga->msgLen), full_flg);
	ga->msgType = INFOD_MSG_TYPE_INFO;
	ga->keepConn = 0;
	return 1;
}


int gossipAlgoMinDead_init(void **gossipData) {
	*gossipData = NULL;
	return 1;
}


int prepRandomNodePush(ivec_t vec, struct gossipAction *ga, int onlyAlive)
{
	if( infoVecRandomNode( vec, onlyAlive, &(ga->randIP)) == 0) 
		return 0;

	/* prepare the message */
	ga->msgData = infoVecGetWindow( vec, &(ga->msgLen), 1);
	ga->msgType = INFOD_MSG_TYPE_INFO;
	ga->keepConn = 0;
	return 1;
}

int gossipAlgoMinDead_step(ivec_t vec, void *gossip_data,
			   struct gossipAction *ga)
{
	int num_alive = 0;
	int param = param = INFOD_SEND_TO_ANY_CYCLE;
	//static int full_flg   = 1;
	static int only_alive = 1;

	/*
	 * The following code is used to reduce the amount of messages
	 * to inactive nodes. Use the number of active nodes known to the
	 * local node, so that on average only one node of the active nodes
	 * will send a message to a general node (active or inactive).
	 */
	if( ( num_alive = infoVecNumAlive( vec )) > param )
		param = num_alive;
	
	if( ( rand() % param ) == 0 )
		only_alive = 0;
	else
		only_alive = 1;
	
	return prepRandomNodePush(vec, ga, only_alive);
}

int gossipAlgoPushPull_init(void **gossipData) {
	*gossipData = NULL;
	return 1;
}


info_pull_msg_t pullBuff;

int prepOldestNodePull(ivec_t vec, struct gossipAction *ga)
{
	if( infoVecOldestNode( vec, &(ga->randIP)) == 0) 
		return 0;

	/* currently using a dummy message buffer */
	pullBuff.param = 99;
	
	/* prepare the message */
	ga->msgData  = &pullBuff;
	ga->msgLen   = sizeof(info_pull_msg_t);
	ga->msgType  = INFOD_MSG_TYPE_INFO_PULL;
	ga->keepConn = 1;
	return 1;
}	

int gossipAlgoPushPull_step(ivec_t vec, void *gossip_data,
			    struct gossipAction *ga)
{
	int res = 0, num_alive = 0;
	int param = INFOD_SEND_TO_ANY_CYCLE;
	//static int full_flg   = 1;
	static int only_alive = 1;
	static int pull_cycle = 0;
	
	/*
	 * The following code is used to reduce the amount of messages
	 * to inactive nodes. Use the number of active nodes known to the
	 * local node, so that on average only one node of the active nodes
	 * will send a message to a general node (active or inactive).
	 */
	if( ( num_alive = infoVecNumAlive( vec )) > param )
		param = num_alive;
	
	if( ( rand() % param ) == 0 )
		only_alive = 0;
	else
		only_alive = 1;

	// In the only alive sycles we perform the push-pull, in the other cycles
	// only the push
	if(only_alive)
	{
		if(pull_cycle) {
			res = prepOldestNodePull(vec, ga);
			debug_lb( INFOD_DEBUG, "~~~~ PULL from oldest --> %s\n",
				  inet_ntoa(ga->randIP));
		}
		else {
			res = prepRandomNodePush(vec, ga, only_alive);
			debug_lb( INFOD_DEBUG, "~~~~ PUSH to alive --> %s\n",
				  inet_ntoa(ga->randIP));
		}
		pull_cycle++;
		pull_cycle = pull_cycle % 2;
	} else
	{
		res = prepRandomNodePush(vec, ga, only_alive);
		debug_lb( INFOD_DEBUG, "~~~~ PUSH to any ---> %s\n",
			  inet_ntoa(ga->randIP));
	}
	return res;
}


// All the possible gossip algorithms in use
struct infodGossipAlgo infodGossipAlgo[] =
{
	{ "reg",
	  gossipAlgoRegular_init,
	  gossipAlgoRegular_step
	},
	{ "mindead",
	  gossipAlgoMinDead_init,
	  gossipAlgoMinDead_step
	},
	{ "pushpull",
	  gossipAlgoPushPull_init,
	  gossipAlgoPushPull_step,
	},
	{ NULL, NULL, NULL },
};
	  
struct infodGossipAlgo  *getGossipAlgoByName(char *name)
{
	int    i = 0;

	// Iterating over the known gossip algos
	while(infodGossipAlgo[i].algoName != NULL) {
		if(strcmp(name, infodGossipAlgo[i].algoName) == 0)
		{
			return &infodGossipAlgo[i];
		}
		i++;
	}
	return NULL;
}
struct infodGossipAlgo  *getGossipAlgoByIndex(int index)
{
	int    i = 0;

	// Iterating over the known gossip algos
	while(infodGossipAlgo[i].algoName != NULL) {
		if(i<index) i++;
		else {
			return &infodGossipAlgo[i];
		}
	}
	return NULL;
}
