/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __FREEZE_INFO
#define __FREEZE_INFO


typedef enum {
        FRZINF_INIT = 0,
        FRZINF_ERR,
        FRZINF_OK,
} freeze_info_status_t;

#define MAX_FREEZE_DIRS   (20)
typedef struct _freeze_info {
        char          *freeze_dirs[MAX_FREEZE_DIRS];
        short          freeze_dirs_num;

        freeze_info_status_t status;
        unsigned int   total_mb;
        unsigned int   free_mb;
} freeze_info_t;

/*
Format of xml 
<freeze-info stat="OK" total="1000" free="900" />


*/

char *freezeInfoStatusStr(freeze_info_status_t st);
int   strToFreezeInfoStatus(char *str, freeze_info_status_t *stat);

// Translating from a string to a list of proc-watch entries
int  getFreezeInfo(char *str, freeze_info_t *fi);
// Converting to a string a list of proc-watch entries
int  writeFreezeInfo(freeze_info_t *fi, char *buff);

#endif
