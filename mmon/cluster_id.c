/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/



#include <stdio.h>
#include <string.h>
#include <info.h>
#include <mmon.h>

/*
 The purpose of this function is to find how many clusters exists in the
 info array (according to the cluster_id string. And produce a different
 cluster id which is shorter and is easier to understand

 The way this is done is by taking the first node clusterId and calling it
 as A, the second as B, the third as C ....
*/

char clusterIdArr[31][32];
int  maxId = 28;
int  idNum = 0;

int nullIdIndex = 29;

static
int getClusterIdIndex(char *cId) {
  
  if(!cId)
    return nullIdIndex;
  for(int i=0 ; i<idNum ; i++) {
    if(strcmp(cId, clusterIdArr[i]) == 0)
      return i;
        }
        return -1;
}

int calcClusterId(all_info_items_t *infoArr, int size) {

        sprintf(clusterIdArr[nullIdIndex], "0");
        idNum = 0;
        for(int i=0 ; i<size ; i++) {
                // Skeeping dead nodes
                if( !(infoArr[i].infod_status & INFOD_ALIVE))
                        continue;
                if( !infoArr[i].cluster_id)
                        continue;
                int index;
                if((index = getClusterIdIndex(infoArr[i].cluster_id)) >= 0)
                        sprintf(infoArr[i].cluster_id, "%c", 'A' + index);
                
                else {
                        //Adding the cluster ID to the array
                        strcpy(clusterIdArr[idNum], infoArr[i].cluster_id);
                        sprintf(infoArr[i].cluster_id, "%c", 'A' + idNum);
                        if(idNum < maxId)
                                idNum++;
                }
        }
        return 1;
}
