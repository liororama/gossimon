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

/***************************************************************************
 * File: infolib.c
 **************************************************************************/

#include <netinet/in.h>      
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <msx_debug.h>
#include <msx_error.h>
#include <info.h>
#include <infolib.h>
#include <msx_error.h>
#include <comm.h>

#define  MAX_BUFF_SIZE 8192

static idata_t* infolib_recv_info( int sock );
static char*    infolib_recv_names( int sock );
static int      infolib_recv_stats( int sock, infod_stats_t *stats );
static char*    infolib_str_request( char* server, unsigned short portnum,
				     infolib_req_t req );

static idata_t* infolib_no_arg_request( char *server,unsigned short portnum,
					infolib_req_t req );

/****************************************************************************
 * Interface functions
 ***************************************************************************/

/****************************************************************************
 * Get the info about all the machines known to the server 
 ***************************************************************************/ 
idata_t*
infolib_all( char *server,unsigned short portnum ) {
	return infolib_no_arg_request(server, portnum, INFOLIB_ALL);
}
idata_t*
infolib_window( char *server,unsigned short portnum ) {
	return infolib_no_arg_request(server, portnum, INFOLIB_WINDOW);
}

static idata_t*
infolib_no_arg_request( char *server,unsigned short portnum, infolib_req_t req ) {

	int   sock, num_to_send;
	char  buff[MAX_BUFF_SIZE];
	comm_hdr_t    *msg_hdr;
	infolib_msg_t *message;
	idata_t *data = NULL;
      
	/* establish communication */ 
	if( ( sock = comm_connect_client( server, portnum ) ) == -1 )
		return NULL; 

	bzero( buff, MAX_BUFF_SIZE ) ;
      
	msg_hdr = (comm_hdr_t*)&(buff[0]);
	msg_hdr->type = INFOD_MSG_TYPE_INFOLIB;
	msg_hdr->size = sizeof( infolib_msg_t );
      
	/* prpare the message */
	message = (infolib_msg_t*)&(buff[ sizeof(comm_hdr_t) ]);
	message->version = MSX_INFOD_INFO_VER;
	message->request = req;

	num_to_send = sizeof(comm_hdr_t) + msg_hdr->size; 

	/* send the request to the daemon */ 
	if(( send( sock, buff, num_to_send, MSG_NOSIGNAL)) != num_to_send ){
		close(sock);
		debug_r( "Error: send Failed. %s\n", strerror(errno));
		return NULL;
	}
     
	/* get the data */
	data = infolib_recv_info( sock );
	close(sock);
      
	return data;
}

/****************************************************************************
 *  Get the information about a continuous set of machines 
 ***************************************************************************/
idata_t*
infolib_cont_pes( char *server,
		  unsigned short portnum, cont_pes_args_t *args ){

	int   sock, num_to_send;
	char  buff[MAX_BUFF_SIZE]; 
	comm_hdr_t    *msg_hdr;
	infolib_msg_t *message;
	idata_t *data = NULL;
      
	/* establish communication */ 
	if( ( sock = comm_connect_client( server, portnum ) ) == -1 )
		return NULL; 

	bzero( buff, MAX_BUFF_SIZE ) ;
      
	/* header fields */ 
	msg_hdr = (comm_hdr_t*)&(buff[0]);
	msg_hdr->type = INFOD_MSG_TYPE_INFOLIB;
	msg_hdr->size =
		sizeof( infolib_msg_t ) + sizeof(cont_pes_args_t);
      
	/* prpare the message */
	message = (infolib_msg_t*)&(buff[ sizeof(comm_hdr_t) ]);
	message->version = MSX_INFOD_INFO_VER;
	message->request = INFOLIB_CONT_PES;
	memcpy( message->args, args, sizeof(cont_pes_args_t));

	num_to_send = sizeof(comm_hdr_t) + msg_hdr->size; 

	/* send the request to the daemon */ 
	if(( send( sock, buff, num_to_send, MSG_NOSIGNAL)) != num_to_send ){
		close(sock);
		debug_r( "Error: send Failed. %s\n", strerror(errno));
		return NULL;
	}

	data = infolib_recv_info( sock );
	close(sock);

	return data;
}

/****************************************************************************
 * Get the load information about a subset of machines 
 ***************************************************************************/
