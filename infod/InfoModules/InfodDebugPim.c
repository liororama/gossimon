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
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#include <msx_error.h>
#include <msx_debug.h>
#include <parse_helper.h>
#include <infod.h>
#include <info.h>
#include <InfodDebugPim.h>
#include "infoModuleManager.h"

typedef struct idbg_pim {
        struct    infod_runtime_info  *infodRTI;
        int       infodUptimeSec;
} idbg_pim_t;

int idbg_pim_init(void **module_data, void *module_init_data)
{
	idbg_pim_t    *idbg;

	pim_init_data_t *pim_init = (pim_init_data_t *) module_init_data;

	struct infod_runtime_info  *irti = (struct infod_runtime_info *)pim_init->init_data;

        if(!irti)
                return 0;
	idbg = malloc(sizeof(idbg_pim_t));
	if(!idbg) return 0;
        
	bzero(idbg, sizeof(idbg_pim_t));
        idbg->infodRTI = irti;
	*module_data = idbg;
        return 1;

}

int idbg_pim_free(void **module_data) {
	idbg_pim_t *idbg = (idbg_pim_t*) (*module_data);
	
	if(!idbg)
		return 0;
	free(idbg);
	*module_data = NULL;
        return 1;
}

int idbg_pim_update(void *module_data)
{
	idbg_pim_t   *idbg = (idbg_pim_t*)module_data;

        struct timeval tim;

        gettimeofday(&tim, NULL);

        idbg->infodUptimeSec = tim.tv_sec - idbg->infodRTI->startTime.tv_sec;
        return 1;
}

static 
void writeIdbgInfo(idbg_pim_t *idbg, char *buff) {

        sprintf(buff, "<%s nchild=\"%d\">%u</%s>",
                ITEM_INFOD_DEBUG_NAME, idbg->infodRTI->childNum,
                idbg->infodUptimeSec, ITEM_INFOD_DEBUG_NAME);
}

int idbg_pim_get(void *module_data, void *data, int *size)
{
     idbg_pim_t    *idbg = (idbg_pim_t *)module_data;
     char          *buff = (char *) data;
     
     writeIdbgInfo(idbg, buff);
     *size = strlen(buff)+1;
     debug_lg(INFOD_DBG, "Infod debug module:\n%s\n\n", buff);
     return 1;
}


int idbg_pim_desc(void *module_data, char *buff) {
  sprintf(buff, "\t<vlen name=\"%s\" type=\"string\" unit=\"xml\" />\n",
	  ITEM_INFOD_DEBUG_NAME);

  return 1;
}

