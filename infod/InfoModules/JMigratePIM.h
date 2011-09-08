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

#ifndef __JMIGRATE_INFO_PIM
#define __JMIGRATE_INFO_PIM

#include <infoModuleManager.h>

// If file mtime is more than 30 seconds the jmigrate is considered as dead

#define JMIG_FILE_MAX_AGE  (30)

int jmig_pim_init(void **module_data, void *init_data);
int jmig_pim_free(void **module_data);
int jmig_pim_update(void *module_data);
int jmig_pim_get(void *module_data, void *data, int *size);
int jmig_pim_desc(void *module_data, char *desc);


// A function to register the whole module
int jmig_pim_register(pim_entry_t *pim_arr, int maxSize);

#endif
