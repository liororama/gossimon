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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <glib.h>

#include <LoadConstraints.h>

typedef struct __timeLC {
	struct timeval startTime;
	struct timeval durationTime;
} timeLC_t;

typedef struct __controlledLCInfo {
	int   controllerPid;
	
} controlledLCInfo_t;


typedef struct __nodeInfo {
	
	nodeLCInfo_t     totals;
	//mliItemList_t        itemsList; // Each node will have a list of load items
	GList            *lcList;
	
	
} internalLCNodeInfo_t;

typedef enum {
	LC_TIME,
	LC_PROCESS,
} lcTypes_t;

typedef struct __lcItem {
	nodeLCInfo_t lcInfo;
	lcTypes_t    type;
	union {
		timeLC_t           timeLC;
		controlledLCInfo_t controlledLC;
	};
} lcItem_t;


struct loadConstraint_struct {

	int              nodeNum;
	struct timeval   currTime;   // used to hold current time
	GHashTable      *nodesHash;  // Hash table to contain all nodes
};


/**************************** Functions ******************************/
void internalNodeInfoInit(internalLCNodeInfo_t *ni) {
	ni->totals.minMemory = 0;
	ni->totals.minProcesses = 0;

	ni->lcList = NULL;
}

void internalNodeInfoDestroy(internalLCNodeInfo_t *ni) {

	// Should go over the list and free all elements
	g_list_free (ni->lcList);
}

int internalNodeInfoGetLC(internalLCNodeInfo_t *internalNI, nodeLCInfo_t *ni)
{
	ni->minMemory = 0;
	ni->minProcesses = 0;

	if(!internalNI->lcList)
		return 1;
	
	// Iterating over the lc list and taking all the  constraints
	GList *curr = internalNI->lcList;
	lcItem_t  *lcItem;
	do {
		lcItem = (lcItem_t *) curr->data;
		ni->minMemory    += lcItem->lcInfo.minMemory;
		ni->minProcesses += lcItem->lcInfo.minProcesses;
	} while((curr = g_list_next(curr)));
	
	return 1;
}

int verifyTimeItem(lcItem_t *lcItem, struct timeval *currTime)
{
	struct timeval time;
	struct timeval expireTime;
	if(!lcItem)
		return 0;
	if(currTime)
		time = *currTime;
	else
		gettimeofday(&time, NULL);
	
	expireTime = lcItem->timeLC.startTime;
	expireTime.tv_sec += lcItem->timeLC.durationTime.tv_sec;
	
	//printf("Before time cmp\n");
	if(timercmp(&time, &expireTime, >)) {
		//printf("Time item verification failed\n");
		return 0;
	}
	return 1;
	
}

int verifyControlledItem(lcItem_t *lcItem)
{
	int         pid = lcItem->controlledLC.controllerPid;
	struct stat statBuf;
	char        procPath[256];
	
	sprintf(procPath, "/proc/%d", pid);
	if(stat(procPath, &statBuf) == -1 && errno == ENOENT)
		return 0;
	return 1;
}

int internalNodeInfoVerify(internalLCNodeInfo_t *ini, loadConstraint_t lc)
{
	if(!ini || !lc)
		return 0;
	if(!ini->lcList)
		return 1;

	// Iterating over all list and removing obsolete entries
	GList      *list = ini->lcList;
	int        delete = 0;
	while(list) {
		lcItem_t  *lcItem = (lcItem_t *) list->data;
		switch(lcItem->type) {
		    case LC_TIME:
			    if(!verifyTimeItem(lcItem, &lc->currTime))
				    delete = 1;
			    break;
		    case LC_PROCESS:
			    if(!verifyControlledItem(lcItem))
				    delete = 1;
			    break;
		}

		if(delete) {
			int fixFirst = 0;
			if(list == ini->lcList)
				fixFirst = 1;
			list = g_list_remove(list, list->data);
			if(fixFirst) {
				ini->lcList = list;
				fixFirst = 0;
			}
			delete = 0;
		} else {
			list = g_list_next(list);
		}
	}
	return 1;
}

void valueDestroyFunc(gpointer data) {
	return internalNodeInfoDestroy((internalLCNodeInfo_t *) data);
}

/*
   Getting a new initialized lc object
*/

loadConstraint_t  lcNew() {
	loadConstraint_t  lc;

	lc = malloc(sizeof(struct loadConstraint_struct));
	if(!lc)
		return NULL;

	lc->nodeNum = 0;
	lc->nodesHash = g_hash_table_new_full(g_str_hash, g_str_equal,
					      free, valueDestroyFunc);
	if(!lc->nodesHash) 
		goto failed;
	
	return lc;
 failed:
	lcDestroy(lc);
	return NULL;

}

void lcDestroy(loadConstraint_t lc)
{

	if(!lc)
		return;

	lc->nodeNum = 0;
	// Cleaning the hash
	if(lc->nodesHash) {
		g_hash_table_destroy(lc->nodesHash);
		lc->nodesHash = NULL;
	}
	
}

int lcHostNum(loadConstraint_t lc) {
	if(!lc)
		return 0;
	return  g_hash_table_size(lc->nodesHash);
	
}

