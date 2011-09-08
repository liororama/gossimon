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

/******************************************************************************
 * File: ctl.c. implementation of the ctl subsystem
 *****************************************************************************/
#ifndef __USE_GNU
#define __USE_GNU
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>


#include <msx_error.h>
#include <msx_debug.h>
#include <infod.h>
#include <ctl.h>

/****************************************************************************
 * The status of the control channel 
 ***************************************************************************/

static enum ctl_status {
	CTL_UN_INITILIZED,   // Not initilized yet
	CTL_WAIT_FOR_NEW,    // Waiting for a new connection
	CTL_WAIT_FOR_REQ,    // Connected  and waiting for the request
	CTL_WAIT_FOR_RPL,    // Wait until the request is served and replied
} ctl_status = CTL_UN_INITILIZED;


static int  ctl_sockfd = -1, ctl_curr_sockfd = -1;
static char ctl_sockname[MAX_INFODCTL_SOCK_NAME];

#define CTL_BUFF_SZ (1024)
static char ctl_buff[CTL_BUFF_SZ];
static char data_buff[CTL_BUFF_SZ];

/****************************************************************************
 * Interface functions 
 ***************************************************************************/

/****************************************************************************
 * Initialize the control socket
 ***************************************************************************/
int
infodctl_init( char *infod_sock_name ) {
	struct sockaddr_un addr;
    
	strncpy( ctl_sockname, infod_sock_name, MAX_INFODCTL_SOCK_NAME );
        
	if( ( ctl_sockfd = socket( PF_UNIX, SOCK_STREAM, 0)) == -1 ){
		debug_lg( CTL_DEBUG, "Error: opening socket: %s", strerror(errno) );
                syslog(LOG_ERR, "Error creating control channel: %s\n",
                       strerror(errno));
                goto failed;
	}

	addr.sun_family = AF_UNIX;
    	strcpy( addr.sun_path, infod_sock_name );
	
	if( (bind( ctl_sockfd, (struct sockaddr*)&addr,
		  sizeof( struct sockaddr_un ))) == -1 ) {
		debug_lr( CTL_DEBUG, "Error: bind\n%s\n",
			  strerror(errno));
                syslog(LOG_ERR, "Error binding to control channel: %s\n",
                       strerror(errno));
		goto failed;
	}
        
	if( (listen( ctl_sockfd, 5)) == -1) {
		debug_lr( CTL_DEBUG, "Error: listen\n%s\n",
			  strerror(errno));
                syslog(LOG_ERR, "Error listening to control channel: %s\n",
                       strerror(errno));
		unlink(ctl_sockname);
		goto failed;
	}

        syslog(LOG_INFO, "Control channel socket %d\n", ctl_sockfd );
	ctl_status = CTL_WAIT_FOR_NEW;
	return 1;
    
 failed:
	close(ctl_sockfd);
	ctl_sockfd = -1;
	ctl_curr_sockfd = -1;
	ctl_status = CTL_UN_INITILIZED;
    	return 0 ;
}

/****************************************************************************
 * Stop/Reset the control sub system
 ***************************************************************************/
int
infodctl_stop() {

	close(ctl_sockfd);
	close(ctl_curr_sockfd);
	ctl_sockfd = -1;
	ctl_curr_sockfd = -1;
	ctl_status = CTL_UN_INITILIZED;
    
	if( unlink( ctl_sockname ) == -1)
		return 0;
	return 1;
}

int
infodctl_reset() {
	close( ctl_curr_sockfd );
	ctl_curr_sockfd = -1;
	ctl_status = CTL_WAIT_FOR_NEW;
	return 1;
}

/*****************************************************************************
 * Add a socket to the rfds.
 * If CTL_WAIT_FOR_NEW then insert the ctl_sockfd
 * if CTL_WAIT_FOR_REQ then insert the ctl_curr_sockfd 
 * If CTL_WAIT_FOR_RPL | CTL_UN_INITILIZED insert nothing
 *
 * Return value: the number of sockfs set in on the rfds
 *****************************************************************************/
int
infodctl_get_fdset( fd_set *rfds ) {
	int res = 0;
	if( ctl_status == CTL_WAIT_FOR_NEW ) {
		FD_SET( ctl_sockfd, rfds );
		res = 1;
	}
	
	else if( ctl_status == CTL_WAIT_FOR_REQ ) {
		FD_SET(ctl_curr_sockfd, rfds);
		res = 1;
	}
	return res;
}

/*****************************************************************************
 * Returning the maximum sockfd we are currently working with
 *****************************************************************************/
int
infodctl_get_max_sock() {
	switch( ctl_status ) {
	    case CTL_WAIT_FOR_NEW:
		    return ctl_sockfd;
	    case CTL_WAIT_FOR_REQ:
		    return ctl_curr_sockfd;
	    default:
		    return -1;
	}
	return -1;
}

/*****************************************************************************
 * Checking if ctl_sockfd is on the rfds we got from the select. If true,
 * an accept() can be called on the socket
*****************************************************************************/
int
infodctl_is_new_connection( fd_set *rfds ) {
	if( ctl_status == CTL_WAIT_FOR_NEW &&
	    FD_ISSET( ctl_sockfd, rfds ) )
		return 1;
	return 0;
}
 
/****************************************************************************
 * Call accept on the main socket and move to CTL_WAIT_FOR_REQ state
 ****************************************************************************/