idata_t*
infolib_subset_pes( char *server,
		    unsigned short portnum, subst_pes_args_t *args ) {

	int   sock, num_to_send;
	char  buff[MAX_BUFF_SIZE]; 
	comm_hdr_t    *msg_hdr;
	infolib_msg_t *message;
	idata_t *data  = NULL;
      
	/* establish communication */ 
	if( ( sock = comm_connect_client( server, portnum ) ) == -1 )
		return NULL; 

	bzero( buff, MAX_BUFF_SIZE ) ;
      
	msg_hdr = (comm_hdr_t*)&(buff[0]);
	msg_hdr->type = INFOD_MSG_TYPE_INFOLIB;
	msg_hdr->size = sizeof(infolib_msg_t) +
		sizeof(int) + ( args->size * sizeof(node_t) );
      
	/* prpare the message */
	message = (infolib_msg_t*)&(buff[ sizeof(comm_hdr_t) ]);
	message->version = MSX_INFOD_INFO_VER;
	message->request = INFOLIB_SUBST_PES;
	memcpy( message->args, args,
		sizeof(int) + (args->size * sizeof(node_t)));

	num_to_send = sizeof(comm_hdr_t) + msg_hdr->size; 

	/* send the request to the daemon */ 
	if(( send( sock, buff, num_to_send, MSG_NOSIGNAL)) != num_to_send ){
		close(sock);
		debug_r( "Error: send Failed. %s\n", strerror(errno));
		return NULL;
	}

	data = infolib_recv_info( sock );
	close(sock);

	return data;
}

/****************************************************************************
 * Get the information about the machines, whos names are given in
 * 'machines_names'. The str must be in the format:
 *                   machine1 machine2 machine3 ...
 ***************************************************************************/
idata_t*
infolib_subset_name( char *server,
		     unsigned short portnum, char *machines_names ) {


	int   sock, num_to_send;
	char  buff[MAX_BUFF_SIZE]; 
	comm_hdr_t    *msg_hdr;
	infolib_msg_t *message;
	idata_t* data = NULL;
      
	/* establish communication */ 
	if( ( sock = comm_connect_client( server, portnum ) ) == -1 )
		return NULL; 

	bzero( buff, MAX_BUFF_SIZE ) ;
      
	msg_hdr = (comm_hdr_t*)&(buff[0]);
	msg_hdr->type = INFOD_MSG_TYPE_INFOLIB;
	msg_hdr->size =
		sizeof(infolib_msg_t) + strlen(machines_names) + 1;
      
	/* prpare the message */
	message = (infolib_msg_t*)&(buff[ sizeof(comm_hdr_t) ]);
	message->version = MSX_INFOD_INFO_VER;
	message->request = INFOLIB_NAMES;
	memcpy( message->args, machines_names, strlen( machines_names ) + 1);

	num_to_send = sizeof(comm_hdr_t) + msg_hdr->size; 

	/* send the request to the daemon */ 
	if(( send( sock, buff, num_to_send, MSG_NOSIGNAL)) != num_to_send ){
		close(sock);
		debug_r( "Error: send Failed. %s\n", strerror(errno));
		return NULL;
	}

	data = infolib_recv_info( sock );
	close(sock);

	return data;
}

/****************************************************************************
 * Get all the nodes up to a given age
 ***************************************************************************/ 
idata_t*
infolib_up2age( char *server, unsigned short portnum, unsigned long age ) {
	
	int   sock, num_to_send;
	char  buff[MAX_BUFF_SIZE]; 
	comm_hdr_t    *msg_hdr;
	infolib_msg_t *message;
	idata_t  *data = NULL;
      
	/* establish communication */ 
	if( ( sock = comm_connect_client( server, portnum ) ) == -1 )
		return NULL; 

	bzero( buff, MAX_BUFF_SIZE ) ;
      
	msg_hdr = (comm_hdr_t*)&(buff[0]);
	msg_hdr->type = INFOD_MSG_TYPE_INFOLIB;
	msg_hdr->size = sizeof(infolib_msg_t) + sizeof(unsigned long);
      
	/* prpare the message */
	message = (infolib_msg_t*)&(buff[ sizeof(comm_hdr_t) ]);
	message->version = MSX_INFOD_INFO_VER;
	message->request = INFOLIB_AGE;
	memcpy( message->args, &age, sizeof(unsigned long));
      
	num_to_send = sizeof(comm_hdr_t) + msg_hdr->size; 

	/* send the request to the daemon */ 
	if(( send( sock, buff, num_to_send, MSG_NOSIGNAL)) != num_to_send ){
		close(sock);
		debug_r( "Error: send Failed. %s\n", strerror(errno));
		return NULL;
	}
     
	/* get the data */
	data = infolib_recv_info( sock );

	close(sock);
      
	return data;
}

