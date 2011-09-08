/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


/*****************************************************************************
 *
 * Author(s): Amar Lior, Peer Ilan
 *
 *****************************************************************************/

 /*****************************************************************************
 *    File: info.h. General definitions for the info daemon
 *****************************************************************************/

#ifndef _INFO_H
#define _INFO_H

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

/****************************************************************************
 * Constants and macros 
 ***************************************************************************/

#define INFOD_INTERNAL_PROVIDER_MODE  "INTERNAL_PROVIDER"

#define MSX_INFOD_INFO_VER            (2.0)
#define DEF_TIMEOUT                   (5)

// The usual ports 
#ifndef INFOD2
#define MSX_INFOD_DEF_PORT            (8000)
#define MSX_INFOD_LIB_DEF_PORT        (12000)
#define MSX_INFOD_GINFOD_DEF_PORT     (16000)

// In case of INFOD2 defined (for measurments)
#else 
#define MSX_INFOD_DEF_PORT            (8001)
#define MSX_INFOD_LIB_DEF_PORT        (12001)
#define MSX_INFOD_GINFOD_DEF_PORT     (16001)

#endif

/*
 * define message types
 */
#define INFOD_MSG_TYPE_INFO      (0x01)
#define INFOD_MSG_TYPE_INFOLIB   (0x02)
#define INFOD_MSG_TYPE_GINFOD    (0x04)
#define INFOD_MSG_TYPE_GLOBAL    (0x08)
#define MSG_TYPE_STR             (0x10)
#define INFOD_MSG_TYPE_INFO_PULL (0x20)

#define  XML_ROOT_TAG        "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
#define  MEM_UNIT_SIZE       (4096)
#define  MACHINE_NAME_SZ     (24)

#define COMM_IP_VER          (4)
#define MILLI                (1000000)

/****************************************************************************
 * The data structure that hold node information
 ***************************************************************************/

// Changing the following requires recompilation of all the clients.
typedef int node_t ;


// Infod status and death cause
#define  INFOD_ALIVE           0x001
#define  INFOD_DEAD_INIT       0x002
#define  INFOD_DEAD_AGE        0x004
#define  INFOD_DEAD_CONNECT    0x008
#define  INFOD_DEAD_PROVIDER   0x010
#define  INFOD_DEAD_VEC_RESET  0x020

#define  INFOD_ALIVE_STR            "alive"
#define  INFOD_DEAD_INIT_STR        "init"
#define  INFOD_DEAD_AGE_STR         "age"
#define  INFOD_DEAD_CONNECT_STR     "connect"
#define  INFOD_DEAD_PROVIDER_STR    "provider"
#define  INFOD_DEAD_VEC_RESET_STR   "reset"
#define  INFOD_DEAD_UNKNOWN_STR     "unknown"

char *infoStatusToStr(int status);




/**************************************
 * The pull information message format
 **************************************/ 
typedef struct info_pull_msg {
	int         param;
} info_pull_msg_t;


/**************************************
 * Client requests messages
 **************************************/ 
#include <netinet/in.h>

typedef struct node_hdr {
	
	unsigned int    pe;
	char            ip[COMM_IP_VER];
	struct in_addr  IP;         // The IP here should allwyas be in network order
	unsigned int    status;     // Indicating the node status
	unsigned int    cause;
	unsigned int    param;
	unsigned int    psize;      // size of partial information
	unsigned int    fsize;      // size of full information
	struct timeval  time;
        int             external_status; // Status of external info      
        
} node_hdr_t;


typedef struct node_info {
	
	node_hdr_t      hdr;
	char            data[0];
	
} node_info_t;

typedef struct infod_stats {
	
	unsigned int     total_num;     /* Number of nodes in the cluster  */
	unsigned int     num_alive;     /* number of alive nodes           */
	unsigned int     ncpus;         /* number of cpus                  */
	unsigned int     tmem;          /* total amount of available mem   */
	unsigned int     sspeed;        /* standard machine speed          */
	double           avgload;        /* the maximal age                 */
	double           avgage;        /* the average age of the info     */
	double           maxage;
	double           unused[5];
} infod_stats_t;

