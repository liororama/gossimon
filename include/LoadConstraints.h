/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


/**
   Define a simple text base file which gives information about the minimum
   load a node should have.
   This is usfull for assigning processes to nodes, where we wish the
   assignment action to be noticed at once. Since it takes time for the
   assignment to compleate and to the remote process to start working hard,
   we would like to know that a node got a process

   hosts: 2
   host: mos1 : lines: 2
     timed: start DD:HH:MM:SS period: 30 processes: 1 memory: 50
     process-responsible: pid 44323 processes: 3 memory 100
     
   host: mos2 : lines: 1
     timed: start DD:HH:MM:SS period: 30 processes: 1 memory: 50

**/

#ifndef __MIN_LOAD_INFO
#define __MIN_LOAD_INFO

#include <sys/time.h>
#include <sys/types.h>

typedef struct loadConstraint_struct  *loadConstraint_t;

typedef struct __nodeLCInfo {
	int    minMemory;
	int    minProcesses;
} nodeLCInfo_t;


loadConstraint_t  lcNew();
void lcDestroy(loadConstraint_t lc);

void lcPrintf(loadConstraint_t lc);
/**
   Reading/Writing the min load info file
 **/

/* loadConstraint_t  lcRead(char *fname); */
/* int lcWrite(loadConstraint_t lc, char *fname); */
int lcHostNum(loadConstraint_t lc); 

/**
   Fix the lc object so that it is valid, no expierd timed lc and
   that all processes exists. This should be called by the builder
   at the end to verify 
 **/
int lcVerify(loadConstraint_t lc);

/**
 Adding minimum load constraints to a node
**/
int lcAddTimeConstraint(loadConstraint_t lc, char *nodeName,
			struct timeval *start, struct timeval *duration,
			nodeLCInfo_t *lcInfo);

int lcAddControlledConstraint(loadConstraint_t lc, char *nodeName, 
			      int pid, nodeLCInfo_t *lcInfo);

int lcGetNodeLC(loadConstraint_t lc, char *nodeName, nodeLCInfo_t *nodeInfo);

/* /\** */
/*    Used, for iterating over all existing nodes and getting their minimum load info */
/* **\/ */
/* void lcRewind(loadConstraint_t lc); */
/* int  lcNext(loadConstraint_t lc, nodeLCInfo_t *nodeInfo); */


loadConstraint_t buildLCFromBinary(char *buff, int size);
int writeLCAsBinary(loadConstraint_t lc, char *buff, int size);

#endif