/****************************************************************************
 * Get the names of all the machines 
 ***************************************************************************/
char*
infolib_all_names( char* server, unsigned short portnum ){
	return infolib_str_request( server, portnum, INFOLIB_GET_NAMES );
}

char* infolib_info_description(char *server, unsigned short portnum) {
	return infolib_str_request( server, portnum, INFOLIB_DESC );
}

/*****************************************************************************
 * Get the names of all the machines 
 ***************************************************************************/
char*
infolib_str_request( char* server, unsigned short portnum,
		     infolib_req_t req ) {

	int   sock, num_to_send;
	char  buff[MAX_BUFF_SIZE]; 
	comm_hdr_t    *msg_hdr;
	infolib_msg_t *message;
	char *names;
      
	/* establish communication */ 
	if( ( sock = comm_connect_client( server, portnum ) ) == -1 )
		return NULL; 

	bzero( buff, MAX_BUFF_SIZE ) ;
      
	msg_hdr = (comm_hdr_t*)&(buff[0]);
	msg_hdr->type = INFOD_MSG_TYPE_INFOLIB;
	msg_hdr->size = sizeof(infolib_msg_t);
      
	/* prpare the message */
	message = (infolib_msg_t*)&(buff[ sizeof(comm_hdr_t) ]);
	message->version = MSX_INFOD_INFO_VER;
	message->request = req;

	num_to_send = sizeof(comm_hdr_t) + msg_hdr->size; 

	/* send the request to the daemon */ 
	if(( send( sock, buff, num_to_send, MSG_NOSIGNAL)) != num_to_send ){
		close(sock);
		debug_r( "Error: send Failed. %s\n", strerror(errno));
		return NULL;
	}

	names = infolib_recv_names( sock );
	close(sock);

	return names ; 
}

/****************************************************************************
 * Get a structure holding all the statistics of the infod
 ***************************************************************************/
int
infolib_get_stats( char* server,
		   unsigned short portnum, infod_stats_t *stats ) {
      
	int   sock, num_to_send, ret;
	char  buff[MAX_BUFF_SIZE];
	comm_hdr_t    *msg_hdr;
	infolib_msg_t *message;
      
	/* establish communication */ 
	if( ( sock = comm_connect_client( server, portnum ) ) == -1 )
		return -1; 

	bzero( buff, MAX_BUFF_SIZE ) ;
      
	msg_hdr = (comm_hdr_t*)&(buff[0]);
	msg_hdr->type = INFOD_MSG_TYPE_INFOLIB;
	msg_hdr->size = sizeof( infolib_msg_t );
      
	/* prpare the message */
	message = (infolib_msg_t*)&(buff[ sizeof(comm_hdr_t) ]);
	message->version = MSX_INFOD_INFO_VER;
	message->request = INFOLIB_STATS;

	num_to_send = sizeof(comm_hdr_t) + msg_hdr->size; 

	/* send the request to the daemon */ 
	if(( send( sock, buff, num_to_send, MSG_NOSIGNAL)) != num_to_send ){
		close(sock);
		debug_r( "Error: send Failed. %s\n", strerror(errno));
		return -1;
	}
     
	/* get the data */
	ret = infolib_recv_stats( sock, stats );

	close(sock);
      
	return ret ;
}

/****************************************************************************
 *   "Private functinos" 
 ****************************************************************************/

/*
 * Get the information from the server
 */
static idata_t*
infolib_recv_info( int sock ){

	comm_hdr_t      hdr; 
	int ret = 0;
	void *data_buff = NULL;

	int num2read = 0, num_got = 0;
    

	/* get the message header */
	if( !( comm_recv_hdr( &hdr, sock ) ))
		return NULL;

	if( hdr.size <= 0 ) {
		debug_r( "Error: failed reading critical info in header\n" ) ;
		return NULL;
	}
    
	/* Allocate the array, to hold the reply */
	if( !( data_buff = malloc( hdr.size ))) {
		debug_r( "Error: malloc failed\n" ) ;
		return NULL;
	}

	/* Read all the information available on the socket */ 
	num2read = hdr.size ;
	num_got  = 0;
    
	while( 1 ){
		
		fd_set rfds;
		struct timeval timeout;

		/* set the fd to wait on, and the time to wait  */ 
		FD_ZERO( &rfds );
		FD_SET( sock, &rfds );
		
		timeout.tv_sec  = DEF_WAIT_SEC;
		timeout.tv_usec = 0;
                
		ret = select( sock + 1, &rfds, NULL, NULL, &timeout );
		if( ret <= 0 ) {
			debug_r( "Error: recveing node info\n" ) ;
			free( data_buff );
			return NULL;
		}
		
		ret = recv( sock, data_buff + num_got, num2read - num_got,
			    MSG_NOSIGNAL) ;
	
		if( (ret == 0 ) || ((ret == -1) && (errno != EINTR))){
			debug_r( "Error: recveing node info\n" ) ;
			free( data_buff );
			return NULL;
		}
		  
		num_got += ret ;
		if( num_got == num2read ){
			break;
		}
	}

	return (idata_t*)(data_buff);
}

