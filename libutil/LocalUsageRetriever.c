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
#include <utime.h>
#include <signal.h>
#include <sys/types.h>
#include <pwd.h>

#include <msx_debug.h>
#include <msx_error.h>
#include <msx_proc.h>
#include <msx_common.h>
#include <readproc.h>

#include <Mapper.h>
#include <MapperBuilder.h>
#include <UsedByInfo.h>

#include <LocalUsageRetriever.h>


struct _local_usage_retriever {

        char             verbose;
        char             debug;
        node_usage_status_t status;
        int              myPrio;

        char            *procDir;
        mapper_t         partnersMap;

        // Process list information
        int              procArrMaxSize;
        int              procArrSize;
        proc_entry_t    *procArr;

        // Local using clusters information
        int              usingClusters;
        int              maxUsingClusters;
        used_by_entry_t *usedByInfo;

        char             errMsg[512];
        int              internalTmpBuff;
        char            *tmpBuff;
};

// Function declarations
static int readCurrPrio(lur_t lur, int *prio);
static int findLocalUsedByInfo(lur_t lur);
static int findGridUsedByInfo(lur_t lur);
static uid_t getNonMosixUid(char *name);

lur_t  lur_init(char *procDir, mapper_t partnersMap, int maxproc, char *tmpBuff)
{
        if(!procDir)
                return NULL;

        lur_t lur = malloc(sizeof(struct _local_usage_retriever));
        if(!lur)
                return NULL;
        bzero(lur, sizeof(struct _local_usage_retriever));

        // General
        lur->procDir = strdup(procDir);
        lur->partnersMap = partnersMap;
        if(tmpBuff) {
                lur->tmpBuff = tmpBuff;
                lur->internalTmpBuff = 0;
        }
        else {
                lur->tmpBuff = malloc(4096);
                lur->internalTmpBuff = 1;
        }
        // Process array
        lur->procArrMaxSize = maxproc < 200 ? 200 : maxproc; 
        lur->procArrSize = 0;
        lur->procArr = malloc(sizeof(proc_entry_t) * lur->procArrMaxSize);
        if(!lur)
                goto failed;

        // Usage info is given by the caller
        lur->usingClusters = 0;
        lur->maxUsingClusters = 0;
        lur->usedByInfo = NULL;

        // Usage status 
        lur->status = UB_STAT_INIT;
        
        return lur;

 failed:
        lur_free(lur);
        return NULL;
}


void   lur_free(lur_t lur)
{
        if(!lur)
                return;
        if(lur->procDir) free(lur->procDir);
        if(lur->procArr) free(lur->procArr);
        if(lur->internalTmpBuff && lur->tmpBuff)
                free(lur->tmpBuff);
        if(lur->partnersMap)
                mapperDone(lur->partnersMap);
}

void   lur_setVerbose(lur_t lur, int verbose) {
        if(!lur) return;
        lur->verbose = verbose;
}
void   lur_setDebug(lur_t lur, int debug) {
        if(!lur) return;
        lur->debug = debug;
}

void   lur_setMapper(lur_t lur, mapper_t partnersMap) {

        if(lur->partnersMap)
                mapperDone(lur->partnersMap);
        lur->partnersMap = partnersMap;
}

int    lur_getLocalUsage(lur_t lur, node_usage_status_t *status,
                         used_by_entry_t *usedByInfo, int *size)
{
        int res = 0;
        int prio;
        
        if(!lur || *size <= 0 || !usedByInfo)
                return 0;
        //debug_lg(LUR_DEBUG, "~~~~~~~~~~~~~~~~~ LUR LUR LUR ~~~~~~~~~~~~~~~~~~~~~\n");
        if(!readCurrPrio(lur, &prio)) {
                debug_lr(LUR_DEBUG, "Error reading priority\n");
                return 0;
        }

        // Each time starting from 0 using clusters
        lur->usingClusters = 0;
        lur->maxUsingClusters = *size;
        lur->usedByInfo = usedByInfo;
        
        // If used by owner doing nothing
        if(MSX_IS_LOCAL_PRIO(prio))
        {
                if(lur->verbose)
                        fprintf(stderr, "Used by Local processes\n");
                
                // First time we go from any type of usage to local usage
                lur->status = UB_STAT_LOCAL_USE;
                res = findLocalUsedByInfo(lur);
        }
        else if(MSX_IS_AVAIL_PRIO(prio))
        {
                if(lur->debug)
                        fprintf(stderr, "Currently Free\n");
                lur->status = UB_STAT_FREE;
                res = 1;
        }
        // We have grid processes 
        else if(MSX_IS_GRID_PRIO(prio))
        {
		if(lur->partnersMap) {
			lur->status = UB_STAT_GRID_USE;
			res = findGridUsedByInfo(lur);
		} else {
			res = 0;
		}
        }
        
        if(!res)
                return 0;

        *status = lur->status;
        *size = lur->usingClusters;

        // clearing references to the outside memory once we finish
        lur->usingClusters = 0;
        lur->maxUsingClusters = 0;
        lur->usedByInfo = NULL;
        
        return 1;
}


