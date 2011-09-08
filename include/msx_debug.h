/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __UMOSIX_DEBUG__
#define __UMOSIX_DEBUG__

#include <stdio.h>

typedef struct msx_debug_ent {
  unsigned int    flag;
  char           *str;
  char           *shortStr;
} msx_debug_ent_t;
        
// Definition of debugging values
#define COMM_DEBUG     (0x000001)
#define COMM_DBG       (COMM_DEBUG)

#define MAP_DEBUG      (0x000002)
#define MAP_DBG        (MAP_DBG)

#define UPLIST_DEBUG   (0x000004)
#define UPLIST_DBG     UPLIST_DEBUG

#define READPROC_DEBUG (0x000004)
#define READPROC_DBG   READPROC_DEBUG

#define VEC_DEBUG      (0x000008)
#define VEC_DBG        VEC_DEBUG

#define WIN_DEBUG      (0x000010)
#define WIN_DBG        WIN_DEBUG

#define INFOD_DEBUG    (0x000020)
#define INFOD_DBG      INFOD_DEBUG
#define ID_DBG         INFOD_DEBUG

#define GINFOD_DEBUG   (0x000040)
#define GINFOD_DBG     GINFOD_DEBUG

#define GLOBAL_DEBUG   (0x000080)
#define GLOBAL_DBG     GLOBAL_DEBUG

#define CTL_DEBUG      (0x000100)
#define CTL_DBG        CTL_DEBUG

#define KCOMM_DEBUG    (0x000200)
#define KCOMM_DBG      KCOMM_DEBUG

#define CLEAR_DEBUG    (0x000400)
#define CLEAR_DBG      CLEAR_DEBUG

#define SETPE_DEBUG    (0x000800)
#define SETPE_DBG      SETPE_DEBUG

#define MMON_DEBUG     (0x001000)
#define MMON_DBG       MMON_DEBUG

#define LUR_DEBUG      (0x001000)
#define LUR_DBG        LUR_DEBUG

#define MCONF_DEBUG    (0x002000)
#define MCONF_DBG      MCONF_DEBUG

#define OWNERD_DEBUG   (0x004000)
#define PRIOD_DEBUG    (0x004000)
#define FREEZD_DEBUG   (0x008000)
#define PRIOD2_DEBUG   (0x008000)
#define MOSCTL_DEBUG   (0x010000)
#define UVR_DEBUG      (0x010000)
#define FST_DEBUG      (0x020000)
#define FST_DBG        FST_DEBUG

#define MEASURE_DEBUG  (0x020000)
#define MEASURE_DBG    MEASURE_DEBUG


// The default debug file to use
#define DEFAULT_DEBUG_FILE    "/tmp/infod.debug"

// This string represent output location 
#define MSX_DEBUG_OUT_FILE_STR "OUTPUT"


#define COMM_DEBUG_STR        "COMM_DBG"
#define MAP_DEBUG_STR         "MAP_DBG"
#define UPLIST_DEBUG_STR      "UPLIST_DBG"
#define READPROC_DEBUG_STR    "READPROC_DBG"
#define VEC_DEBUG_STR         "VEC_DBG"
#define WIN_DEBUG_STR         "WIN_DBG"
#define INFOD_DEBUG_STR       "INFOD_DBG"
#define GINFOD_DEBUG_STR      "GINFOD_DBG"
#define GLOBAL_DEBUG_STR      "GLOBAL_DBG"
#define CTL_DEBUG_STR         "CTL_DBG"
#define KCOMM_DEBUG_STR       "KCOMM_DBG"
#define CLEAR_DEBUG_STR       "CLEAR_DBG"
#define SETPE_DEBUG_STR       "SETPE_DBG"
#define MMON_DEBUG_STR        "MMON_DBG"
#define LUR_DEBUG_STR         "LUR_DBG"
#define MCONF_DEBUG_STR       "MCONF_DBG"
#define OWNERD_DEBUG_STR      "OWNERD_DBG"
#define FREEZD_DEBUG_STR      "FREEZD_DBG"
#define MOSCTL_DEBUG_STR      "MOSCTL_DBG"
#define MEASURE_DEBUG_STR     "MEASURE_DBG"
#define PRIOD_DEBUG_STR       "PRIOD_DBG"
#define PRIOD2_DEBUG_STR      "PRIOD2_DBG"
#define UVR_DEBUG_STR         "UVR_DBG"
#define FST_DEBUG_STR         "FST_DBG"


/* Bit mask for definining different debugging levels */
extern int msx_debug_level;

void msx_add_debug(int level);
void msx_set_debug(int dbg_level);
int  msx_get_debug();
int get_debug_from_short(char *debugStr);
FILE * msx_get_debug_fh();

int msx_read_debug_file(char *fname);


#endif /* __UMOSIX_DEBUG__ */
