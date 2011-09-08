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

/*
 * Implementing an asyncronous send recive library for information
 * desimmination.
 *
 * This library should be used by the daemons that dessiminate information.
 * Its send function is designed so it does not block (the connect) so if
 * we send a message to a dead node we dont get stuck. Every send and receive
 * has two stages. The first where in send we do an unblocking connect and
 * in the receive we do an unblocking accept. And then we wait until we are
 * ready for the second statge where in the send it is after the connect
 * is done (succesful or not) and in the receive it is when there is data
 * wating for us in the socket.
 *
 * We must handle the recieve asynchorunously too since after an accept
 * we dont get the data immidiatly since the connect is not followed by
 * send but the actuall send is called after the select tells us that
 * the connect went O.K.  
 */

#include <netinet/in.h>
#include <netdb.h>   
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/tcp.h>

#include <info.h>
#include <msx_error.h>
#include <msx_debug.h>
#include <comm.h>

#define MAX_BUFF_SZ (1024)

/******************************************************************************
 * Set the header fields
 *****************************************************************************/
int
comm_hdr_set( comm_hdr_t *hdr, int type, int size ){
	hdr->type = type;
	hdr->size = size;
	return 1;
}

/******************************************************************************
 * Function that add entries to the data structures
 *****************************************************************************/

/*
 * Adding a (socket, data) to the list of inprogress sends
 * Note that the send buffer holds all the info that would
 * be sent including the type and the size of the data
 * buffer 
 */
static int
comm_add_inprogress_send( msx_comm_t *comm, int sock, char *ip,
			  char *buff, int type, int size, int mv2recv ){

	/* we add at comm_inpr_recv_next */
	int pos = comm->comm_inpr_send_next;
	comm_hdr_t hdr;
	
	/* Test that there is enough space left */ 
	if( pos == COMM_MAX_INPROGRESS - 1 ){
		debug_lr( COMM_DEBUG,
			  "Error: Already (%d) connections in progress\n",
			  COMM_MAX_INPROGRESS);
		return 0;
	}

	bzero( &(comm->comm_inpr_send[ pos ]), sizeof(comm_inprogress_send_t));
	
	/* Set the header fields */
	if( !( comm_hdr_set( &hdr, type, size )))
		return 0;
      
	comm->comm_inpr_send[ pos ].data_size = sizeof(comm_hdr_t) + size;

	if(!(comm->comm_inpr_send[ pos ].data =
	     malloc( comm->comm_inpr_send[ pos ].data_size ))){
		debug_lb( COMM_DEBUG,
			  "Error: malloc, comm_add_inprogress_send\n" );
		return 0;
	}

	comm->comm_inpr_send[ pos ].curr_size = 0 ;
	comm->comm_inpr_send[ pos ].sock      = sock;
	comm->comm_inpr_send[ pos ].mv2recv   = mv2recv ;

	/* first copy the header to buff */ 
	memcpy(comm->comm_inpr_send[ pos ].data, &hdr, sizeof(comm_hdr_t));

	/* copy the buffer that holds the info */ 
	memcpy((comm->comm_inpr_send[ pos ].data) + sizeof(comm_hdr_t),
	       buff, size);

	memcpy( comm->comm_inpr_send[ pos ].ip, ip, COMM_IP_VER );
	timerclear( &(comm->comm_inpr_send[ pos ].time) ) ; 
      
	if( comm->comm_maxfd < sock )
		comm->comm_maxfd = sock;

	comm->comm_inpr_send_next++;
	return 1;
}

/*
 * Adding a (sock) to the list of connections that we are doing
 * receive on.
 * Note that the time field is initiated here.
 */
static int
comm_add_inprogress_recv( msx_comm_t *comm, int sock, char *ip ){

	/* we add at comm_inpr_recv_next */ 
	int pos = comm->comm_inpr_recv_next;
      
	if( pos == (COMM_MAX_INPROGRESS - 1)){
		debug_lr( COMM_DEBUG,
			  "Error: Already (%d) connections in progress\n",
			  COMM_MAX_INPROGRESS );
		return 0;
	}

	bzero( &(comm->comm_inpr_recv[ pos ]), sizeof(comm_inprogress_recv_t));
	comm->comm_inpr_recv[ pos ].sock = sock ;
	memcpy(comm->comm_inpr_recv[ pos ].ip, ip, COMM_IP_VER );
	gettimeofday( &(comm->comm_inpr_recv[ pos ].time), NULL );
      
	if( comm->comm_maxfd < sock )
		comm->comm_maxfd = sock;
      
	comm->comm_inpr_recv_next++;
	return 1;
}

/*
 * Adding a socket that should be closed
 */
