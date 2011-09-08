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
 * Author(s): Amar Lior, Peer Ilan
 *
 *****************************************************************************/

/******************************************************************************
 * File: mosix_funcs.c. Implementation of the mosix1 specific functions
 *****************************************************************************/

#ifndef _MOSIX_FUNCS_H
#define _MOSIX_FUNCS_H

#include <msx_common.h>
// Time between updates of less important information in milli seconds
#define   MOSIX_UPDATE_INFO_SLOW     (5000)
#define   MOSIX_UPDATE_INFO_FAST     (1000)

// For the external provider
/* constants for status report */

#define LOAD_MAGIC              0x94979184
#define REQUEST_MAGIC           0x874bb4d3
#define WHOTAKES_MAGIC          0x22e1724f
#define CONSTCNG_MAGIC          0xb9195067
#define SPEEDCNG_MAGIC          0xccc1532d
#define FREQUENCY_MAGIC         0xb47f1284
#define MAP_MAGIC               0xa28c3332

typedef struct frequency_request {
        unsigned long magic;
        int milli;      /* 500 */
        int ninfo;      /* 1 */
        int max_age;    /* 4 */
} frequency_request_t;

struct costcng_msg {
	unsigned long magic;
	unsigned long ntopology;
	unsigned long costs[0];
};

typedef struct whotakes_request {
	unsigned long magic;
	unsigned long queryid;  /* to be returned in reply */
	int home;               /* of process in question */
	int owner;              /* that MOSIXD believes about 'home' */
	int nreplies;           /* to this query */
} whotakes_request_t;

struct mosix_data {
     unsigned long  freepages;
     unsigned long  totpages;
     unsigned int   load;
     unsigned int   speed;
     int            ignore_1;
     unsigned short status;
     unsigned short level;
     unsigned short frozen;
     unsigned char  util;     /* 0 < % < 100 */
     unsigned char  ncpus;
     unsigned short nproc;
     unsigned short token_level;
     unsigned long  edgepages;
     unsigned long availpages;
     unsigned short batches;
     unsigned short ignore_2;
     unsigned short totswap;
     unsigned short freeswap;
#if __WORDSIZE == 64
     int pad[0];
#else
     int pad[4];
#endif  
};

struct mosix_data_64bit 
{
        unsigned long long freepages;     
        unsigned long long totpages;      
        unsigned long int load;           
        unsigned int speed;             
        int PRIVATE_TO_MOSIX_1;
        unsigned short status;          
        unsigned short level;           
        unsigned short frozen;          
        unsigned char utilization;      
        unsigned char ncpus;            
        unsigned short nproc;           
        unsigned short tokenlevel;      
        unsigned long long PRIVATE_TO_MOSIX_2;
        unsigned long long PRIVATE_TO_MOSIX_3;
        int PRIVATE_TO_MOSIX_4;
        unsigned short totswap;         
        unsigned short freeswap;        
};

struct mosix_data_2_28_1 {
  unsigned long long freepages;   
  unsigned long long totpages;    
  unsigned long long INTERNAL_1;
  unsigned long long INTERNAL_2;
  unsigned int load;              
  unsigned int speed;             
  int INTERNAL_3;
  unsigned int status;            
  unsigned short level;           
  unsigned short frozen;          
  unsigned char util;      
  unsigned char ncpus;     
  unsigned short token_level;     
  unsigned short nproc;           
  unsigned short batches;         
  unsigned short totswap;         
  unsigned short freeswap;        
  unsigned int total_gpus;        
  unsigned int avail_gpus;        
  unsigned int total_cpus;        
  unsigned int avail_cpus;        
  char reserved[48];
};
  

/* additional information added by the kcomm module */ 
struct mosix_extra_data {
	unsigned long tmem;     // Total phisical memory
	unsigned long tswap;    // Total swap space
	unsigned long fswap;    // Free swap space
	unsigned long uptime;   // The uptime of the node

	unsigned int disk_read_rate;
	unsigned int disk_write_rate;
	unsigned int net_rx_rate;
	unsigned int net_tx_rate;

	unsigned long tdisk;  // Total disk size in MEM_UNITS
	unsigned long fdisk;  // Free (avail) disk size in MEM_UNITS
	
	int           locals;
	int           guests;
	int           maxguests;
	unsigned int  kernel_version;
        unsigned int  total_freeze; // Total freeze in MB
        unsigned int  free_freeze;  // Free freeze in MB
        
        // Currently unused fields 
        char          machine_arch; // will be 32 or 64 depending on the machine
        char          io_wait_percent; // Percent of time spent in IO wait
        short         unused_2;

        int           unused_3;
	int           unused_4;
	int           unused_5;


	unsigned long rpc_ops;

};

/* the information passed to the infod */ 
struct mosix_infod_data {
	struct mosix_extra_data  extra;
	struct mosix_data        data;
        char                     external[0];
}; 

//struct mosix2_infod_data {
//	struct mosix_extra_data  extra;
//	struct mosix2_data        data;
//}; 


struct mosix_infomsg {
	unsigned long            magic;
	struct mosix_data        data;
};
 
#define   MOSIX_DATA_SZ         (sizeof(struct mosix_data))
#define   MOSIX_EDATA_SZ        (sizeof(struct mosix_extra_data))
#define   MOSIX_IDATA_SZ        (sizeof(struct mosix_infod_data))
#define   MOSIX_INFOD_DATA_SIZE (sizeof(struct mosix_infod_data))
#define   MOSIX_MSG_SZ          (sizeof(struct mosix_infomsg))

typedef struct __mosmap_entry
{
        unsigned int ip;
        unsigned short n;
        short in_cluster;
} mosmap_entry_t;


#endif

/****************************************************************************
 *                               E O F
 ***************************************************************************/
