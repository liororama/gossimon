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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <msx_error.h>
#include <msx_debug.h>
#include <parse_helper.h>
#include <provider.h>
#include <EcoInfo.h>
#include <EconomyPIM.h>

/* An example of the xml we produce */
/* <echo-info> */
/*     <on-time>14.2</on-time> */
/*     <status>on</status> */
/*     <curr-price>12</curr-price> */
/*     <min-price>10</min-price> */
/*   </echo-info> */

static int  globBuffSize = 1024;
static char globBuff[1024];

typedef struct eco_pim {
     char        fileName[256];  //The economical status file name
     int         fileStatusOk;
     time_t      fileMTime;
     int         fileSize;
     char        ecoXml[1024];  // This will hold the xml string
} eco_pim_t;


int writeErrorEcoXml(char *xmlStr, char *errorMsg)
{
     if(!xmlStr) 
          return 0;
     
     sprintf(xmlStr, "<%s><%s>%s</%s></%s>",
             ITEM_ECONOMY_STAT_NAME,
             ECO_ELEMENT_ERROR,
             errorMsg,
             ECO_ELEMENT_ERROR,
             ITEM_ECONOMY_STAT_NAME);
     return 1;
}


int eco_pim_init(void **module_data, void *init_data)
{
     eco_pim_t   *eInfo;
     char         *file = (char *)init_data;

     // If no file given this pim will not do anything
     if(!file)
	  return 0;
     
     eInfo = malloc(sizeof(eco_pim_t));
     if(!eInfo)
	  return 0;
     
     strncpy(eInfo->fileName, file, 254);
     eInfo->fileName[254] = '\0';
     
     *module_data = eInfo;
     return 1;
}

int eco_pim_free(void **module_data)
{
        eco_pim_t *eInfo = (eco_pim_t *) (*module_data); 

        if(!eInfo)
                return 0;
	free(eInfo);
	*module_data = NULL;
        return 1;
        
}

/*
  Reading the economy status file. For now we take it as it is.
*/
int eco_pim_update(void *module_data)
{
        eco_pim_t *eInfo = (eco_pim_t *) module_data; 
	int        fd = -1, size;

	debug_lr( KCOMM_DEBUG, "=============>> EcoPimUpdate <<============\n");
             
        if(!eInfo)
                return 0;

        eInfo->ecoXml[0]    = '\0';
	eInfo->fileStatusOk = 0;

        // Doing stat for the mtime of the file
	struct stat st;
        if(stat(eInfo->fileName, &st) == -1){
             writeErrorEcoXml(eInfo->ecoXml, "stat");
             debug_lr( KCOMM_DEBUG, "Error stating economy file file\n");
	     goto out;
        }
        eInfo->fileMTime = st.st_mtime;
        struct timeval tim;
        gettimeofday(&tim, NULL);
        if(tim.tv_sec - st.st_mtime  > ECO_FILE_MAX_AGE) {
             writeErrorEcoXml(eInfo->ecoXml, "too old");
             debug_lr( KCOMM_DEBUG, "Error economy file too old\n");
	     goto out;
        }    

        // Reading the file content
	fd = open(eInfo->fileName, O_RDONLY);
        if(fd == -1) {
             writeErrorEcoXml(eInfo->ecoXml, "open");
             debug_lr( KCOMM_DEBUG, "Error opening economy status file\n");
             goto out;
	}
        
        size = read(fd, globBuff, globBuffSize -1);
        if(size == -1) {
             writeErrorEcoXml(eInfo->ecoXml, "read");
             debug_lr( KCOMM_DEBUG, "Error reading economy status\n");
             goto out;
        }
	
        debug_lg(KCOMM_DEBUG, "Got economy status:\n%s\n", globBuff);
        globBuff[size] = '\0';
	eInfo->fileSize = size+1;
        memcpy(eInfo->ecoXml, globBuff, eInfo->fileSize);

        
        eInfo->fileStatusOk = 1;
	
 out:
        if(fd != -1) close(fd);
        return 1;
}


int eco_pim_get(void *module_data, void *data, int *size)
{
        int            res;
	eco_pim_t     *eInfo = (eco_pim_t *)module_data;
        char          *buff = (char *) data;

        res = sprintf(buff, "%s", eInfo->ecoXml);
        
	if(res) *size = strlen(buff)+1;
        return res;
}

int eco_pim_desc(void *module_data, char *buff) {
  sprintf(buff, "\t<vlen name=\"" ITEM_ECONOMY_STAT_NAME "\" type=\"string\" unit=\"xml\" />\n" );
  
  return 1;
}