void  printLCItem(gpointer data, gpointer user_data) {
	lcItem_t *lcItem = (lcItem_t *)data;
	
	switch(lcItem->type) {
	    case LC_TIME:
		    printf("\tTLC: dur %ld ", lcItem->timeLC.durationTime.tv_sec);
		    break;
	    case LC_PROCESS:
		    printf("\tCLC: pid %d", lcItem->controlledLC.controllerPid);
		    break;
	    default:
		    printf("\tError detected unsupported type\n");
		    
	}
	printf(" load %d mem %d\n",
	       lcItem->lcInfo.minProcesses,  lcItem->lcInfo.minMemory);
}

void  printHostEntry(gpointer key, gpointer value, gpointer user_data)
{
	char *name = (char*) key;
	internalLCNodeInfo_t *ini = (internalLCNodeInfo_t *)value;

	printf("Host %s:\n", name);
	g_list_foreach(ini->lcList, printLCItem, NULL);
}


void lcPrintf(loadConstraint_t lc) {
	if(!lc) {
		printf("LC is NULL\n");
		return;
	}
		
	printf("Hosts in lc Hash:\n");
	g_hash_table_foreach(lc->nodesHash, printHostEntry, NULL);
	
}

int lcAddItem(loadConstraint_t lc, char *nodeName, lcItem_t *lcItem)
{
	int     res;
	char   *origKey;
	internalLCNodeInfo_t *value;
		
	if(!lc || !nodeName || !lcItem)
		return 0;
	
	// If the node does not appear we add it
	res = g_hash_table_lookup_extended(lc->nodesHash,
					   nodeName,
					   (gpointer *)&origKey,
					   (gpointer *)&value);
	if(!res) {
		char *tmpName;
		//printf("Host %s not found in hash\n", nodeInfo->name);

		value = malloc(sizeof(internalLCNodeInfo_t));
		internalNodeInfoInit(value);
		tmpName = strdup(nodeName);
		g_hash_table_insert(lc->nodesHash, tmpName, value);
		
	}
	value->lcList = g_list_append(value->lcList, lcItem);
	return 1;
}

int lcAddTimeConstraint(loadConstraint_t lc, char *nodeName,
			struct timeval *start, struct timeval *duration,
			nodeLCInfo_t *lcInfo)
{
		
	if(!lc || !nodeName || !duration || !lcInfo)
		return 0;
	
	// Add the constraint to the node constraint list
	//value->totals.minProcesses += nodeInfo->minProcesses;
	lcItem_t *lcItem = malloc(sizeof(lcItem_t));
	lcItem->lcInfo = *lcInfo;
	lcItem->type = LC_TIME;
	if(!start)
		gettimeofday(&(lcItem->timeLC.startTime), NULL);
	else
		lcItem->timeLC.startTime = *start;
	lcItem->timeLC.durationTime = *duration;
	return lcAddItem(lc, nodeName, lcItem);
}

int lcAddControlledConstraint(loadConstraint_t lc, char *nodeName, 
			      int pid, nodeLCInfo_t *lcInfo)
{
	lcItem_t *lcItem = malloc(sizeof(lcItem_t));
	lcItem->lcInfo = *lcInfo;
	lcItem->type = LC_PROCESS;
	lcItem->controlledLC.controllerPid = pid;
	return lcAddItem(lc, nodeName, lcItem);
}

/**
   Get the aggregated load constraint of a single node. The name of the node
   is taken from the nodeInfo parameter
**/
int lcGetNodeLC(loadConstraint_t lc, char *nodeName, nodeLCInfo_t *nodeInfo)
{
	int     res;
	char   *origKey;
	internalLCNodeInfo_t *value;
	
	if(!lc || !nodeInfo)
		return 0;

	// If the node does not appear we add it
	res = g_hash_table_lookup_extended(lc->nodesHash,
					   nodeName,
					   (gpointer *)&origKey,
					   (gpointer *)&value);
	if(!res)
		return 0;
	
	// Aggregating all the constraint of a single node
	return internalNodeInfoGetLC(value, nodeInfo);
}



/*
  Verifing a node, and if after verification and removal of obeleate entries
  the node is empty return true.
 */
gboolean canRemoveNode(gpointer key, gpointer value, gpointer user_data)
{
	//char                  *nodeName = (char *) key;
	internalLCNodeInfo_t  *internalNode = (internalLCNodeInfo_t *) value;
	loadConstraint_t       lc = (loadConstraint_t)user_data;

	//printf("Verifing node %s\n", nodeName);
	internalNodeInfoVerify(internalNode, lc);
	if(!internalNode->lcList)
		return 1;
	return 0;
}

	

/*
  Going over all the lcItems and verifing them.
  
*/
int lcVerify(loadConstraint_t lc) {
	if(!lc)
		return 0;


	//printf("Starting verify\n");
	gettimeofday(&lc->currTime, NULL);
	g_hash_table_foreach_remove(lc->nodesHash, canRemoveNode, lc);
	return 1;
}

#define MAX_LC_BINARY_NODE_NAME  (50)
#define LC_BINARY_MAGIC          (0xdadfaf)

