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

/******************************************************************************
 * File: UsedByPIM Implementation of a used by info module for infod
 *
 * This module collect used by information and supply a pim interface
 *****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <errno.h>

#include <info.h>
#include <msx_error.h>
#include <msx_debug.h>
#include <msx_proc.h>
#include <msx_common.h>
#include <provider.h>

#include <Mapper.h>
#include <MapperBuilder.h>
#include <infoModuleManager.h>
#include <UsedByInfo.h>
#include <LocalUsageRetriever.h>

#define MAX_UB_INFO_SIZE       (20)
#define REBUILD_MAP_CYCLE      (36)
struct usedby_pim_data
{
        int                  cycle;
        lur_t                lur;
        char                *data;
        node_usage_status_t  ubStatus;
        int                  ubMaxSize;
        int                  ubSize;
        used_by_entry_t      ubInfo[MAX_UB_INFO_SIZE];;
        int                  size;

};

int usedby_pim_init(void **data, void *init_data)
{
        lur_t lur;
        mapper_t mapper;

        // Building a partners mapper
        mapper = BuildPartnersMap(MOSIX_PARTNERS_DIR, PARTNER_SRC_TEXT);
        if(!mapper) {
		debug_lr(LUR_DEBUG, "PIM USEDBY: Working without partner map\n");
        }
	
        lur = lur_init("/proc", mapper, 500, NULL);
        if(!lur) {
                mapperDone(mapper);
        }
        
        struct usedby_pim_data *pimdata;
        pimdata = malloc(sizeof(struct usedby_pim_data));
        if(!pimdata) {
                mapperDone(mapper);
                lur_free(lur);
                return 0;
        }

        // Setting the UsedByPIM private data
        pimdata->cycle  = 0;
        pimdata->lur    = lur;
        pimdata->data   = NULL;
        pimdata->size   = 0;
        pimdata->ubSize = 0;
        pimdata->ubMaxSize = MAX_UB_INFO_SIZE;
        pimdata->ubStatus = UB_STAT_INIT;
        
        *data = pimdata;
        
        return 1;
}

int usedby_pim_free(void **data)
{
        struct usedby_pim_data *pimdata = (struct usedby_pim_data *)*data;

        lur_free(pimdata->lur);
        return 1;
}

int usedby_pim_update(void *data)
{
        struct usedby_pim_data *pimdata = (struct usedby_pim_data *)data;
        int                     res;
        node_usage_status_t     status;

        if(pimdata->cycle >= REBUILD_MAP_CYCLE) {
                mapper_t mapper;
                
                pimdata->cycle = 0;
                // Building a partners mapper
                mapper = BuildPartnersMap(MOSIX_PARTNERS_DIR, PARTNER_SRC_TEXT);
                if(!mapper) {
                        debug_lr(LUR_DEBUG, "PIM USEDBY: Working without partner map\n");
                }
	
                lur_setMapper(pimdata->lur, mapper);
        }

        int size = pimdata->ubMaxSize;
        
        res = lur_getLocalUsage(pimdata->lur, &status, pimdata->ubInfo, &size);
        debug_ly(LUR_DEBUG, "PIM USEDBY: Calling lur_getLocalUsage res = %d\n", res);
        if(!res) {
                pimdata->ubStatus = UB_STAT_ERROR;
                pimdata->ubSize = 0;
        } else {
                pimdata->ubStatus = status;
                pimdata->ubSize = size;
        }
        pimdata->cycle++;
        return 1;
}

int usedby_pim_get(void *module_data, void *data, int *size)
{
        struct usedby_pim_data *pimdata = (struct usedby_pim_data *)module_data;
        
        writeUsageInfo(pimdata->ubStatus, pimdata->ubInfo, pimdata->ubSize,
                       (char *)data);
        *size = strlen((char *)data) + 1;
        return 1;
}

int usedby_pim_desc(void *module_data, char *buff) {
  sprintf(buff, "\t<vlen name=\"" ITEM_USEDBY_NAME "\" type=\"string\" unit=\"xml\" />\n" );
  return 1;
}