/*
 * Get all the machine names from the daemon 
 */ 
static char*
infolib_recv_names( int sock ){

	comm_hdr_t      hdr; 
	int ret ;
	char *names;
	int num_read = 0, num2read = sizeof(comm_hdr_t);
      
	/* get the message header */
	if( !( comm_recv_hdr( &hdr, sock ) ))
		return NULL;

	if( hdr.size <= 0 ) {
		debug_r( "Error: errors in header\n" ) ; 
		return NULL;
	}

	if( !( names = (char*)malloc( hdr.size))){
		debug_r( "Error: malloc failed in infolib_recv_names\n" ) ;
		return NULL;
	}

	num_read = 0;
	num2read = hdr.size;
      
	/* get the answer for the requested command  */
	while(( ret = recv( sock, names + num_read, num2read, MSG_NOSIGNAL )) !=
	      num2read){

		if( (ret == 0) || ((ret == -1) && (errno != EINTR))){
			debug_r( "Error: receive answer\n" ) ;
			free( names );
			return NULL;
		}

		num_read += ret ;
		num2read -= ret ;
		if( num2read == 0 )
			break;
	}

	return names ; 
}

static int 
infolib_recv_stats( int sock, infod_stats_t *stats ) {

	comm_hdr_t hdr; 
	int ret ;
	int num_read = 0, num2read;

	/* get the message header */
	if( !( comm_recv_hdr( &hdr, sock ) ))
		return -1;

	if( hdr.size != STATS_SZ ) {
		debug_r( "Error: wrong message size\n" ) ; 
		return -1;
	}
	
	num2read = hdr.size;
      
	/* get the answer for the requested command  */
	while(( ret = recv( sock, stats + num_read,
			    num2read, MSG_NOSIGNAL )) != num2read) {
		
		if( (ret == 0) || ((ret == -1) && (errno != EINTR))){
			debug_r( "Error: receiving infod stat\n" ) ; 
			return -1;
		}
	    
		num_read += ret ;
		num2read -= ret ;
		if( num2read == 0 )
			break;
	}
	return 1 ; 
}

// Extra functions
char *infoStatusToStr(int status) {

	if(status & INFOD_DEAD_CONNECT)
		return INFOD_DEAD_CONNECT_STR;
	if(status & INFOD_DEAD_PROVIDER)
		return INFOD_DEAD_PROVIDER_STR;
	if(status & INFOD_DEAD_INIT)
		return INFOD_DEAD_INIT_STR;
	if(status & INFOD_DEAD_AGE)
		return INFOD_DEAD_AGE_STR;
	if(status & INFOD_DEAD_VEC_RESET)
		return INFOD_DEAD_VEC_RESET_STR;
	if(status & INFOD_ALIVE)
		return INFOD_ALIVE_STR;
	return INFOD_DEAD_UNKNOWN_STR;
}

char external_status_buff[40];
char *infoExternalStatusToStr(int status) {

	if(status == EXTERNAL_STAT_INFO_OK)
                return EXTERNAL_STAT_INFO_OK_STR;
        if(status == EXTERNAL_STAT_NO_INFO)
                return EXTERNAL_STAT_NO_INFO_STR;
        if(status == EXTERNAL_STAT_EMPTY)
                return EXTERNAL_STAT_EMPTY_STR;
        if(status == EXTERNAL_STAT_ERROR)
                return EXTERNAL_STAT_ERROR_STR;

        if(status >= 0) {
                sprintf(external_status_buff, "%d", status);
                return external_status_buff;
        }

        // On case of unknown status
        sprintf(external_status_buff, "%s", "unknown status");
        return external_status_buff;
}





/******************************************************************************
                           E O F
******************************************************************************/
