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

#ifndef __TOP_USAGE_PIM_H__
#define __TOP_USAGE_PIM_H__

#include <glib.h>
#include <stdio.h> 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <term.h>
#include <termios.h>
#include <dirent.h>

#define FILE_READER_MAX_FILES     (100)  // Maximal number external files to handle

// module info
struct filereader_module_data {
     int                fileNum;
     int                maxFiles;
     data_file_info_t  *fileDataArr;
};

typedef struct inf_module_data topusage_module_t ;
// init

int topusage_pim_init(void **module_data, void *module_init_data);

// update
int topusage_pim_update(void *module_data);

// get data (data points to xml string with output)
int topusage_pim_get(void *module_data, void *data, int *size);

// cleanup
int topusage_pim_free(void **module_data);

int topusage_pim_desc(void *module_data, char *buff);
#endif
