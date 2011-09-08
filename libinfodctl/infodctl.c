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


/*****************************************************************************
 *    File: infodctl.c
 *****************************************************************************/
//#include <linux/socket.h>
#define __USE_GNU
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>


#include <infodctl.h>

/****************************************************************************
 * Static functions
 ***************************************************************************/
static int
connect_infod( char *infod_name, int infod_namelen ) {

	int sock;
	struct sockaddr_un addr;
  
	if ( ( sock = socket(PF_UNIX, SOCK_STREAM, 0)) == -1 ){
		//perror( "Error socket" );
		return -1;
	}
	
	addr.sun_family = AF_UNIX;
	strncpy( addr.sun_path, infod_name, infod_namelen+1 );
    
	if( ( connect( sock, (struct sockaddr*)&addr,
		     sizeof( struct sockaddr_un))) == -1 ) {
		//perror("Error connect\n");
		return -1;
	}
	return sock;
}

static void
prepare_cred_msg( struct msghdr *msg ) {

	struct cmsghdr *cmsg;
	struct ucred   my_cred; 
	struct ucred   *cred_ptr;
   
	my_cred.pid = getpid();
	my_cred.uid = getuid();
	my_cred.gid = getgid();
   
	cmsg = CMSG_FIRSTHDR( msg );
	cmsg->cmsg_level  = SOL_SOCKET;
	cmsg->cmsg_type   = SCM_CREDENTIALS;
	cmsg->cmsg_len    = CMSG_LEN(sizeof(struct ucred));
	
	/* Initialize the payload: */
	cred_ptr = (struct ucred*)CMSG_DATA(cmsg);
	memcpy( cred_ptr, &my_cred, sizeof(struct ucred));
	msg->msg_controllen = cmsg->cmsg_len;
}

/****************************************************************************
 * Send a control message to the infod
 ***************************************************************************/
int
infod_ctl( int s, infod_ctl_hdr_t *ctl_msg ) {

	int sock, res = -1;
	infod_ctl_msg_hdr_t infod_hdr;
	struct iovec vec[1];
	struct msghdr msg = {0};
	char *buf[CMSG_SPACE(sizeof(struct ucred))];
	
	msg.msg_control = buf;
	msg.msg_controllen = sizeof buf;
    
	if( !ctl_msg )
		return res;

	// if we got a name of infod's socket connect to that name
	// without using the given socke (s)
	sock = s;
	if( ctl_msg->ctl_name ) {
		if(( sock = connect_infod( ctl_msg->ctl_name,
					   ctl_msg->ctl_namelen)) == -1 )
			return res;
	}
	
	else if( s <= 0 ) {
		fprintf( stderr, "Error: incalid socket descriptor\n");
		return res;
	}

	/* Prepare the message credentials */ 
	prepare_cred_msg( &msg );
    
	/* Add the data to the message */ 
	infod_hdr.msg_type    = ctl_msg->ctl_type;
	infod_hdr.msg_datalen = ctl_msg->ctl_datalen;
	infod_hdr.msg_arg1    = ctl_msg->ctl_arg1;
	infod_hdr.msg_arg2    = ctl_msg->ctl_arg2;
	
	vec[0].iov_len  = sizeof(infod_ctl_msg_hdr_t) + ctl_msg->ctl_datalen;
	if( !( vec[0].iov_base = malloc(vec[0].iov_len))) {
		perror("Error malloc\n");
		return res;
	}
	
	memcpy( vec[0].iov_base, &infod_hdr, sizeof(infod_ctl_msg_hdr_t));
	if( ctl_msg->ctl_datalen > 0 )
		memcpy(((char*)(vec[0].iov_base))+sizeof(infod_ctl_msg_hdr_t),
		       ctl_msg->ctl_data, ctl_msg->ctl_datalen);

	msg.msg_iov     = vec;
	msg.msg_iovlen  = 1;
	msg.msg_name    = NULL;
	msg.msg_namelen = 0;
	msg.msg_flags   = 0;
    
	if( sendmsg( sock, &msg, 0 ) == -1) {
		//perror( "Error sendmsg" );
		free( vec[0].iov_base );
		return res;
	}
	
	free(vec[0].iov_base);
    
	/* get the reply for the control action */ 
	msg.msg_name       = NULL;
	msg.msg_namelen    = 0;
	msg.msg_control    = NULL;
	msg.msg_controllen = 0;

	// If we expect a long answer we give the buffer 
	if(ctl_msg->ctl_reslen > 0) {
		vec[0].iov_base = ctl_msg->ctl_res;
		vec[0].iov_len  = ctl_msg->ctl_reslen;
	}
	
	// else we expect only an int
	else {
		vec[0].iov_base = buf;
		vec[0].iov_len= sizeof(int);
	}
	
	msg.msg_iov = vec;
	msg.msg_iovlen = 1;
        
	if( recvmsg( sock, &msg, MSG_NOSIGNAL) < sizeof(int)) {
		//perror( "Error in recvmsg" );
		return res;
	}

	// if we got something from infod then we return 1 if there
	// is something ready in the ctl_res buffer otherwise we
	// expect atleast one int as the result of the action.
	else {
		if(ctl_msg->ctl_reslen > 0)
			res = 1;
		else 
			res = *(int *)(msg.msg_iov[0].iov_base);
	}

	close(sock);
	return res;
}

/****************************************************************************
 *                              E O F
 ***************************************************************************/



