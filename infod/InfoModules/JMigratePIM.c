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
#include <JMigrateInfo.h>
#include <JMigratePIM.h>

/* An example of the xml we produce */
/* <echo-info> */
/*     <on-time>14.2</on-time> */
/*     <status>on</status> */
/*     <curr-price>12</curr-price> */
/*     <min-price>10</min-price> */
/*   </echo-info> */

static int  globBuffSize = 1024;
static char globBuff[1024];

typedef struct jmig_pim {
     char        fileName[256];  //The jmignomical status file name
     int         fileStatusOk;
     time_t      fileMTime;
     int         fileSize;
     char        jmigXml[1024];  // This will hold the xml string
} jmig_pim_t;


int writeErrorJmigXml(char *xmlStr, char *errorMsg)
{
     if(!xmlStr) 
          return 0;
     
     sprintf(xmlStr, "<%s><%s>%s</%s></%s>",
             ITEM_JMIGRATE_NAME,
             JMIG_ELEMENT_ERROR,
             errorMsg,
             JMIG_ELEMENT_ERROR,
             ITEM_JMIGRATE_NAME);
     return 1;
}


int jmig_pim_init(void **module_data, void *init_data)
{
     jmig_pim_t   *eInfo;
     char         *file = (char *)init_data;

     // If no file given this pim will not do anything
     if(!file)
	  return 0;
     
     eInfo = malloc(sizeof(jmig_pim_t));
     if(!eInfo)
	  return 0;
     
     strncpy(eInfo->fileName, file, 254);
     eInfo->fileName[254] = '\0';
     
     *module_data = eInfo;
     return 1;
}

int jmig_pim_free(void **module_data)
{
        jmig_pim_t *eInfo = (jmig_pim_t *) (*module_data); 

        if(!eInfo)
                return 0;
	free(eInfo);
	*module_data = NULL;
        return 1;
        
}

/*
  Reading the jmigrate status file. For now we take it as it is.
*/
int jmig_pim_update(void *module_data)
{
        jmig_pim_t *eInfo = (jmig_pim_t *) module_data; 
	int        fd = -1, size;

	debug_lr( KCOMM_DEBUG, "=============>> JmigPimUpdate <<============\n");
             
        if(!eInfo) {
             debug_lr(KCOMM_DEBUG, "BBBBBBad\n");
             return 0;
        }
        eInfo->jmigXml[0]    = '\0';
	eInfo->fileStatusOk = 0;

        // Doing stat for the mtime of the file
	struct stat st;
        if(stat(eInfo->fileName, &st) == -1){
             writeErrorJmigXml(eInfo->jmigXml, "stat");
             debug_lr( KCOMM_DEBUG, "Error stating jmigrate file file\n");
	     goto out;
        }
        eInfo->fileMTime = st.st_mtime;
        struct timeval tim;
        gettimeofday(&tim, NULL);
        if(tim.tv_sec - st.st_mtime  > JMIG_FILE_MAX_AGE) {
             writeErrorJmigXml(eInfo->jmigXml, "too old");
             debug_lr( KCOMM_DEBUG, "Error jmigrate file too old\n");
	     goto out;
        }    

        // Reading the file content
	fd = open(eInfo->fileName, O_RDONLY);
        if(fd == -1) {
             writeErrorJmigXml(eInfo->jmigXml, "open");
             debug_lr( KCOMM_DEBUG, "Error opening jmigrate status file\n");
             goto out;
	}
        
        size = read(fd, globBuff, globBuffSize -1);
        if(size == -1) {
             writeErrorJmigXml(eInfo->jmigXml, "read");
             debug_lr( KCOMM_DEBUG, "Error reading jmigrate status\n");
             goto out;
        }
	
        debug_lg(KCOMM_DEBUG, "Got jmigrate status:\n%s\n", globBuff);
        globBuff[size] = '\0';
	eInfo->fileSize = size+1;
        memcpy(eInfo->jmigXml, globBuff, eInfo->fileSize);

        
        eInfo->fileStatusOk = 1;
	
 out:
        if(fd != -1) close(fd);
        return 1;
}


int jmig_pim_get(void *module_data, void *data, int *size)
{
        int            res;
	jmig_pim_t    *eInfo = (jmig_pim_t *)module_data;
        char          *buff = (char *) data;

        res = sprintf(buff, "%s", eInfo->jmigXml);
        
	if(res) *size = strlen(buff)+1;
        return res;
}

int jmig_pim_desc(void *module_data, char *buff) {
  sprintf(buff, "\t<vlen name=\"" ITEM_JMIGRATE_NAME "\" type=\"string\" unit=\"xml\" />\n" );
  return 1;
}
