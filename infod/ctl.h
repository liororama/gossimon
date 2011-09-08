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
 * Author(s): Peer Ilan, Amar Lior
 *
 *****************************************************************************/

/******************************************************************************
 * File: ctl.h. definitions of the ctl subsystem
 *****************************************************************************/

#ifndef _MOSIX_INFODCTL
#define _MOSIX_INFODCTL

#ifndef __USE_GNU
#define __USE_GNU
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include <infodctl.h>
#include <sys/socket.h>

#define MAX_INFODCTL_SOCK_NAME  (128)

/****************************************************************************
 * Holds valid information after parsing a message
 ***************************************************************************/
typedef struct infodctl_action {
	infod_ctl_t      action_type;
	struct ucred     action_cred;
	int              action_arg1;
	int              action_arg2;
	void            *action_data;
	int              action_datalen;
} infodctl_action_t;

/****************************************************************************
 * Interface functions 
 ***************************************************************************/

/* Initilizing the ctl unix socket */ 
int infodctl_init( char *infod_sock_name );
int infodctl_stop();
int infodctl_reset();

/* Add the relevant file descriptors to the read fd's of select() */
int infodctl_get_fdset( fd_set *rfds );
int infodctl_get_max_sock();

/* Determines if one of the ready fd's is of the contorl channel  */
int infodctl_is_new_connection( fd_set *rfds );
int infodctl_req_arrived( fd_set *rfds );
int infodctl_setup_connection();

/*
 * Get a message and parse it.Return the action associated with the
 * message and the action args. Valid only if a successful
 * infodctl_setup_connection() was performed.
 * Note that the credetial are returned as part of infodctl_action_t
 * a should be verified by the calling process.
 */
int infodctl_recv( infodctl_action_t *ctl_action );

/* send a reply on the control channel */ 
int infodctl_reply( char *buff, int len );

#endif /* _MOSIX_INFODCTL */

/****************************************************************************
 *                                E O F 
 ***************************************************************************/
