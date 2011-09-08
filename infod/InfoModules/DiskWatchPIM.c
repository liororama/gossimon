/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


/********************************************************************
 * Author: Lior Amar (ClusterLogic Ltd)
 *******************************************************************/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <msx_error.h>
#include <msx_debug.h>
#include <infoxml.h>
#include <parse_helper.h>
#include <provider.h>
#include <TimeUtil.h>
#include <DiskWatchPIM.h>

#define NIC_MAX_NAME_LEN    (24)
typedef struct diskDeviceInfo {
     char      name[NIC_MAX_NAME_LEN + 1];

     long long      lastSend;
     long long      lastRecv;
     
     float          sendMBSec;
     float          recvMBSec;

} nicInfo_t;

#define MAX_NICS            (10)
typedef struct disk_watch {
     int                  maxSize;
     int                  size;
     struct timeval       lastTime;
     nicInfo_t            nicInfoArr[MAX_NICS];
} nw_pim_t;

typedef struct proc_watch *proc_watch_t;


int diskwatch_pim_init(void **module_data, void *module_init_data)
{
	nw_pim_t     *nw;
 	char         *initStr = (char *)init_data;

        int           size = MAX_NICS;
        int           linesNum;
        char         *linesArr[MAX_NICS];
        
	if(!(nw = malloc(sizeof(nw_pim_t))))
	     return 0;
	*module_data = nw;
	memset(nw, 0, sizeof(nw_pim_t));
	nw->maxSize = MAX_NICS;

	pim_init_data_t *pim_init = (pim_init_data_t *) module_init_data;
	char         *initStr = (char *)pim_init->init_data;
	
        // Nothing to parse
	if(!initStr) {
                return 1;
	}

	// FIXME - fix split_str to take care of regexp
	
	linesNum = split_str(initStr, linesArr, size, ",");
	debug_lb(FST_DBG, "DiskWatch: got ifaces = %s num=%d\n",
		 initStr, linesNum);
		
	// Setting the diskwork interfaces names
        for(int i=0; i<linesNum && i < MAX_NICS ; i++) {
		strncpy(nw->nicInfoArr[i].name, linesArr[i], NIC_MAX_NAME_LEN);
		debug_lg(FST_DBG, "NW: Got nic %s\n", nw->nicInfoArr[i].name); 
		nw->size++;
        }

	gettimeofday(&nw->lastTime, NULL);
	
        return 1;
 failed:
	diskwatch_pim_free((void **)&nw);
	return 0;
}

int diskwatch_pim_free(void **module_data) {
	nw_pim_t *nw = (nw_pim_t*) (*module_data);
	
	if(!nw)
		return 0;
	free(nw);
	*module_data = NULL;
        return 1;
}

int nicInfo(nw_pim_t *nw, char *nicName)
{
     for(int i=0 ; i < nw->size ; i++) {
	  if(strcmp(nw->nicInfoArr[i].name, nicName) == 0) 
	       return &(nw->nicInfoArr[i]);
     }
     return NULL;
}

inline void parseNicInfo(char *line, unsigned int *rx_bytes, unsigned int *tx_bytes)
{
     unsigned int tmp;
     
     // Reading the device
     sscanf(line, "%u %u %u %u %u %u %u %u %u",
	    rx_bytes,
	    &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp,
	    tx_bytes);
}

int diskwatch_pim_update(void *module_data)
{
        int           res;
	nw_pim_t     *nw = (nw_pim_t*)module_data;

	int           dev_fd;
	char          line[256];
	char         *buff_ptr;
	char         *line_ptr;
	
	if((dev_fd = open(DISK_WATCH_PROC_FILE, O_RDONLY)) == -1)
		return 0;
	if((res = read(dev_fd, proc_file_buff, 8192)) == -1 ) {
		close(dev_fd);
		return 0;
	}
	close(dev_fd);

	struct timeval currTime;
	gettimeofday(&currTime, NULL);
	float secPassed = timeDiffFloat(&(nw->lastTime), &currTime);
	nw->lastTime = currTime;
	//nio->rx_bytes = 0;
	//nio->tx_bytes = 0;

	proc_file_buff[res] = '\0';
	buff_ptr = proc_file_buff;
	do {
		unsigned int rx_bytes, tx_bytes;
		buff_ptr = sgets(line, 256, buff_ptr);
		if(!(line_ptr = strchr(line, ':')))
			continue;
		
		*line_ptr = '\0';
		line_ptr++;

		// checking if the device is one of the watched one
		nicInfo_t *nic = nicInfo(nw, line);
		if(!nic)
		     continue;

		parseNicInfo(line_ptr, &rx_bytes, &tx_bytes);
		debug_lb(FST_DBG,"+++++++ %s: %u %u\n", line, rx_bytes, tx_bytes);


		nic->recvMBSec = ((rx_bytes - nic->lastRecv)/(1024.0*1024.0))/secPassed;
		nic->lastRecv = rx_bytes;

		nic->sendMBSec = ((tx_bytes - nic->lastSend)/(1024.0*1024.0))/secPassed;
		nic->lastSend = tx_bytes;

	}
	while(buff_ptr);
	return 1;
}

int diskwatch_pim_get(void *module_data, void *data, int *size)
{
	nw_pim_t      *nw = (nw_pim_t *)module_data;
        char          *buff = (char *) data;

	buff[0] = '\0';
        if(nw->size == 0) {
                *size = 0;
                return 1;
        }
	//debug_lb(FST_DBG, "NW: Getting xml\n"); 
	char tmp[256];
	sprintf(tmp, "<network>\n");
	
	strcat(buff, tmp);
	for(int i=0 ; i< nw->size ; i++) {
		nicInfo_t *nic = &(nw->nicInfoArr[i]);

		sprintf(tmp, "%s\t<%s>\n", XML_INFO_ITEM_IDENT_STR, nic->name);
		strcat(buff, tmp); 

		sprintf(tmp, "%s\t\t<sendRate>%.2f</sendRate>\n",
			XML_INFO_ITEM_IDENT_STR, nic->sendMBSec);
		strcat(buff, tmp); 

		sprintf(tmp, "%s\t\t<recvRate>%.2f</recvRate>\n",
			XML_INFO_ITEM_IDENT_STR, nic->recvMBSec);
		strcat(buff, tmp); 
		
		sprintf(tmp, "%s\t</%s>\n", XML_INFO_ITEM_IDENT_STR, nic->name);
		strcat(buff, tmp); 
	}
	sprintf(tmp, "%s</network>", XML_INFO_ITEM_IDENT_STR);
	strcat(buff, tmp);
	*size = strlen(buff)+1;
	//debug_lb(FST_DBG, "NW: Buff: [%s]\n", buff);
        return 1;
}

int diskwatch_pim_desc(void *module_data, char *buff) {
  sprintf(buff, "\t<vlen name=\"" ITEM_DISK_WATCH_NAME "\" type=\"string\" unit=\"xml\" />\n" );

}
