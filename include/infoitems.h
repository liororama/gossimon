/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


/*
 *
 *  Author(s): Amar Lior Ilan Peer
 * 
 */

#ifndef __MOSIX2_INFO_ITEMS__
#define __MOSIX2_INFO_ITEMS__


/****************************************************************************
 * Names used to retrieve information from the infod
 ***************************************************************************/


#define ITEM_TMEM_NAME              "tmem"
#define ITEM_TSWAP_NAME             "tswap"
#define ITEM_FSWAP_NAME             "fswap"
#define ITEM_UPTIME_NAME            "uptime"
#define ITEM_DISK_READ_RATE         "disk_read_rate"
#define ITEM_DISK_WRITE_RATE        "disk_write_rate"
#define ITEM_NET_RX_RATE            "net_rx_rate"
#define ITEM_NET_TX_RATE            "net_tx_rate"
#define ITEM_NFS_CLIENT_RPC_RATE    "nfs_client_rpc_rate"

#define ITEM_SPEED_NAME             "speed"
#define ITEM_IOSPEED_NAME           "iospeed"
#define ITEM_NTOPOLOGY_NAME         "ntopology"
#define ITEM_NCPUS_NAME             "ncpus"
#define ITEM_LOAD_NAME              "load"
#define ITEM_ELOAD_NAME             "export_load"
#define ITEM_PRIO_NAME              "level"
#define ITEM_FROZEN_NAME            "frozen"
#define ITEM_UTIL_NAME              "util"
#define ITEM_IOWAIT_NAME            "iowait"
#define ITEM_STATUS_NAME            "status"
#define ITEM_FREEPAGES_NAME         "freepages"
#define ITEM_TOKEN_LEVEL_NAME       "token_level"

#define ITEM_TDISK_NAME             "tdisk"
#define ITEM_FDISK_NAME             "fdisk"

#define ITEM_BCOLOR_NAME            "basic_color"
#define ITEM_CCOLOR_NAME            "current_color"
#define ITEM_CID_NAME               "cluster_id"

#define ITEM_LOCALS_PROC_NAME       "locals"
#define ITEM_GUESTS_PROC_NAME       "guests"
#define ITEM_MAXGUESTS_NAME         "maxguests"
#define ITEM_MIN_GUEST_DIST_NAME    "min_guest_dist"
#define ITEM_MAX_GUEST_DIST_NAME    "max_guest_dist"
#define ITEM_MAX_MIG_DIST_NAME      "max_mig_dist"
#define ITEM_MOSIX_VERSION_NAME     "mosix_version"
#define ITEM_MIN_RUN_DIST_NAME      "min_run_dist"
#define ITEM_MOS_PROCS_NAME         "mos_procs"
#define ITEM_OWNERD_STATUS_NAME     "ownerd_status"

#define ITEM_TOTAL_FREEZE_NAME      "total_freeze"
#define ITEM_FREE_FREEZE_NAME       "free_freeze"

#define ITEM_MOSIX_INFO_NAME        "mosix-info"
#define ITEM_FREEZE_INFO_NAME       "freeze-info"
#define ITEM_USEDBY_NAME            "usedby"
#define ITEM_PROC_WATCH_NAME        "proc-watch"
#define ITEM_CID_CRC_NAME           "cid-crc"
#define ITEM_EXTERNAL_NAME          "external"
#define ITEM_INFOD_DEBUG_NAME       "infod-debug"
#define ITEM_ECONOMY_STAT_NAME      "economy-status"
#define ITEM_JMIGRATE_NAME          "jmigrate"

#define ITEM_NET_WATCH_NAME         "net-watch"
#define ITEM_PROVIDER_TYPE_NAME     "provider-type"
#define ITEM_TOP_USAGE_NAME         "top-usage"
#define ITEM_KERNEL_RELEASE_NAME    "kernel-version"

/****************************************************************************
 * All the information items that can be obtained from the retrieved data.
 * We try to keep a correct view of all the possible fields for comfort
 ***************************************************************************/
#define INFO_MACHINE_NAME_SZ  (24)

typedef struct __all_info_items
{
        char name[ INFO_MACHINE_NAME_SZ ];   // machine name;
	
	unsigned long tmem;     // Total phisical memory
        unsigned long tswap;    // Total swap space
        unsigned long fswap;    // Free swap space
	unsigned long tdisk;    // Total disk space
	unsigned long fdisk;    // Free disk space
        unsigned long uptime;   // The uptime of the node

	unsigned int disk_read_rate;
	unsigned int disk_write_rate;
	unsigned int net_rx_rate;
	unsigned int net_tx_rate;
	
	unsigned long   pe;
	int             infod_status;
        unsigned long   speed;
        unsigned long   iospeed;
        unsigned short  ntopology;
        unsigned short  ncpus;
        unsigned long   load;
        unsigned long   export_load;
	unsigned short  frozen;
	unsigned short  priority;
        unsigned short  util;
        unsigned short  status;
	unsigned short  token_level;
        unsigned long   freepages;

	// Grid related stuff
	unsigned int locals;
	unsigned int guests;
	unsigned int maxguests;
	unsigned int kernel_version;
	int          min_guest_dist;
	int          max_guest_dist;
	int          max_mig_dist;
	int          min_run_dist;
	int          mos_procs;
	int          ownerd_status;
	unsigned long nfs_client_rpc_rate;

        // Pointer to external data
        int           external_status;
        char         *external;
        char         *usedby;
        char         *proc_watch;
        char         *freeze_info;
        char         *cluster_id;
        char         *infod_debug;
} all_info_items_t;


#endif
