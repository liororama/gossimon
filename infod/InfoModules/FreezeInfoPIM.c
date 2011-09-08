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

#include <parse_helper.h>
#include <provider.h>
#include <FreezeInfo.h>
#include <FreezeInfoPIM.h>

#include <msx_error.h>
#include <msx_debug.h>

#include "infoModuleManager.h"

typedef struct frzinfo_pim_struct {
        char           freezeConfFile[256];
        freeze_info_t  freezeInfo;
        int            reloadAfter;
        int            currCycle;
} frzinfo_pim_t;

/**************************************************************************
 * Init string :  freeze-conf-file : reload-file-cycles
 **************************************************************************/

int frzinfo_pim_init(void **module_data, void *module_init_data)
{
        frzinfo_pim_t *fi;

	pim_init_data_t *pim_init = (pim_init_data_t *) module_init_data;
	char         *argsStr = (char *)pim_init->init_data;

        int    size = 50;
        char  *args[50];
        
        fi = malloc(sizeof(frzinfo_pim_t));
        if(!fi)
                return 0;

        bzero(fi, sizeof(frzinfo_pim_t));
        int items;
        items = split_str(argsStr, args, size, ":"); 
        if(items < 2)
                return 0;
        
        strcpy(fi->freezeConfFile, args[0]);
        fi->reloadAfter = atoi(args[1]);
        fi->currCycle = -1;
        *module_data = fi;
 
        return 1;
}
int frzinfo_pim_free(void **module_data)
{
        frzinfo_pim_t *fi = (frzinfo_pim_t *) (*module_data); 

        if(!fi)
                return 0;
        free_freeze_dirs(&fi->freezeInfo);
        
        free(fi);
	*module_data = NULL;
        return 1;
        
}
int frzinfo_pim_update(void *module_data)
{
        frzinfo_pim_t *fi = (frzinfo_pim_t *) module_data; 
        int            res;
        
        if(!fi)
                return 0;

        fi->currCycle++;
        
        if(fi->currCycle >= fi->reloadAfter)
                fi->currCycle = 0;

        if(fi->currCycle == 0) {
                res = get_freeze_dirs(&fi->freezeInfo, fi->freezeConfFile);
                if(!res) {
                        fi->freezeInfo.status = FRZINF_ERR;
                        fi->freezeInfo.freeze_dirs_num = 0;
                        return 1;
                }
        }

        res = get_freeze_space(&fi->freezeInfo);
        if(res == 0) {
                fi->freezeInfo.status = FRZINF_ERR;
        }
        fi->freezeInfo.status = FRZINF_OK;
        return 1;
}

int frzinfo_pim_get(void *module_data, void *data, int *size)
{
        int            res;
	frzinfo_pim_t *fi = (frzinfo_pim_t *)module_data;
        char          *buff = (char *) data;

        res = writeFreezeInfo(&fi->freezeInfo, buff);
        if(res) *size = strlen(buff)+1;
        return res;
}

int frzinfo_pim_desc(void *module_data, char *buff) {

  debug_ly(INFOD_DEBUG, "FREEZE-INFO: [%s]\n", ITEM_FREEZE_INFO_NAME);

  sprintf(buff, "\t<vlen name=\"%s\" type=\"string\" unit=\"xml\" />\n",
          ITEM_FREEZE_INFO_NAME);

  return 1;
}