static 
int readCurrPrio(lur_t lur, int *prio) {
        int   res;
        res =  msx_readval(MOSIX_CURR_PRIO_FILENAME, prio);
        if(!res) {
                if(lur->verbose || lur->debug)
                        fprintf(stderr, "Error reading %s priority file\n",
                                MOSIX_CURR_PRIO_FILENAME);
                debug_lr(LUR_DEBUG, "Error reading %s priority file\n",
                         MOSIX_CURR_PRIO_FILENAME);
        }
        
        return res;
}

static
void updateUsingClusterEntry(used_by_entry_t *uce, proc_entry_t *pe)
{
        int i;

        // Updating the using uids
        for(i= 0 ; i< uce->uidNum ; i++) {
                if(uce->uid[i] == pe->euid)
                        break;
        }
        if(i >= uce->uidNum && uce->uidNum < MAX_USING_UIDS_PER_CLUSTER) {
                uce->uid[i] = pe->euid;
                uce->uidNum++;
        }
}

static 
void initUsingClusterEntry(used_by_entry_t *uce, const char *clusterName, proc_entry_t *pe)
{
        strncpy(uce->clusterName, clusterName, MAPPER_MAX_CLUSTER_NAME-1);
        uce->uidNum = 0;
        updateUsingClusterEntry(uce, pe);
}

static
void printUsingClusterEntry(used_by_entry_t *uce)
{
        int i;
        
        printf("%-15s ", uce->clusterName);
        for(i = 0 ; i< uce->uidNum ; i++) {
                printf("%d ", uce->uid[i]);
        }
        printf("\n");
}
/*************************************************************************
 * Adding a using cluster to the udedByInfo array
 ************************************************************************/
static
int updateUsingCluster(lur_t lur, const char *clusterName, proc_entry_t *e) {
        int i;

        for(i=0 ; i<lur->usingClusters ; i++) {
                if(strcmp(clusterName, lur->usedByInfo[i].clusterName) == 0) {
                        updateUsingClusterEntry(&lur->usedByInfo[i], e);
                        break;
                }
        }

        // Cluster was not found among the currently using cluster
        // The common case will be that this is the first time a cluster is
        // using this node
        if( i == lur->usingClusters && i< lur->maxUsingClusters) {
                initUsingClusterEntry(&lur->usedByInfo[i], clusterName, e);
                lur->usingClusters++;
        }
        return 1;
}
                           
int findGridUsedByInfo(lur_t lur)
{
        int    res  = 0;
        int    i;
        int    size;
        char   clusterName[MAPPER_MAX_CLUSTER_NAME];
        
        size = lur->procArrMaxSize;
        res  = get_mosix_processes(lur->procDir, MOS_PROC_GUEST,
                                   lur->procArr, &size);
        if(!res) {
                debug_lr(LUR_DEBUG, "Error reading mosix process information\n",
                         MOSIX_CURR_PRIO_FILENAME);
                goto out;
        }
        
        lur->procArrSize = size;
        //print_mosix_processes(lur->procArr, lur->procArrSize);

        for(i=0 ; i<size ; i++) {
                int r;
                proc_entry_t *e = &(lur->procArr[i]);
                r = mapper_addr2cluster(lur->partnersMap, &e->fromAddr, clusterName);
                if(r) {
                        //debug_ly(PRIOD_DEBUG, "Found cluster %s\n", clusterName);
                        updateUsingCluster(lur, clusterName, e);
                } else {
                        debug_lr(LUR_DEBUG, "Strange, guest process is not from any partner\n");
                        updateUsingCluster(lur, UNKNOWN_CLUSTER_NAME, e);
                }
        }
        
 out:
        return res;
        
}

// Called when the node is in used by the owner (priority = 0)
static
int findLocalUsedByInfo(lur_t lur)
{
        int    res  = 0;
        int    i;
        int    size;
        
        size = lur->procArrMaxSize;
        res  = get_mosix_processes(lur->procDir, MOS_PROC_ALL,
                                   lur->procArr, &size);
        if(!res) {
                debug_lr(LUR_DEBUG, "Error reading mosix process information\n",
                         MOSIX_CURR_PRIO_FILENAME);
                goto out;
        }
        
        lur->procArrSize = size;
        //print_mosix_processes(lur->procArr, lur->procArrSize);

        // Iterating over the processes (usually all of them should be local)
        // and adding the processes which are non-deputy and guests
        for(i=0 ; i<size ; i++) {

                
                proc_entry_t *e = &(lur->procArr[i]);

                //debug_lr(LUR_DEBUG, "Checking local use: %s\n", e->name);
                if(is_mosix_process_non_deputy(e) ||
                   is_mosix_process_guest(e)) {
                        //debug_lr(LUR_DEBUG, "Process is ok: !!%s!!\n", e->name);
                        if(strcmp(e->name, MSX_MOSIXD_NAME) == 0) {
                                
                                e->uid = getNonMosixUid(NON_MOSIX_PROC_USER);
                                e->euid = e->uid;
                                //debug_lr(LUR_DEBUG, "Process is mosixd: %s uid %d\n", e->name, e->uid);
                        }
                        updateUsingCluster(lur, LOCAL_CLUSTER_NAME, e);
                }
        }
        res = 1;
 out:
        return res;
        
}

