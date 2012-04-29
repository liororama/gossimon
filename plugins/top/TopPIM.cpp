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
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>


#include <ModuleLogger.h>
#include <msx_error.h>
#include <msx_debug.h>
#include <parse_helper.h>
#include "infoitems.h"

#include <TopFinder.h>
extern "C" {
    

char moduleName[] = "top";
char im_name[] =  "top";
int  im_debug = 0;
int  im_period = 10;

int mlog_id;
int mlog_id_2;

typedef struct dummy_pim {
    char        dataStr[100];
    int         value;
} dummy_pim_t;

int im_init(void **module_data, void *module_init_data)
{
 
    mlog_registerModule("top", "Getting info about the most cpu intensive processes", "top");
    mlog_registerModule("top2", "Getting info about the most cpu intensive processes 2", "top2");

    mlog_getIndex("top", &mlog_id);
    mlog_getIndex("top2", &mlog_id_2);

    mlog_dg(mlog_id, "im_init() \n");
    //pim_init_data_t *pim_init = (pim_init_data_t *) module_init_data;


    //struct infod_runtime_info  *irti = (struct infod_runtime_info *)pim_init->init_data;

    //if(!irti)
    //        return 0;
    TopFinder *tf = new TopFinder("/proc");
    if(!tf) return 0;
    tf->set_mlog_id(mlog_id);
    tf->set_mlog_id2(mlog_id_2);
    *module_data = (void *) tf;

    mlog_dg(mlog_id, "After TopFinder constructor\n%s\n", tf->get_info().c_str());
    
    sleep(3);
    return 1;

}

int im_free(void **module_data) {
    TopFinder *tf = (TopFinder *) (*module_data);
	
    if(!tf)
        return 0;
    delete(tf);
    *module_data = NULL;
    return 1;
}

int im_update(void *module_data)
{
    TopFinder *tf = (TopFinder *) module_data;

    //struct timeval tim;
    tf->update();
    //gettimeofday(&tim, NULL);
    return 1;
}

int im_get(void *module_data, void *data, int *size)
{
    TopFinder *tf = (TopFinder *) module_data;
    char          *buff = (char *) data;

    if(tf->get_xml(buff, size))
    	return 1;
    return 0;
}


int im_description(void *module_data, char *buff) {
    sprintf(buff, "\t<vlen name=\"%s\" type=\"string\" unit=\"xml\" />\n",
            moduleName);

    return 1;
}
}
