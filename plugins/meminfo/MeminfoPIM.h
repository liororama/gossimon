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

#ifndef __MEMINFO_PIM
#define __MEMINFO_PIM

extern "C"
{
int im_init(void **module_data, void *module_init_data);
int im_free(void **module_data);
int im_update(void *module_data);
int im_get(void *module_data, void *data, int *size);
int im_description(void *module_data, char *buff);
}

#endif