#define      NODE_SZ       (sizeof(node_t))
#define      NHDR_SZ       (sizeof(node_hdr_t))
#define      NODE_HEADER_SIZE (sizeof(node_hdr_t)) 
#define      NINFO_SZ        (sizeof(node_info_t))
#define      NODE_INFO_SIZE  (sizeof(node_info_t))
#define      STATS_SZ      (sizeof(infod_stats_t))


#define     IS_VLEN_INFO(n)   (((n)->hdr.fsize - (n)->hdr.psize) > 0)

#include <stdio.h> 
int    add_vlen_info(node_info_t *ninfo, char *data_name, char *data, int size);
void  *get_vlen_info(node_info_t *ninfo, char *info_name, int *size);
void   print_vlen_items(FILE *fh, node_info_t *ninfo);




// Given a node_info_t we find if we have external information
//#define      EXTERNAL_INFO_SIZE(n)   ((n)->hdr.fsize - (n)->hdr.psize)

// The value of the external_status field, if the information is valid
// then the external status > 0 and the value represent the age in seconds
// of the information relative to the age of the information packet.
#define      EXTERNAL_STAT_IS_TIME(s)  ((s)>=0)

// The maximal amount of time in which external information should be
// considered valid. If the external information was not update (the
// mtime of the file) for 60 seconds, the information is considered to be
// stalled 
#define      EXTERNAL_INFO_STALLED(s)  ((s)>60)

#define      EXTERNAL_STAT_INFO_OK     (-1)
#define      EXTERNAL_STAT_NO_INFO     (-2)
#define      EXTERNAL_STAT_EMPTY       (-3)
#define      EXTERNAL_STAT_ERROR       (-4)

#define      EXTERNAL_STAT_INFO_OK_STR    "info ok"
#define      EXTERNAL_STAT_NO_INFO_STR    "no info"
#define      EXTERNAL_STAT_EMPTY_STR      "empty"   
#define      EXTERNAL_STAT_ERROR_STR      "error"

char *infoExternalStatusToStr(int status);

/*****************************************************************************
 *  Communication between the infod and the infolib
 *****************************************************************************/
typedef enum infolib_req {
	INFOLIB_ALL = 0,              /* no args */
	INFOLIB_CONT_PES,             /* args: ( node_t start, int size) */ 
	INFOLIB_SUBST_PES,            /* args: ( int size, node_t *pes ) */
	INFOLIB_NAMES,                /* args: ( int size, char *names_str ) */
	INFOLIB_AGE,                  /* args: ( struct timeval ) */
	INFOLIB_GET_NAMES,            /* no args */
	INFOLIB_STATS,                /* no args */
	INFOLIB_DESC,                 /* no args */ 
	INFOLIB_WINDOW,               /* get infod window */
	INFOLIB_MAX_REQ
} infolib_req_t;

typedef struct _infolib_msg {
	int  version;
	infolib_req_t  request;
	char args[0];
} infolib_msg_t;

/* The args variable in 'infolib_msg_t' may char* or one of the following */ 
typedef struct cont_pes_args {
	node_t pe;
	int size;
} cont_pes_args_t ;

typedef struct subst_pes_args {
	int size;
	node_t pes[0];
} subst_pes_args_t ;

/*
 * The reply sent to the client
 */
typedef struct idata_entry {

	int         valid, size;
	char        name[MACHINE_NAME_SZ];
	node_info_t data[0];
	
} idata_entry_t;

typedef struct idata {
	
	int           num, total_sz;
	idata_entry_t data[0];
	
} idata_t; 

typedef idata_entry_t info_replay_entry_t;
typedef idata_t info_replay_t;

#define IDATA_ENTRY_SZ             (sizeof(idata_entry_t))
#define INFO_REPLAY_ENTRY_SIZE     (sizeof(idata_entry_t))

#define IDATA_SZ                   (sizeof(idata_t))
#define INFO_REPLAY_SIZE           (sizeof(idata_t))

/*
 * Information obtained from the gridd
 */
typedef struct ginfod_msg {
	
	int num, entry_sz, total_sz;
	char data[0];
	
} ginfod_msg_t;

#define GINFOD_MSG_SZ (sizeof(ginfod_msg_t))

#include <infoitems.h>
#endif

/******************************************************************************
 *                             E O F
 *****************************************************************************/
