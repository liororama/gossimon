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


#ifndef __NET_WATCH_PIM
#define __NET_WATCH_PIM

int netwatch_pim_init(void **module_data, void *init_data);
int netwatch_pim_free(void **module_data);
int netwatch_pim_update(void *module_data);
int netwatch_pim_get(void *module_data, void *data, int *size);
int netwatch_pim_desc(void *module_data, char *buff);

#define NET_WATCH_PROC_FILE   "/proc/net/dev"
#endif