int
infodctl_setup_connection() {
	int new_sock;
	int conn_addr_len = sizeof(struct sockaddr_un);
	struct sockaddr_un conn_addr;
    
	if( ( new_sock = accept( ctl_sockfd, (struct sockaddr*)&conn_addr,
				 (socklen_t*) &conn_addr_len)) == -1 ){
		debug_lr( CTL_DEBUG, "Error: accept \n%s\n",
			  strerror(errno));
		return 0;
	}
    
	ctl_status = CTL_WAIT_FOR_REQ;
	ctl_curr_sockfd = new_sock;
	return 1;
}

/****************************************************************************
 * Checking if the ctl_curr_sockfd is in rfds returend from the select.
 * If true, we are ready to get the ctl request.
 ****************************************************************************/
int
infodctl_req_arrived( fd_set *rfds ) {
	if( ctl_status == CTL_WAIT_FOR_REQ &&
	    FD_ISSET( ctl_curr_sockfd, rfds ))
		return 1;
	return 0;

}

/*****************************************************************************
 * Read the message and we check that we:
 * 1) get cradentials
 * 2) got enough data so we have a valid header
 * 3) got enough data so that the datalen field is correct
 *
 * We fill the ctl_action struct with the request
 *****************************************************************************/
int
infodctl_recv( infodctl_action_t *ctl_action ) {
	int recv_len, flag = 1;
	
	struct msghdr msgh;
	struct cmsghdr *cmsg;
	struct ucred *cred_ptr;
	struct ucred received_cred;
	struct iovec vec[1];
	infod_ctl_msg_hdr_t *ctl_hdr;
    
	msgh.msg_name       = NULL;
	msgh.msg_namelen    = 0;
	msgh.msg_control    = ctl_buff;
	msgh.msg_controllen = CTL_BUFF_SZ;
    
	vec[0].iov_base = data_buff;
	vec[0].iov_len  = CTL_BUFF_SZ;
    
	msgh.msg_iov = vec;
	msgh.msg_iovlen = 1;

	// enabling receiving of the credentials of the sending
	// process ancillary data
	setsockopt( ctl_curr_sockfd, SOL_SOCKET, SO_PASSCRED,
		    &flag, sizeof(int));
    
	if( (recv_len = recvmsg( ctl_curr_sockfd, &msgh, MSG_NOSIGNAL)) <= 0){
		debug_lr( CTL_DEBUG, "Error: recv credentials\n" );
		goto failed;
	}
        
	/* Receive auxiliary data in msgh */
	for( cmsg = CMSG_FIRSTHDR(&msgh);
	     cmsg != NULL;
	     cmsg = CMSG_NXTHDR(&msgh,cmsg)) {
		if ( cmsg->cmsg_level == SOL_SOCKET
		     && cmsg->cmsg_type == SCM_CREDENTIALS ) {
			cred_ptr = (struct ucred*) CMSG_DATA(cmsg);
			received_cred = *cred_ptr;
			break;
		}
	}
	
	if( cmsg == NULL ) {
		debug_lr( CTL_DEBUG, "Error: did not get credentials\n" );
		goto failed;
	}
	
	else {
		debug_lg( CTL_DEBUG, "CTL Cred:  (pid %d) (uid %d) (gid %d)\n",
			  received_cred.pid,
			  received_cred.uid,
			  received_cred.gid); 
	}
    
	if( recv_len < sizeof(infod_ctl_msg_hdr_t)) {
		debug_lr( CTL_DEBUG, "CTL: recv_len (%d) < header len (%d)\n",
			  recv_len, sizeof(infod_ctl_msg_hdr_t)); 
		goto failed;
	}
	
	ctl_hdr = (infod_ctl_msg_hdr_t*)(msgh.msg_iov[0].iov_base);
	
	debug_lg( CTL_DEBUG, "CTL: got %d bytes, type is %d\n",
		  recv_len, ctl_hdr->msg_type);
    
	if( ctl_hdr->msg_datalen > (recv_len - sizeof(infod_ctl_msg_hdr_t))) {
		debug_lr( CTL_DEBUG, "datalen is bigger that whats left "
			  "(%d) (%d)\n", ctl_hdr->msg_datalen,
			  (recv_len - sizeof(infod_ctl_msg_hdr_t)));
		goto failed;
	}

	// Packing the action
	ctl_action->action_type = ctl_hdr->msg_type;
	ctl_action->action_arg1 = ctl_hdr->msg_arg1;
	ctl_action->action_arg2 = ctl_hdr->msg_arg2;
	ctl_action->action_cred = received_cred;
	
	// If we have data we give a pointer to the data from our static buffer
	// This means that we can handle more than one ctl msg at a time.
	if( (ctl_action->action_datalen = ctl_hdr->msg_datalen))
		ctl_action->action_data =
			msgh.msg_iov[0].iov_base + sizeof(infod_ctl_msg_hdr_t);

	ctl_status = CTL_WAIT_FOR_RPL;
    	return 1;
    
 failed:
	infodctl_reset();
	return 0;
}


/****************************************************************************
 * Return a reply
 ***************************************************************************/
int
infodctl_reply( char *buff, int len ) {
	send( ctl_curr_sockfd, buff, len, MSG_NOSIGNAL );
	infodctl_reset();
	return 1;
}   

/****************************************************************************
 *                            E O F 
 ***************************************************************************/
