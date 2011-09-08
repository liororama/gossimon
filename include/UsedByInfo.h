/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __PARSE_EXTERNAL_INFO
#define __PARSE_EXTERNAL_INFO

// Clusters and users which use the current machine

#include <Mapper.h>
#define   MAX_USING_UIDS_PER_CLUSTER  (5)
#define   MAX_USING_CLUSTERS          (5)

// Local usage will be listed under this cluster
#define   LOCAL_CLUSTER_NAME    "local-use"

// Processes from machines which does not belong to any partner
// will be listed under the unknown cluster
#define   UNKNOWN_CLUSTER_NAME  "unknown"


// Non mosix usage will be counted to the user nobody
#define   NON_MOSIX_PROC_USER   "nobody"


typedef struct _used_by {
        char          clusterName[MAPPER_MAX_CLUSTER_NAME];

        int           uidNum;
        int           uid[MAX_USING_UIDS_PER_CLUSTER];
        
} used_by_entry_t;


typedef enum {
        UB_STAT_INIT = 0,
        UB_STAT_LOCAL_USE,
        UB_STAT_GRID_USE,
        UB_STAT_FREE,
        UB_STAT_ERROR,
} node_usage_status_t;

// Return the usage information from the given string.
// arr should have size places in it.
// Retrun value:  status of usage 
//                1 on success
//                  size contain the number of valid entries in arr
node_usage_status_t getUsageInfo(char *str, used_by_entry_t *arr, int *size);
int writeUsageInfo(node_usage_status_t stat, used_by_entry_t *arr,
                   int arrSize, char *buff);

char *usageStatusStr(node_usage_status_t st);
node_usage_status_t strToUsageStatus(char *str);

#endif
