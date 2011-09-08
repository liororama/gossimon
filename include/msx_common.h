/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

#ifndef __MOSIX2_COMMON__
#define __MOSIX2_COMMON__

#include <sys/socket.h>

// Maximum length of host name (just in case)
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN   (256)
#endif

struct mosixnet
{
        int base;
        short x;
        unsigned char outsider:1;
        unsigned char dontgo:1;
        unsigned char donttake:1;
        unsigned char proximate:1;
        unsigned char w:4;
        char y;
        int ip;
        int owner;
        long z;
        int n;
};

/*
 * MOSIX API
 */

#define MAX_MOSNET_ENTS    (256)
#define MOSIX_MAX          ((1<<24) -1)   // 16777215
#define MOSIX_STD_SPEED    (10000)
#define ALL_TUNE_CONSTS    32
#define MAX_MOSIX_TOPOLOGY 10
#define MSX_AVAIL_NODE_PRI (65535)


/* status bits: */
#define MSX_STATUS_CONF         1
#define MSX_STATUS_UP           2
#define MSX_STATUS_STAY         4
#define MSX_STATUS_LSTAY        8
#define MSX_STATUS_BLOCK        16
#define MSX_STATUS_QUIET        32
#define MSX_STATUS_NOMFS        40
#define MSX_STATUS_TOKEN        128
#define MSX_STATUS_NO_PROVIDER  1024
#define MSX_STATUS_VM           2048


/********************************************
 * Type of guest
 ********************************************/

typedef enum  {
	GRID_IS_GUEST = 1,    // Other clusters are guests
	CLUSTER_IS_GUEST, // Other partitions are guests
	
} guest_t;

// Default system filenames
#define MOSIX_MAP_FILENAME        "/etc/mosix/mosix.map"
#define MOSIX_PARTNERS_DIR        "/etc/mosix/partners"
#define MOSIX_GRID_DIR            "/etc/mosix/grid"
#define MOSIX_GRID_DIR_LOCK       "/etc/mosix/.gridlock"
#define MCONF_FILENAME            "/etc/mosix/mosix.conf"
#define FREEZE_CONF_FILENAME      "/etc/mosix/freeze.conf"
#define CONFIG_FILENAME		  "/proc/mosix/config"
#define MOSPE_FILENAME		  "/proc/mosix/admin/mospe"
#define LOCALS_FILENAME           "/proc/mosix/locals"
#define GUESTS_FILENAME           "/proc/mosix/guests"
#define MAXGUESTS_FILENAME        "/proc/mosix/maxguests"
#define MOSIP_FILENAME            "/proc/mosix/mosip"
#define MOSIX_CURR_PRIO_FILENAME  "/proc/mosix/priority"
#define MOSIX_KERNEL_VER_FILENAME "/proc/mosix/version"

#define MOSIX_INFO_FILENAME       "/etc/mosix/var/.mosixinfo"
#define MOSIX_SELF_INFO_FILENAME  "/etc/mosix/var/.mymosixinfo"
#define MOSIX_INFO_FILENAME_OLD   "/var/.mosixinfo"

// Checkpoint stuff
#define MOSIX_CHKPT_FILENAME      "/proc/self/checkpoint"


// Proc stuff
#define MSX_PROC_PID_WHERE        "where"
#define MSX_PROC_PID_FROM         "from"
#define MSX_PROC_PID_FREEZER      "freezer"

#define MSX_PROC_PID_NO_MSX_VAL   (-3)
#define MSX_PROC_PID_NO_MSX_STR   "-3"

#define MSX_WHERE_IS_HERE_STR     "0"
#define MSX_FROM_IS_HERE_STR      "0"

// Deamons names
#define MSX_MOSIXD_NAME           "mosixd"


#include <limits.h>
// MACROS to check values of fields
#define MSX_IS_AVAIL_PRIO(prio)   ((prio) == USHRT_MAX)
#define MSX_IS_GRID_PRIO(prio)        ((prio) > 0 && (prio) <  USHRT_MAX)
#define MSX_IS_LOCAL_PRIO(prio)       ((prio) == 0)

#include <infoitems.h>

#endif