static uid_t getNonMosixUid(char *name)
{
        static int   first = 1; 
        static uid_t uid = 0;

        if(first) {
                struct passwd *pw;

                pw = getpwnam(name);
                if(!pw) {
                        return uid;
                }
                uid = pw->pw_uid;
                first = 0;
        }
        return uid;
}

void printUsingInfo(lur_t lur) {
        int      i;
        int      len;
        char    *currPtr = lur->tmpBuff;
        
        len = sprintf(currPtr, "Using Clusters:\n");
        currPtr += len;
        len = writeUsageInfo(lur->status, lur->usedByInfo,
                             lur->usingClusters, currPtr);
        currPtr += len;
        sprintf(currPtr, "\n");
        printf("%s", lur->tmpBuff);
}

/* /\* */
/*  * Writing the information file so that the infod can read it. */
/*  * The write is atomic, first we write the file to a temporary */
/*  * name and then we call rename */
/*  *\/ */
/* int writeUsageInfoFile() */
/* { */
/*         int      res; */
/*         int      len; */
/*         int      fd; */
/*         char     infoTmpPath[256]; */

        
/*         len = writeUsageInfo(priod.status, priod.usedByInfo, priod.usingClusters, */
/*                              priod.tmpBuff); */
        
        
/*         sprintf(infoTmpPath, "%s/%s.%d", */
/*                 priod.infoDirName, PRIOD_DEF_INFO_FILE_PREFIX, getpid()); */
/* 	if(priod.debug) { */
/* 		debug_lb(PRIOD_DEBUG, "info tmp name %s\n", infoTmpPath); */
/* 	} */
/* 	fd= open(infoTmpPath, O_WRONLY | O_CREAT | O_TRUNC, 0644 ); */
/* 	if(fd == -1) { */
/* 		fprintf(stderr, "Failed opening %s\n", infoTmpPath); */
/* 		return 0; */
/* 	} */
/* 	res = write(fd, priod.tmpBuff, len); */
/* 	if(res != len) { */
/* 		fprintf(stderr, "Error while writing tmp lc"); */
/* 		return 0; */
/* 	} */

/* 	close(fd); */

/* 	res = rename(infoTmpPath, priod.infoFileName); */
/* 	if(res == -1) { */
/* 		fprintf(stderr, "Error while renaming %s to %s\n", */
/* 			infoTmpPath, priod.infoFileName); */
/* 		return 0; */
/* 	} */
/* 	return 1; */
/* } */

/* int touchUsageInfoFile() { */

/*         utime(priod.infoFileName, NULL); */
/*         return 1; */
/* } */

/* int updateUsedByInfo(int currPrio) */
/* { */
/*         // Each time starting from 0 using clusters */
/*         priod.usingClusters = 0; */
        
/*         // If used by owner doing nothing */
/*         if(MSX_IS_LOCAL_PRIO(currPrio)) { */
/*                 if(priod.verbose) */
/*                         fprintf(stderr, "Used by Local processes\n"); */
                
/*                 // First time we go from any type of usage to local usage */
/*                 priod.status = UB_STAT_LOCAL_USE; */
/*                 findLocalUsedByInfo(); */
/*                 writeUsageInfoFile(); */
/*         } */
/*         else if(MSX_IS_AVAIL_PRIO(currPrio)) { */
/*                 if(priod.debug) */
/*                         fprintf(stderr, "Currently Free\n"); */
/*                 // First time we go from any type of usage to available */
/*                 if(priod.status != UB_STAT_FREE) { */
/*                         priod.status = UB_STAT_FREE; */
/*                         writeUsageInfoFile(); */
/*                 } */
/*                 else { */
/*                         touchUsageInfoFile(); */
/*                 } */
/*         } */
/*         // We have grid processes  */
/*         else if(MSX_IS_GRID_PRIO(currPrio)) { */
                
/*                 priod.status = UB_STAT_GRID_USE; */
/*                 findGridUsedByInfo(); */
/*                 //if(priod.debug) */
/*                 //printUsingInfo(); */
/*                 writeUsageInfoFile(); */
/*         } */
/*         return 1; */
/* } */
