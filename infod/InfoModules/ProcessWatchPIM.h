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

#ifndef __PROCESS_WATCH_PIM
#define __PROCESS_WATCH_PIM

int procwatch_pim_init(void **module_data, void *init_data);
int procwatch_pim_free(void **module_data);
int procwatch_pim_update(void *module_data);
int procwatch_pim_get(void *module_data, void *data, int *size);
int procwatch_pim_desc(void *module_data, char *buff);

#endif
