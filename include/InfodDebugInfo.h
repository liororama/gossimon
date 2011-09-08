/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __INFOD_DEBUG_INFO
#define __INFOD_DEBUG_INFO

typedef struct _infod_debug_info {
        int   childNum;
        int   uptimeSec;
} infod_debug_info_t;

int getInfodDebugInfo(char *str, infod_debug_info_t *idbg);
#endif
