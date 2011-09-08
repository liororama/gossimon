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
 * Author(s): Amar Lior
 *
 *****************************************************************************/

#ifndef __MOSIX_COMM_
#define __MOSIX_COMM_

#include <unistd.h>

#define COMM_BINARY            (1)
#define COMM_MAX_INPROGRESS    (200) 
#define COMM_MAX_LISTEN        (5)
#define MAX_MSG_SIZE           (1024*1024)
#define CLOSE_WAIT_TIME        (1000000)
#define COMM_ADMIN_WAIT        (1)
#define COMM_CONNECT_WAIT      (1)

/****************************************************************************
 * header and header handling functions
 ***************************************************************************/
typedef struct comm_hdr {
	int type;
	int size;
} comm_hdr_t ;


/******************************************************************************
 * msx_comm and related interface functions 
 *****************************************************************************/

/*
 * Used to hold messages that should be asynchronously delivered
 */ 
typedef struct comm_inprogress_send {
    int         sock;
    struct      timeval time;
    int         data_size;     
    int         curr_size;
    void        *data;
    char        ip[COMM_IP_VER];
    int         mv2recv; 
      
} comm_inprogress_send_t;

/*
 * Used to hold messages that should be asynchronously read. Wait for all
 * the message to be read and only than pass it to the application.
 */ 
typedef struct comm_inprogress_recv{
    int         sock;
    struct      timeval time;
    comm_hdr_t  hdr;
    int         curr_size;
    void        *data;
    char        ip[COMM_IP_VER];
} comm_inprogress_recv_t;

/*
 * When sending a message is done, and no other messages should be read
 * from the other side, the socket is not closed. Instead we wait to
 * get an indication that the socket was closed by the other side and than
 * close it.
 */
typedef struct comm_send_close{
    int sock;
    struct timeval time;
} comm_send_close_t ;

/*
 * The msx_comm data structure
 */
typedef struct _msx_comm_hndl{
      
    int             comm_sock[COMM_MAX_LISTEN];
    int             comm_timeout; /* time out in seconds */
    int             comm_maxfd;
      
    comm_inprogress_send_t comm_inpr_send[COMM_MAX_INPROGRESS];
    int             comm_inpr_send_next;

    comm_send_close_t  comm_close[COMM_MAX_INPROGRESS];
    int                comm_close_next;
    
    comm_inprogress_recv_t comm_inpr_recv[COMM_MAX_INPROGRESS];
    int             comm_inpr_recv_next;

    // The time for the next admin op
    struct timeval  next_admin;
    
} msx_comm_t;

/****************************************************************************
 * The priority of the different sockets is according to their position
 * in comm_sock[]. In comm_is_new_connection we return first the lower numbered
 * socket.
 ***************************************************************************/
msx_comm_t  *comm_init( unsigned short *ports, int timeout );

/* Used to remove sockets that are not active */
void comm_admin( msx_comm_t *comm, int tflg );

/* Is a new connection requested? */ 
int comm_is_new_connection( msx_comm_t *comm, fd_set *set);

/* Is there enough space to send a message? */ 
int  comm_inprogress_send_active( msx_comm_t *comm, fd_set *set);

/* Is there information available to one of the read sockets? */ 
int  comm_inprogress_recv_active(msx_comm_t *comm, fd_set *set);

/* Is one of the 'too be closed' sockets closed on the other size ? */ 
int  comm_close_on_socket(msx_comm_t *comm, fd_set *set );

/* initiate a new connection to the given ip on the  specified port */ 
int  comm_send( msx_comm_t *comm, char *ip, unsigned short port,
		void *buff, int type, int size, int mv2recv );

/* send a message on an already initialized socket */ 
int  comm_send_on_socket( msx_comm_t *comm, int sock,
			  void *buff, int type, int size, int mv2recv );

/* conntinue sending a message on one of the sockets */ 
int comm_finish_send( msx_comm_t *comm, fd_set *finished, char *ip);

/* establish a new connection */ 
int comm_setup_connection( msx_comm_t *comm, fd_set *fds, char *ip );

/* recv a message */ 
int  comm_recv( msx_comm_t *comm, fd_set *fds,
		comm_inprogress_recv_t *recv_info );

int comm_close(msx_comm_t *comm);

int comm_get_listening_fdset( msx_comm_t *comm, fd_set *set );
int comm_get_inprogress_send_fdset( msx_comm_t *comm, fd_set *set );
int comm_get_inprogress_recv_fdset( msx_comm_t *comm, fd_set *set );
int comm_get_close_fdset(msx_comm_t *comm, fd_set *set);
int comm_get_max_sock( msx_comm_t *comm );

/* Read the header from a socket */
int comm_set_hdr( comm_hdr_t *hdr, int type, int size );
int comm_recv_hdr( comm_hdr_t *hdr, int sock );

int comm_connect_client( char* rserver, unsigned short portnum );

void comm_print_status(msx_comm_t *comm, char *buff, int size);

#endif /* __MOSIX_COMM_H */

/****************************************************************************
 *                             E O F 
 ***************************************************************************/
