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
 * File: ClusterID_CRC_PIM implementation of the cluster id crc pim
 *
 * This module find the crc32 of the cluster id (the cluster mapper)
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
#include <ClusterID_CRC_PIM.h>

#define CMAPPER_REFRESH_ITER  (60)
struct cidcrc_pim_data
{
        unsigned int   cidcrc;
        mapper_t       cmapper;
        char          *cmapfile;
        int            counter;
};

int cidcrc_pim_init(void **data, void *module_init_data)
{
        struct cidcrc_pim_data *pimdata;
        pimdata = malloc(sizeof(struct cidcrc_pim_data));
        if(!pimdata) {
                return 0;
        }
	
	pim_init_data_t *pim_init = (pim_init_data_t *) module_init_data;
	char         *initStr = (char *)pim_init->init_data;


        pimdata->cidcrc   = 0;
        pimdata->cmapper  = NULL;
        pimdata->cmapfile = strdup(initStr);
        pimdata->counter  = 0;
        *data = pimdata;
        return 1;
}

int cidcrc_pim_free(void **data)
{
        struct cidcrc_pim_data *pimdata = (struct cidcrc_pim_data *)*data;
        
        if(!pimdata)
                return 0;
        if(pimdata->cmapper)
                mapperDone(pimdata->cmapper);
        if(pimdata->cmapfile)
                free(pimdata->cmapfile);

        free(pimdata);
        return 1;
}

int cidcrc_pim_update(void *data)
{
        struct cidcrc_pim_data *pimdata = (struct cidcrc_pim_data *)data;

        // Reloading the cluster mapper
        if(pimdata->counter % CMAPPER_REFRESH_ITER == 0)
        {
                // Freeing the previous mapper
                if(pimdata->cmapper)
                        mapperDone(pimdata->cmapper);
                // Building a new one
                
                pimdata->cmapper = BuildMosixMap(pimdata->cmapfile,
                                                 strlen(pimdata->cmapfile) + 1,
                                                 INPUT_FILE);
                // Getting the CRC
                if(pimdata->cmapper) {
                        pimdata->cidcrc = mapperGetCRC32(pimdata->cmapper);
                } else {
                        pimdata->cidcrc = 0;
                }
                debug_lr(KCOMM_DEBUG, "PIM CIDCRC: mapper %p file %s crc %u\n",
                         pimdata->cmapper, pimdata->cmapfile,pimdata->cidcrc);
        
                
        }
        pimdata->counter ++;
        return 1;
}

int cidcrc_pim_get(void *module_data, void *data, int *size)
{
        struct cidcrc_pim_data *pimdata = (struct cidcrc_pim_data *)module_data;
        
        char  *buff = (char *)data;
        
        sprintf(buff, "<%s>%u</%s>",
                ITEM_CID_CRC_NAME, pimdata->cidcrc, ITEM_CID_CRC_NAME);
        *size = strlen(buff) + 1;
        return 1;
}


int cidcrc_pim_desc(void *module_data, char *buff) {
  sprintf(buff, "\t<vlen name=\"" ITEM_CID_CRC_NAME "\" type=\"string\" unit=\"xml\" />\n" );

  return 1;
}

int cidcrc_pim_register(pim_entry_t *ent) {
  return 0;
}