struct lcBinaryNodeInfo {
	char         nodeName[MAX_LC_BINARY_NODE_NAME];
	int          lcItemNum;
	lcItem_t     lcItems[0];
};


struct lcBinaryFormat {
	int          magic;  // Just for safty
	int          nodeNum;
	
	char         data[0]; // The data itself
};


loadConstraint_t buildLCFromBinary(char *buff, int size) {

	loadConstraint_t  lc;
	int               res;
	int               i, j;
	char             *ptr;
	struct lcBinaryFormat    *header;
	struct lcBinaryNodeInfo  *binNodeInf;

	
	lc = lcNew();
	if(!lc) return NULL;

	
	header = (struct lcBinaryFormat *)buff;
	if(header->magic != LC_BINARY_MAGIC) {
		lcDestroy(lc);
		return NULL;
	}
	
	ptr = header->data;
	for(i=0 ; i<header->nodeNum ; i++) {
		lcItem_t     *lcItem;

		binNodeInf = (struct lcBinaryNodeInfo *)ptr;

		for(j=0 ; j < binNodeInf->lcItemNum ; j++) {
			lcItem = &(binNodeInf->lcItems[j]);
			res = lcAddItem(lc, binNodeInf->nodeName, lcItem);
			if(!res) {
				lcDestroy(lc);
				return NULL;
			}
		}
		ptr +=  sizeof(struct lcBinaryNodeInfo) +
			binNodeInf->lcItemNum * sizeof(lcItem_t);
	}
	return lc;
}

struct hashIterWriteData {
	char            *writeBuff; // Auxilary buffer for writing
	int              writeBuffSize;
	
	char            *currPtr;  // Current place
	int              currSize;

	int              error;
};

void hashIterWriteBinaryLC(gpointer key, gpointer value, gpointer user_data)
{
	char                     *name = (char*) key;
	internalLCNodeInfo_t     *ini = (internalLCNodeInfo_t *)value;
	struct hashIterWriteData *auxData = (struct hashIterWriteData *) user_data;

	struct lcBinaryNodeInfo  binNodeInf;

	if(auxData->error)
		return;
	
	strncpy(binNodeInf.nodeName, name, MAX_LC_BINARY_NODE_NAME-1);
	binNodeInf.nodeName[MAX_LC_BINARY_NODE_NAME-1] = '\0';
	binNodeInf.lcItemNum = 0;

	if(!ini->lcList) {
		if((auxData->currSize + sizeof(struct lcBinaryNodeInfo)) >
		   auxData->writeBuffSize) {
			auxData->error = 1;
			return;
		}
		memcpy(auxData->currPtr, &binNodeInf, sizeof(struct lcBinaryNodeInfo));
		auxData->currPtr  += sizeof(struct lcBinaryNodeInfo);
		auxData->currSize += sizeof(struct lcBinaryNodeInfo);
		
	} else {
		GList  *list = ini->lcList;
		char   *tmpPtr = auxData->currPtr;
		int     tmpSize = auxData->currSize;

		if((tmpSize + sizeof(struct lcBinaryNodeInfo)) >
		   auxData->writeBuffSize) {
			auxData->error = 1;
			return;
		}
		tmpPtr  += sizeof(struct lcBinaryNodeInfo);
		tmpSize += sizeof(struct lcBinaryNodeInfo);
		
		do {
			lcItem_t *lcItem = (lcItem_t *)list->data;

			// Adding each lc item
			binNodeInf.lcItemNum++;
			if((tmpSize + sizeof(lcItem_t)) >
			   auxData->writeBuffSize) {
				auxData->error = 1;
				return;
			}
			memcpy(tmpPtr, lcItem, sizeof(lcItem_t));
			tmpPtr  += sizeof(lcItem_t);
			tmpSize += sizeof(lcItem_t);
			
		} while( (list = g_list_next(list)));

		// Adding the node entry
		memcpy(auxData->currPtr, &binNodeInf, sizeof(struct lcBinaryNodeInfo));
		auxData->currPtr  = tmpPtr;
		auxData->currSize = tmpSize;
	}

	return;
}

int writeLCAsBinary(loadConstraint_t lc, char *buff, int size)
{
	struct hashIterWriteData auxData;
	struct lcBinaryFormat    header;
	if(!lc)
		return 0;


	// First calculat the ammount of memory needed for this
	header.magic = LC_BINARY_MAGIC;
	header.nodeNum = g_hash_table_size(lc->nodesHash);

	if(size < sizeof(struct lcBinaryFormat))
		return 0;
	memcpy(buff, &header, sizeof(struct lcBinaryFormat));
		
	
	// Now we write it
	auxData.writeBuff = buff;
	auxData.writeBuffSize = size;
	auxData.currPtr = buff + sizeof(struct lcBinaryFormat);
	auxData.currSize = sizeof(struct lcBinaryFormat);
	auxData.error = 0;
	g_hash_table_foreach(lc->nodesHash, hashIterWriteBinaryLC, &auxData);
	return auxData.error == 0 ? auxData.currSize : 0;
}
