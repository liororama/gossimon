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
#include "InfodDebugPim.h"
#include "infod.h"
#include "infoModuleManager.h"

char moduleName[] = "dummy";
char im_name[] =  "dummy";
int  im_period = 10;

typedef struct dummy_pim {
    char        dataStr[100];
    int         value;
} dummy_pim_t;

int im_init(void **module_data, void *module_init_data)
{
  dummy_pim_t    *dummy;

	//pim_init_data_t *pim_init = (pim_init_data_t *) module_init_data;

        debug_lb(INFOD_DBG, "Dummy..... init()\n");

	//struct infod_runtime_info  *irti = (struct infod_runtime_info *)pim_init->init_data;

        //if(!irti)
        //        return 0;
	dummy = malloc(sizeof(dummy_pim_t));
	if(!dummy) return 0;
        
	bzero(dummy, sizeof(dummy_pim_t));
        dummy->value = 11;
	*module_data = dummy;
        return 1;

}

int im_free(void **module_data) {
	dummy_pim_t *dummy = (dummy_pim_t*) (*module_data);
	
	if(!dummy)
		return 0;
	free(dummy);
	*module_data = NULL;
        return 1;
}

int im_update(void *module_data)
{
  //	dummy_pim_t   *dummy = (dummy_pim_t*)module_data;

        struct timeval tim;

        gettimeofday(&tim, NULL);
        return 1;
}

static 
void writedummyInfo(dummy_pim_t *dummy, char *buff) {

        sprintf(buff, "<%s value=\"%d\">%d</%s>",
                moduleName, dummy->value, dummy->value, moduleName);
}

int im_get(void *module_data, void *data, int *size)
{
     dummy_pim_t    *dummy = (dummy_pim_t *)module_data;
     char          *buff = (char *) data;
     
     writedummyInfo(dummy, buff);
     *size = strlen(buff)+1;
     debug_lg(INFOD_DBG, "Infod debug module:\n%s\n\n", buff);
     return 1;
}


int im_description(void *module_data, char *buff) {
  sprintf(buff, "\t<vlen name=\"%s\" type=\"string\" unit=\"xml\" />\n",
	  moduleName);

  return 1;
}

