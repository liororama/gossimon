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
 * Author(s): Peer Ilan, Amar Lior
 *
 *****************************************************************************/

/*****************************************************************************
 *    File: infodctl.h
 *****************************************************************************/

#ifndef _INFODCTL_H
#define _INFODCTL_H

#ifndef INFOD2
#define MSX_INFOD_CTL_SOCK_NAME       "/var/..ictl_"
#else
#define MSX_INFOD_CTL_SOCK_NAME       "/var/..ictl2_"
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <linux/socket.h>

typedef enum _infod_ctl_types {
    INFOD_CTL_QUIET,
    INFOD_CTL_NOQUIET,
    INFOD_CTL_SET_YARD,
    INFOD_CTL_GET_YARD,
    INFOD_CTL_SETPE,
    INFOD_CTL_SETPE_STOP,
    INFOD_CTL_SET_WINDOW_SIZE,
    INFOD_CTL_SET_TIME_PERIOD,
    INFOD_CTL_SET_DEBUG,
    INFOD_CTL_GET_STATUS,
    INFOD_CTL_SET_SPEED,
    INFOD_CTL_SET_SSPEED,
    INFOD_CTL_CONST_CHANGE,
    INFOD_CTL_MEASURE_AVG_AGE,
    INFOD_CTL_WIN_TYPE,
    INFOD_CTL_WIN_SIZE_MEASURE,
    INFOD_CTL_MEASURE_INFO_MSGS,
    INFOD_CTL_MEASURE_SEND_TIME,
    INFOD_CTL_GOSSIP_ALGO,
    INFOD_CTL_DEATH_LOG_CLEAR,
    INFOD_CTL_DEATH_LOG_GET,
    INFOD_CTL_MEASURE_UPTOAGE,
    
} infod_ctl_t;

#define MAX_INFOD_SOCK_NAME (128)

typedef struct _infod_ctl_hdr {
	char          *ctl_name;   // optional name in case socket is not connected
	int           ctl_namelen; // length of name
	infod_ctl_t   ctl_type;    // type of ctl message
	void          *ctl_data;   // data assosiated with ctl
	int           ctl_datalen; // length of data
	void          *ctl_res;    // buffer to locate results if any
	int           ctl_reslen;  // length of result buffer
	int           ctl_flags;   // some extra flags
	int           ctl_arg1;
	int           ctl_arg2;
} infod_ctl_hdr_t;

// the header of the data that is actually sent for every ctl request.
typedef struct _infod_ctl_msg_hdr {
    infod_ctl_t     msg_type;    // Type of message
    int             msg_arg1;    
    int             msg_arg2;    
    int             msg_datalen;
} infod_ctl_msg_hdr_t;

/* Sending a control message to infod */
int infod_ctl( int s, infod_ctl_hdr_t *msg );

#endif /* _INFODCTL_H */

/***************************************************************************
 *                                E O F
 ****************************************************************************/