static int
comm_add_close( msx_comm_t *comm, int sock ){

	int pos = comm->comm_close_next;
	int flag = 0;

	/* Make the socket blocking */ 
	if( ioctl(sock, FIONBIO, &flag) < 0 )
		debug_lr( COMM_DEBUG,  "Error: ioctl\n" );

	/* Send data as soon as possible (disable Nagle's algorihm) */
	flag = 1;
	setsockopt( sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    
	if( pos == ( COMM_MAX_INPROGRESS - 1)){
		debug_lr( COMM_DEBUG, "Error: Too much fd's in close\n");
		close( sock );
		return 0;
	}
    
	bzero( &(comm->comm_close[ pos ]), sizeof(comm_send_close_t));
	comm->comm_close[ pos ].sock = sock ;
	gettimeofday( &(comm->comm_close[ pos ].time), NULL );
      
	if( comm->comm_maxfd < sock )
		comm->comm_maxfd = sock;
	comm->comm_close_next++;
    
	return 1;
}

/****************************************************************************
 * calculating the maximum number file descriptor so we can use it
 * in a select
 ***************************************************************************/
int
comm_calc_inprogress_maxfd( msx_comm_t *comm ){
	int i;
	int maxfd = -1;

	for(i=0 ; i < COMM_MAX_LISTEN ; i++)
		if( comm->comm_sock[i] > maxfd)
			maxfd = comm->comm_sock[i];
    
	for(i=0; i < comm->comm_inpr_send_next ; i++)
		if( comm->comm_inpr_send[i].sock > maxfd)
			maxfd = comm->comm_inpr_send[i].sock;
    
	for(i=0; i < comm->comm_inpr_recv_next ; i++)
		if( comm->comm_inpr_recv[i].sock > maxfd)
			maxfd = comm->comm_inpr_recv[i].sock;
    
	for(i=0; i < comm->comm_close_next ; i++)
		if( comm->comm_close[i].sock > maxfd)
			maxfd = comm->comm_close[i].sock;
    
	comm->comm_maxfd = maxfd;
	return 1;
}

/****************************************************************************
 *  Initializing the msxcomm object
 ***************************************************************************/
msx_comm_t*
comm_init( unsigned short *ports, int timeout ){
	int result = 0 ; 
	int i;
	struct sockaddr_in socketInfo ;
      
	msx_comm_t *comm;
      
	/* allocating memory for comm */
	if (!(comm = malloc(sizeof(msx_comm_t))))
		return NULL;

	/* set all the memory to null */ 
	bzero( comm, sizeof(msx_comm_t));
      
	/* timeout of receiving and sending */
	comm->comm_timeout = timeout;
      
	/* Opening, binding and listening on all the external sockets */
	comm->comm_maxfd = -1;

	for(i=0 ; i < COMM_MAX_LISTEN ; i++)
		comm->comm_sock[i] = -1;
	
	for( i = 0 ; i < COMM_MAX_LISTEN ; i++ ) {
		if(ports[i] <= 0){
			/* this socket is not used */
			comm->comm_sock[i] = -1;
			continue;
		}
	  
		/* setup the sock_addr data structure */
		socketInfo.sin_family = PF_INET;
		socketInfo.sin_port = htons( ports[i] );
		socketInfo.sin_addr.s_addr = htonl( INADDR_ANY );
	  
		/* create the new socket */
		if ( ( comm->comm_sock[i] =
		       socket( PF_INET, SOCK_STREAM, 0 ) ) == -1 )	  {
			debug_lr( COMM_DEBUG, "Error: socket()\n" ) ;
			goto exit_with_error;
		}
	    
		/* bind the socket */
		if ( ( result = bind( comm->comm_sock[i],
				      (struct sockaddr *)&socketInfo,
				      sizeof( struct sockaddr ) ) ) == -1 ){
			debug_lr( COMM_DEBUG, "Error: bind()\n" ) ;
			goto exit_with_error;
		}
	    
		if ( ( listen( comm->comm_sock[i], SOMAXCONN ) ) == -1 ){
			debug_lr( COMM_DEBUG, "Error: listen()\n" ) ;
			goto exit_with_error;
		}
	    
		if(comm->comm_sock[i] > comm->comm_maxfd)
			comm->comm_maxfd = comm->comm_sock[i];
	}
      
	/* Initilizing the inprogress socket set */
	comm->comm_inpr_send_next = 0;
	comm->comm_inpr_recv_next = 0;
	comm->comm_close_next = 0;

	/* set the time for the next admin operation */ 
	gettimeofday( &(comm->next_admin), NULL );
	comm->next_admin.tv_sec += COMM_ADMIN_WAIT;
    
	return comm;

 exit_with_error:
	comm_close(comm);
	return NULL;
} 

/****************************************************************************
 * Closing msx_comm. first closing all sockets and then deallocating memory
 ***************************************************************************/
int
comm_close( msx_comm_t *comm ){

	int i = 0;
      
	if( !comm )
		return -1;

	/* close all the listenning socks */
	for( i = 0; i < COMM_MAX_LISTEN ; i++ )
		if( comm->comm_sock[ i ] > -1 ) 
			close(comm->comm_sock[ i ] );
	
	/* close all the send sockets */
	for( i = 0 ; i < comm->comm_inpr_send_next; i++ ) {
		close( comm->comm_inpr_send[ i ].sock );
		if( comm->comm_inpr_send[ i ].data ) {
			free( comm->comm_inpr_send[ i ].data );
			comm->comm_inpr_send[ i ].data = NULL;
		}
	}
    
	/* close all the recv sockets */
	for( i = 0; i < comm->comm_inpr_recv_next; i++ ) {
		close( comm->comm_inpr_recv[ i ].sock );
		if( comm->comm_inpr_recv[ i ].data ) {
			free( comm->comm_inpr_recv[ i ].data );
			comm->comm_inpr_recv[ i ].data = NULL;
		}
	}
    
	/* close all the sockets waiting to be closed  */
	for( i = 0; i < comm->comm_close_next; i++ )
		close( comm->comm_close[i].sock );
	
	free( comm );
	comm = NULL;
	return 0;
}

/****************************************************************************
 * Check if one if the file descriptors given in *set is one of the fds 
 * we are listening on
 ***************************************************************************/
int
comm_is_new_connection( msx_comm_t *comm, fd_set *set ) {

	int i = 0;
	for( i = 0; i < COMM_MAX_LISTEN; i++ )
		if( (comm->comm_sock[ i ] > 0) &&
		    ( FD_ISSET( comm->comm_sock[ i ], set )))
			return 1;
	return 0;
}

/****************************************************************************
 * Checking if one of the send sockets is ready for send (the connect
 * finished)
 ***************************************************************************/
int
comm_inprogress_send_active( msx_comm_t *comm, fd_set *set) {

	int i = 0;
	for( i = 0; i < comm->comm_inpr_send_next; i++ )
		if( FD_ISSET( comm->comm_inpr_send[ i ].sock, set ))
			return 1;
	return 0;
}

/****************************************************************************
 * Checking if one of the receiving sockets is ready for reading
 ***************************************************************************/
int
comm_inprogress_recv_active( msx_comm_t *comm, fd_set *set) {

	int i = 0;
	for( i = 0; i < comm->comm_inpr_recv_next; i++ )
		if( FD_ISSET( comm->comm_inpr_recv[ i ].sock, set ))
			return 1;
	return 0;
}

/****************************************************************************
 * Checking if one of the sockets waiting to be closed was signald.
 * If such one is found, close it
 ***************************************************************************/
int
comm_close_on_socket( msx_comm_t *comm, fd_set *set ) {

	int i = 0, recalc_max   = 0;

	for( i = 0; i < comm->comm_close_next; i++ ) {
		if( FD_ISSET( comm->comm_close[ i ].sock, set )) {

			if( close( comm->comm_close[ i ].sock ) < 0 )
				debug_lb( COMM_DEBUG, "Error: close()\n" );
			else
				debug_lb( COMM_DEBUG, "Closed socket %d\n",
					  comm->comm_close[ i ].sock );
	    
			if( comm->comm_maxfd == comm->comm_close[ i ].sock)
				recalc_max = 1;
		  
			if( i != comm->comm_close_next - 1 ) {
				comm->comm_close[ i ] = comm->
					comm_close[ comm->comm_close_next - 1];
				bzero( &(comm->comm_close[
						 comm->comm_close_next - 1]),
				       sizeof(comm_send_close_t));
			}
			
			comm->comm_close_next--;
    
			if( recalc_max )
				comm_calc_inprogress_maxfd( comm );
			return 1;
		}
	}
    	return 0;
}

/****************************************************************************
 * Sending information to the given ip. The function tries to establish
 * connection to the other side, and tries to add the connection to the
 * in progress send
 ***************************************************************************/
int
comm_send( msx_comm_t *comm,  char *ip, unsigned short port,
	   void *buff, int type, int size, int mv2recv ) {

	struct sockaddr_in socketInfo;
	int  socketfd = 0 ;
	int  res;
    
	/* should create the connection to another infod */ 
	if(( socketfd = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ){
		debug_lr( COMM_DEBUG, "Error: socket()");
		return -1; 
	}

	/* We turn the socket to a nonblocking */
	fcntl( socketfd, F_SETFL, O_NONBLOCK );

	/* fill in port info */
	socketInfo.sin_port = htons( port ) ;
      
	memcpy( &(socketInfo.sin_addr.s_addr), ip, COMM_IP_VER );
	socketInfo.sin_family = PF_INET;
      
	if(( connect( socketfd, (struct sockaddr*)&socketInfo,
		      sizeof( socketInfo ))) == -1 ){
		if( errno != EINPROGRESS ){
			debug_lr( COMM_DEBUG, "Error: connect()");
			res = -1 ;
			goto exit_with_close;
		}
	}
    
	/*
	 * connect is in progress or connect succeeded, test if there
	 * is enough place for the asynchronous send
	 */
	if( !comm_add_inprogress_send( comm, socketfd, ip, buff,
				       type, size, mv2recv)) {
		res = -1;
		goto exit_with_close;
	}
      
	else{ 
		res = 1;
		goto exit_without_close;
	}
    
 exit_with_close:
	close( socketfd );
	
 exit_without_close:
	return res;
}

/****************************************************************************
 * Finishing a send. Connect is done but we need to check
 * its return value. If it was succsesfull, send the message we
 * hold inside the object.
 * Note that only part of the message might be send. Also note that
 * the message to be sent already contains the header.
 ***************************************************************************/
int
comm_finish_send( msx_comm_t *comm, fd_set *connected_set, char *ip ) {

	int i;
	int res = 0;
	int next = comm->comm_inpr_send_next;

	/* run over all the sockets that we try to send info on */ 
	for( i = 0; i < next; i++ ) {

		/* Find the one that is ready */ 
		if( FD_ISSET( comm->comm_inpr_send[ i ].sock, connected_set )){

			int res_sz = sizeof(int);
			int so_error_val = 0;
			int recalc_max   = 0;
			int close_socket = 0; 

			/* We need the ip in any case (failure or success) */ 
			memcpy( ip, comm->comm_inpr_send[ i ].ip, COMM_IP_VER);

			/* test that the connect succeeded */ 
			if( !comm->comm_inpr_send[ i ].curr_size ){
				if( getsockopt(comm->comm_inpr_send[ i ].sock,
					      SOL_SOCKET, SO_ERROR,
					      &so_error_val, &res_sz ) == -1) {
					debug_lr( COMM_DEBUG,
						  "Error: getsockopt\n");
					so_error_val = 1;
					res = -1;
				}
			}

			/* The connection has been succefully established */ 
			if( so_error_val == 0 ) {

				/* the number of bytes to send */ 
				int send_size =
					comm->comm_inpr_send[ i ].data_size -
					comm->comm_inpr_send[ i ].curr_size;
		  
				/* the start position of the data to be send */
				char *tmp = (comm->comm_inpr_send[ i ].data) +
					comm->comm_inpr_send[ i ].curr_size;
		
				/* initiate the timeout count */
				gettimeofday( &(comm->comm_inpr_send[i].time),
					      NULL);
		
				res = send( comm->comm_inpr_send[ i ].sock,
					    tmp, send_size, MSG_NOSIGNAL );

				/* The send operation failed */ 
				if( res == -1 ){
					if((errno != EAGAIN) &&
					   (errno != EWOULDBLOCK)){
						debug_lr( COMM_DEBUG,
							  "Error: send. %s\n",
							  strerror(errno));
						res = -1;
						comm->comm_inpr_send[i].mv2recv
							= 0;
						goto bad_send ; 
					}
				}
		
				else if( res == 0 ) {
					debug_lr( COMM_DEBUG,
						  "Error: send=0\n" );
					comm->comm_inpr_send[ i ].mv2recv = 0;
					res = -1;
					goto bad_send;
			      
				}
			
				/* The send operation succeeded */ 
				else{
					/* set num bytes already sent */ 
					comm->comm_inpr_send[ i ].curr_size +=
						res;
		    
					if( comm->comm_inpr_send[i].curr_size
					    ==
					    comm->comm_inpr_send[i].data_size){
						close_socket = 1;
					}
		    
					else
						gettimeofday(
							&(comm->
							  comm_inpr_send[i].
							  time), NULL); 
					res = 0;
				}
			}

			/* The connect operation failed */
			else {
				debug_lr( COMM_DEBUG, "Error: Connect. %s\n",
					  strerror( so_error_val ));
				comm->comm_inpr_send[ i ].mv2recv = 0;
				res = -1;
				goto bad_send;
			}

			/* close the socket (error or finished send */ 
			if( close_socket ) {
				/* done only if the sending succeeded */ 
				res = 1;
			
			bad_send:
				if( comm->comm_inpr_send[ i ].data ) {
					free( comm->comm_inpr_send[ i ].data );
					comm->comm_inpr_send[i].data = NULL;
				}
		
				/* The send operation was successful */
				if( res == 1 ) {

					/* maybe we have to move to receive */
					if( comm->comm_inpr_send[i].mv2recv ){
						if( !(comm_add_inprogress_recv(
							      comm,
							      comm->comm_inpr_send[i].sock,
							      ip ))){

                                                        close(comm->comm_inpr_send[i].sock);
							/* add to 'closed' */
							comm_add_close( comm,
									comm->comm_inpr_send[i].sock);
							res = -2;
						}
					}
					else {
						/* add to 'closed' sockets */
						comm_add_close( comm,
								comm->comm_inpr_send[i].sock);
					}
				}
		
				else {
					close( comm->comm_inpr_send[ i ].sock);
					if( comm->comm_maxfd ==
					    comm->comm_inpr_send[ i ].sock)
						recalc_max = 1;
				}
		
				/*
				 * Fixing the set, we do that simply by
				 * moving the last sock to the current
				 * position. The order is not maintained
				 * but the operation is more efficient
				 */
				if( i != ( next - 1 )) {
					comm->comm_inpr_send[ i ] =
						comm->comm_inpr_send[next-1];
					bzero( &(comm->comm_inpr_send[next-1]),
					       sizeof(comm_inprogress_send_t));
				}
				comm->comm_inpr_send_next--;
			
				if( recalc_max )
					comm_calc_inprogress_maxfd( comm );
			}
			break;
		}
	}
	return res;
}

/****************************************************************************
 *  Send a message on an already open socket
 ***************************************************************************/
int
comm_send_on_socket( msx_comm_t *comm, int sock,
		     void *buff, int type, int size, int mv2recv){

	struct sockaddr_in info;
	socklen_t len = sizeof( struct sockaddr_in );

	if( getpeername( sock, (struct sockaddr*)&info, &len ) < 0 ){
		debug_lr( COMM_DEBUG, "Error: getting peer ip\n" );
		return 0;
	}
      
	return comm_add_inprogress_send( comm, sock,
					 (char*)&(info.sin_addr.s_addr),
					 buff, type, size, mv2recv );
}

/****************************************************************************
 * Establish the communication channel and set the socket to
 * non blocking
 ***************************************************************************/
int
comm_setup_connection( msx_comm_t *comm, fd_set *fds, char *ip ) {

	int sockfd;
	int i;
	struct sockaddr_in socketInfo; 
	int length =  sizeof( struct sockaddr_in ) ;

	/*
	 * The accept should succeed since we should enter this function when
	 * there is a new connection. Test which of the listenning
	 * sockets is ready.
	 */
	for( i = 0; i < COMM_MAX_LISTEN; i++ )
		if( FD_ISSET( comm->comm_sock[ i ], fds ))
			break;

	if( i == COMM_MAX_LISTEN )
		return -1;
  
	if( ( sockfd = accept( comm->comm_sock[ i ],
			       (struct sockaddr*)&socketInfo,
			       &length )) == -1 ){
		debug_lr( COMM_DEBUG, "Error: accept()\n" ) ;
		return -1 ;
	}

	/* set the socket to be nonblocking */ 
	fcntl( sockfd, F_SETFL, O_NONBLOCK);
    
	/* find a place in the inprogress_recv */
	if( !comm_add_inprogress_recv( comm, sockfd,
				       (char*)&(socketInfo.sin_addr.s_addr))){
		close( sockfd ) ;
		return -1;
	}
    
	/* get the ip of the remote machine */
	if( ip )
		memcpy( ip, ( char*)&(socketInfo.sin_addr.s_addr),
			COMM_IP_VER );
      	return 0; 
}

/****************************************************************************
 *  Receive a message. 
 ***************************************************************************/
int
comm_recv( msx_comm_t *comm, fd_set *fds, comm_inprogress_recv_t *recv_info ) {

	int res =  -1;
	int i = 0, recv_size = 0, num_read = 0, recv_fin = 0;
	int next = comm->comm_inpr_recv_next;
	int recalc_max = 0;
	char *tmp = NULL;
	comm_inprogress_recv_t *tmp_recv;
	
	/* find the socket on which data is ready */ 
	for( i = 0; i < comm->comm_inpr_recv_next; i++ )
		if( FD_ISSET( comm->comm_inpr_recv[ i ].sock, fds ))
			break ;

	/* again - sanity check */ 
	if( i == comm->comm_inpr_recv_next ){
		debug_lr( COMM_DEBUG, "Error: no socket ready for receive\n");
		return -1;
	}

	tmp_recv = &(comm->comm_inpr_recv[ i ]);

	/* first test if the header was received and if not get it */
	if( !comm->comm_inpr_recv[ i ].hdr.type ) {

		comm_hdr_t hdr ;
		int num2read = sizeof( comm_hdr_t ) ;
		int num_read = 0;
	  
		/* read the header to a temporary buffer */
		while( num2read ){
			if(( res=recv( comm->comm_inpr_recv[i].sock,
				       (&hdr) + num_read, num2read,
				       MSG_NOSIGNAL )) != num2read ) {

				if( (res == 0) ||
				    ((res == -1) && (errno != EINTR))){
					debug_lr( COMM_DEBUG,
						  "Error: Reading header\n" ); 
					res= -1;
					goto exit_with_close;
				}

				num2read -= res ;
				num_read += res ; 
			}
			else
				break; 
		}
	  
		comm->comm_inpr_recv[ i ].hdr = hdr; 


		if( (comm->comm_inpr_recv[ i ].hdr.size <= 0) ||
		    (comm->comm_inpr_recv[ i ].hdr.size > MAX_MSG_SIZE )) {
			debug_lb( COMM_DEBUG, "Error: Illegal message size\n");
			res = -1;
			goto exit_with_close;
		}
	  
		/* allocate the memory area for the data */
		if( !( comm->comm_inpr_recv[ i ].data =
		       malloc(comm->comm_inpr_recv[ i ].hdr.size ))) {
			debug_lb( COMM_DEBUG, "Error: malloc, comm_recv\n");
			res = -1;
			goto exit_with_close; 
		}
	}

	/* set the parameters for the recv */ 
	recv_size = comm->comm_inpr_recv[ i ].hdr.size -
		comm->comm_inpr_recv[ i ].curr_size; 
	tmp = comm->comm_inpr_recv[ i ].data +
		comm->comm_inpr_recv[i].curr_size;
    
	/* recv the message */ 
	num_read = recv( comm->comm_inpr_recv[ i ].sock, tmp,
			 recv_size, MSG_NOSIGNAL);

        if(num_read > 0) {
             comm->comm_inpr_recv[ i ].curr_size += num_read ;
             if( comm->comm_inpr_recv[ i ].curr_size ==
                 comm->comm_inpr_recv[ i ].hdr.size ) {
                  res = 1;
                  recv_fin = 1;
             }
             else{
                  gettimeofday( &(comm->comm_inpr_recv[i].time), NULL ) ;
                  res = 0;
                  goto exit_no_op;
             }
	}
        else if( num_read < 0 ){
		res = 0 ;
		if( errno != EAGAIN ){
                     debug_lr( COMM_DEBUG,
                               "Error: recv failed. num_read %d\n", num_read );
                     res= -1;
                     goto exit_with_free ;
		}
		else{
                     gettimeofday( &(comm->comm_inpr_recv[ i ].time), NULL);
                     res = 0;
                     goto exit_no_op;
		}
	}
        else { // case num_read == 0. this might happen for some strange cases
             debug_lr( COMM_DEBUG,
                       "Error: recv failed. num_read %d!!!!\n", num_read );
             res= -1;
             goto exit_with_free ;
        }
	
	if( recv_fin )  {
		/* copying the relevant fields to recv_info */
		*recv_info = *tmp_recv ;
		bzero( &(recv_info->time), sizeof(struct timeval));
		recv_info->curr_size = 0 ;
	
		goto delete_connection;
	}

 exit_with_free:
	if(comm->comm_inpr_recv[ i ].data){
		free(comm->comm_inpr_recv[ i ].data);
		comm->comm_inpr_recv[ i ].data = NULL;
	}
    
 exit_with_close:
	close( comm->comm_inpr_recv[ i ].sock );
    
 delete_connection:
	/* Deleting the connection */
	if( comm->comm_maxfd == tmp_recv->sock )
		recalc_max = 1;
    
	if(i != next-1) {
		comm->comm_inpr_recv[ i ] = comm->comm_inpr_recv[ next - 1 ];
		bzero( &(comm->comm_inpr_recv[ next - 1]),
		       sizeof(comm_inprogress_recv_t));
	}
	comm->comm_inpr_recv_next--;
    
	if( recalc_max )
		comm_calc_inprogress_maxfd( comm );
    
 exit_no_op:
	return res;
}

/****************************************************************************
 * Administration: close all the inactive sockets and free the
 * relevant resources.
 ***************************************************************************/
void
comm_admin( msx_comm_t *comm, int tflg ) {

	struct timeval time;
	int i = 0, recalc_max = 0;

	/* Test that it is already the time for the next admin operation */
	if( tflg ) {
		gettimeofday( &time, NULL );
		if( timercmp( &time, &(comm->next_admin), < ))
			return;
	}

	debug_lb( COMM_DEBUG, "Admin\n" );
	
	i = 0;
	while( i < comm->comm_inpr_send_next ) {
		if( ( comm->comm_inpr_send[ i ].time.tv_sec &&
		    (( time.tv_sec - comm->comm_inpr_send[i].time.tv_sec) >=
		     comm->comm_timeout )) ||
		    ( fcntl( comm->comm_inpr_send[ i ].sock, F_GETFL) < 0 )){
			
			close( comm->comm_inpr_send[ i ].sock );
			if( comm->comm_inpr_send[ i ].data ) {
				free( comm->comm_inpr_send[ i ].data );
				comm->comm_inpr_send[ i ].data = NULL;
			}
		  
			if(comm->comm_maxfd == comm->comm_inpr_send[i].sock)
				recalc_max = 1;
	    
			if(i != comm->comm_inpr_send_next - 1 )
				comm->comm_inpr_send[ i ] =
					comm->comm_inpr_send[ comm->comm_inpr_send_next-1];
			comm->comm_inpr_send_next--;
		}
		else
			i++;
	}
    
	i = 0;
	while( i < comm->comm_inpr_recv_next ) {
		if((( time.tv_sec - comm->comm_inpr_recv[ i ].time.tv_sec ) >=
		    comm->comm_timeout) ||
		    ( fcntl( comm->comm_inpr_recv[ i ].sock, F_GETFL ) < 0 )){
			
			if ( close(comm->comm_inpr_recv[ i ].sock) < 0 )
				debug_lb( COMM_DEBUG, "Error: close\n" );
			else
				debug_lb( COMM_DEBUG, "Closed socket %d\n",
					  comm->comm_inpr_recv[ i ].sock  );
			if( comm->comm_inpr_recv[ i ].data ) {
				free(comm->comm_inpr_recv[ i ].data);
				comm->comm_inpr_recv[ i ].data = NULL;
			}
		  
			if(comm->comm_maxfd == comm->comm_inpr_recv[ i ].sock)
				recalc_max = 1;
		  
			if( i != comm->comm_inpr_recv_next - 1 )
				comm->comm_inpr_recv[ i ] =
					comm->comm_inpr_recv[comm->comm_inpr_recv_next-1];
			comm->comm_inpr_recv_next--;
		}
		else
			i++;
	}

	i = 0;
	while( i < comm->comm_close_next ) {
		
		long usecs = 1000000 *
			(time.tv_sec - comm->comm_close[ i ].time.tv_sec ) +
			time.tv_usec - comm->comm_close[ i ].time.tv_usec;

		if( ( usecs >= CLOSE_WAIT_TIME ) ||
		    ( fcntl( comm->comm_close[ i ].sock, F_GETFL )) < 0 ) {

			debug_lb( COMM_DEBUG, "Closing socket\n" );
			
			if ( close(comm->comm_close[ i ].sock ) < 0 )
				debug_lb( COMM_DEBUG, "Error: close\n" );
			else
				debug_lb( COMM_DEBUG, "Closed socket %d\n",
					  comm->comm_close[ i ].sock );
		  
			if( comm->comm_maxfd == comm->comm_close[ i ].sock )
				recalc_max = 1;
		  
			if( i != comm->comm_close_next - 1 )
				comm->comm_close[ i ] =
					comm->comm_close[comm->comm_close_next-1];
			comm->comm_close_next--;
		}
		else
			i++;
	}
	
	if( recalc_max )
		comm_calc_inprogress_maxfd( comm );
	
	/* Set the time for the next admin operation */
	gettimeofday( &(comm->next_admin), NULL );
	comm->next_admin.tv_sec += COMM_ADMIN_WAIT;
}

/****************************************************************************
 * Adding to set all the fds we are listning on 
 ****************************************************************************/
int
comm_get_listening_fdset( msx_comm_t *comm, fd_set *set ) {

	int i = 0;
	for( i = 0; i < COMM_MAX_LISTEN; i++ )
		if( comm->comm_sock[ i ] > 0 )
			FD_SET( comm->comm_sock[ i ], set);
      	return 0;
}

/****************************************************************************
 * Adding to the set all the sockets that we are in the middle of send on
 ***************************************************************************/
int
comm_get_inprogress_send_fdset( msx_comm_t *comm, fd_set *set ) {

	int i = 0;
	for( i = 0; i < comm->comm_inpr_send_next; i++ ) 
		FD_SET( comm->comm_inpr_send[ i ].sock, set );
	return 0;
}

/****************************************************************************
 * Adding to the set all the sockets we are waiting for data to appear on
 ***************************************************************************/
int
comm_get_inprogress_recv_fdset( msx_comm_t *comm, fd_set *set ) {

	int i = 0;
    
	for( i = 0; i < comm->comm_inpr_recv_next ; i++ )
		FD_SET( comm->comm_inpr_recv[ i ].sock, set);
	return 0;
}

/****************************************************************************
 * Adding to the set all the sockets we are waiting to close
 ***************************************************************************/
int
comm_get_close_fdset( msx_comm_t *comm, fd_set *set ){

	int i = 0;
	for( i = 0; i < comm->comm_close_next; i++ )
		FD_SET( comm->comm_close[ i ].sock, set);
	return 0;
}

/***************************************************************************
 * Return the maximal file descriptor
 ***************************************************************************/
int
comm_get_max_sock(msx_comm_t *comm) {
	return comm->comm_maxfd;
}

/****************************************************************************
 * Read the header from the socket
 ***************************************************************************/
int
comm_recv_hdr( comm_hdr_t *hdr, int sock ) {
	int num2read = sizeof( comm_hdr_t );
	int num_read = 0, ret = 0;

        // A select before the actual receive
        fd_set rfds;
        struct timeval timeout;
        
	while(1){
                
                
		/* set the fd to wait on, and the time to wait  */ 
		FD_ZERO( &rfds );
		FD_SET( sock, &rfds );
		
		timeout.tv_sec  = 5;
		timeout.tv_usec = 0;
                
		ret = select( sock + 1, &rfds, NULL, NULL, &timeout );
		if( ret <= 0 ) {
			debug_r( "Error: recveing node info\n" ) ;
			return 0;
		}
		
                if(( ret = recv( sock, hdr + num_read, num2read,
				 MSG_NOSIGNAL )) !=   num2read ) {
			if( (ret == 0) || ((ret == -1) &&
					   (errno != EINTR) &&
					   (errno != EINPROGRESS))){
				debug_r( "Error: receiving hdr\n%s",
					 strerror( errno ));
				return 0;
			}
			num2read -= ret;
			num_read += ret; 
		}
		else
			break;
	}
	return 1;
}


/****************************************************************************
 * Wait for the connection to the infod established
 ***************************************************************************/
static int
comm_connect_wait( int sockfd ) {

	fd_set wfds;
	struct timeval timeout;
	int res_sz       = sizeof(int);
	int so_error_val = 0;
	
	FD_ZERO( &wfds );
	FD_SET( sockfd, &wfds );

	timeout.tv_sec  = COMM_CONNECT_WAIT;
	timeout.tv_usec = 0;
	
	if( ( select( sockfd + 1, NULL, &wfds, NULL, &timeout )) <= 0 )
		return -1;

	if( ( getsockopt( sockfd, SOL_SOCKET, SO_ERROR,
			  &so_error_val, &res_sz )) == -1 ) 
		return -1;
	return 0;
}

/****************************************************************************
 * Establish communication with the infod running on 'rserver'
 ***************************************************************************/ 
int
comm_connect_client( char* rserver, unsigned short portnum ){

	struct hostent* hp = NULL ;
	struct sockaddr_in socketInfo ;
	int  socketfd = 0 ;
	char server[ MAX_BUFF_SZ ];
	int flag = 0;
	
	/* create the socket */
	if( ( socketfd = socket( PF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
		debug_r( "Error: setting creating socket. socket()\n");
		return -1;
	}
      
	/* fill in port info */
	socketInfo.sin_port = htons( portnum );
	bzero( server, MAX_BUFF_SZ );
      
	/* if no server name was given, assume local machine */ 
	if( !rserver ) {
		/* resolv the hostname */
		if(( gethostname( server, MAX_BUFF_SZ )) < -1 ) {
			debug_r( "Error: Failed gethostbyname\n" ); 
			return -1;
		}
	}
	else
		strncpy( server, rserver, MAX_BUFF_SZ); 

	/* get server address */ 
	if( !( hp = (struct hostent*)gethostbyname( (const char*)server ))){
		debug_r( "Error: getting server address, gethostbyname()\n" ); 
		return -1; 
	}

	/* fill in the socket information */
	memcpy( &(socketInfo.sin_addr), hp->h_addr, hp->h_length );
	socketInfo.sin_family = hp->h_addrtype;

	/* Turn the socket to a nonblocking one */
	fcntl( socketfd, F_SETFL, O_NONBLOCK );
	
	/* make the connection */
	if(( connect( socketfd, (struct sockaddr*)&socketInfo,
		       sizeof(socketInfo))) == -1 ){
		if( errno != EINPROGRESS ) {
			debug_r( "Error: connect failed. %s\n",
				 strerror(errno));
			return -1;
		}
	}
	
	if( ( comm_connect_wait( socketfd )) == -1 ) {
		debug_r( "Error: connect failed\n" );
		close( socketfd );
		return -1;
	}

	/* Turn the socket to a blocking one */
	if( ioctl(socketfd, FIONBIO, &flag) < 0 )
		debug_r( "Error: ioctl\n" );
	
	/* return the new socket descriptor */
	return socketfd ;
}

/****************************************************************************
 * Printing the status of the comm object to a string
 ***************************************************************************/
void comm_print_status(msx_comm_t *comm, char *buff, int size)
{
        int i;
        char b[512];
        char *ptr = buff;

        ptr += sprintf(ptr, "COMM STATUS: send %d recv %d close %d  max %d\n",
                       comm->comm_inpr_send_next, comm->comm_inpr_recv_next,
                       comm->comm_close_next, comm->comm_maxfd);
        ptr += sprintf(ptr, "Listen: ");
        // Listening sockets
        for( i = 0; i < COMM_MAX_LISTEN; i++ )
                ptr+= sprintf(ptr, " %d", comm->comm_sock[i]);
        // Sending sockets
        ptr += sprintf(ptr, "\nInprogress send: ");
        for( i = 0; i < comm->comm_inpr_send_next; i++ )
                ptr+= sprintf(ptr, " %d", comm->comm_inpr_send[i].sock);
        // Receiving sockets
        ptr += sprintf(ptr, "\nInprogress recv: ");
        for( i = 0; i < comm->comm_inpr_recv_next; i++ )
                ptr+= sprintf(ptr, " %d", comm->comm_inpr_recv[i].sock);
        // Closing sockets
        ptr += sprintf(ptr, "\nClose: ");
        for( i = 0; i < comm->comm_close_next; i++ )
                ptr+= sprintf(ptr, " %d", comm->comm_close[i].sock);

        ptr+= sprintf(ptr, "\n");
}

/******************************************************************************
 *                             E O F
 *****************************************************************************/
