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



#ifndef __PROVIDER_INFO_MODULE
#define __PROVIDER_INFO_MODULE

#define PIM_MAX_PATH_LEN       (1024)
#define PIM_MAX_NAME_LEN       (256)

/*************************************************************
 * info functions handling. Beside the regular information,
 * the mosix provider support usage of info modules. Each such
 * module (used by, external) should supply 2 functions one
 * for initialization and the other for obtaining the data. 
 *************************************************************/

// Initialization function of the module, the data parameter keeps information
// private to the module
#define IM_INIT_FUNC_NAME   "im_init"
typedef int (*info_module_init_func_t)  (void **module_data, void *module_init_data);

// Initialization function of the module, the data parameter keeps information
// private to the module
#define IM_FREE_FUNC_NAME   "im_free"
typedef int (*info_module_free_func_t)    (void **data);

// Update info function. Retreiving the information
#define IM_UPDATE_FUNC_NAME  "im_update"
typedef int (*info_module_update_func_t)  (void *module_data);

// Get info function. Returning the information collected in the update function
#define IM_GET_FUNC_NAME     "im_get"
typedef int (*info_module_get_func_t)   (void *module_data, void *data, int *size);

// Get description func. Returning the desciption of the pim information
#define IM_DESCRIPTION_FUNC_NAME "im_description"
typedef int (*info_module_description_func_t)   (void *module_data, char *buff);

#define IM_NAME_SYMBOL_NAME   "im_name"
#define IM_PERIOD_SYMBOL_NAME "im_period"
#define IM_DEBUG_SYMBOL_NAME  "im_debug"


typedef struct _provider_info_module_entry {
     char                       *name;
     // Module interface functions
     info_module_init_func_t     init_func;
     info_module_free_func_t     free_func;
     info_module_update_func_t   update_func;
     info_module_get_func_t      get_func;
     info_module_description_func_t desc_func;
     // Time period in seconds to run the update of the module
     int                         period; // In seconds
     
     // Debug mode of the module
     int                         debug;
     // The private data of the info module
     void                       *private_data;
     // Data passed to module when the init function is called
     // The data passed here should be private to the module
     void                       *init_data;
     char                       *config_file; // Configuration file for the pim
} pim_entry_t;

typedef struct _module_init_data {
     void                       *init_data;
     char                       *config_file;
} pim_init_data_t;

int pim_register(pim_entry_t moduleEnt);


struct provider_info_modules {
        int             pimArrMaxSize;
        int             pimArrSize;
        pim_entry_t    *pimArr;
        struct timeval *lastUpdateTime;
        char           *validPim;
	char           *configFile;
        char           *tmpBuff;
};

typedef struct provider_info_modules *pim_t;

#include <info.h>

int     pim_setInitData(pim_entry_t *pimArr, char *pimName, void *data);
pim_t   pim_init(pim_entry_t *pimArr, char *configFile);
void    pim_free(pim_t pim);
int     pim_reload(pim_t pim);
int     pim_update(pim_t pim, struct timeval *currTime);
int     pim_packInfo(pim_t pim, node_info_t *ninfo);

// Return the a default array with all the default pims 
pim_entry_t *pim_getDefaultPIMs();

void    pim_appendDescription(pim_t pim, char *desc);

int load_external_module(pim_entry_t *ent, char *path, char *name);
#endif
