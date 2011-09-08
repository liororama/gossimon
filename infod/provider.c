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
 * Author(s): Amar Lior
 *
 *****************************************************************************/

/******************************************************************************
 * File: kernel_comm.c. implementation of the kernel comm sybsystem
 *
 * This subsystem is responsible for communicating with the "KERNEL" which
 * is for us the local information provider as well as a source of special
 * requests. The information we collect is pushed to the kernel in periodic
 * times as well as other information the kernel may need such as speed update
 * and cost update.
 *
 * This interface can be used to communicate both with LINUX MOSIX
 * kernel.
 *****************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <msx_error.h>
#include <msx_debug.h>
#include <info.h>
#include <prioHeap.h>
#include <provider.h>
#include <linuxMosixProvider.h>

/****************************************************************************
 * Global variables 
 ***************************************************************************/

/* The status of comm */ 
static enum kcomm_status {
	KCOMM_UN_INITILIZED,   // Not initilized yet.
	KCOMM_WAIT_FOR_NEW,    // Waiting for a new kcomm connection
	KCOMM_UP_NOT_AUTH_YET, // kcomm is up but we didn't got cred msg yet
	KCOMM_UP,              // kcomm is up and authenticated
} kcomm_status = KCOMM_UN_INITILIZED;


static int kcomm_client_mode = 0; // value of 1 --> working as client 
static int kcomm_inprsend = 0;    // value of 1 --> in the middle of a send 
static int kcomm_inprrecv = 0;    // value of 1 --> in the middle of a recv 

/* Used to hold received messages */ 
struct inpr_recv_data {
	char     *base;       // base address
	int       max_len;     // maximal size
	char     *ptr;        // current location
	int       curr_len;    // length from start
} inpr_recv_data;

/* Used to hold sent messages */ 
struct inpr_send_data {
	char      *base;
	int        max_len;
	int        len;
} inpr_send_data;

static heap_t kcomm_send_heap;             // priority heap
static int    kcomm_sockfd = -1;           // the socket file descriptor
static int    kcomm_curr_sockfd = -1;    
static int    kcomm_is_there_extra = 0;
static char   kcomm_sockname[MAX_KCOMM_SOCK_NAME];

#define KCOMM_BUFF_SZ (1024)

static char kcomm_buff[KCOMM_BUFF_SZ];
static char *kcomm_inpr_send_buff = NULL;
static unsigned int kcomm_inpr_send_buff_sz = 0 ;


/****************************************************************************
 Function prototypes as should be used by kcomm. The noprovider arguments
 cause the kcomm to be used in another way so we replace the functions
 in one place and all the relevant places uses the correct function.
****************************************************************************/
typedef int    (*kcomm_init_func_t)    (char *, int);
kcomm_init_func_t                 kcomm_init_func;

typedef int    (*kcomm_stop_func_t)    (void);
kcomm_stop_func_t                 kcomm_stop_func;

typedef int    (*kcomm_reset_func_t)   (void);
kcomm_reset_func_t                kcomm_reset_func;

typedef int    (*kcomm_get_fdset_func_t) (fd_set *, fd_set *wfds, fd_set *);
kcomm_get_fdset_func_t            kcomm_get_fdset_func;

typedef int    (*kcomm_get_max_sock_func_t) (int);
kcomm_get_max_sock_func_t         kcomm_get_max_sock_func;

typedef int    (*kcomm_is_new_connection_func_t) (fd_set *);
kcomm_is_new_connection_func_t   kcomm_is_new_connection_func;

typedef int    (*kcomm_is_req_arrived_func_t)  (fd_set *);
kcomm_is_req_arrived_func_t       kcomm_is_req_arrived_func;

typedef int    (*kcomm_is_ready_to_send_func_t) (fd_set *);
kcomm_is_ready_to_send_func_t     kcomm_is_ready_to_send_func;

typedef int    (*kcomm_exception_func_t) (fd_set *);
kcomm_exception_func_t     kcomm_exception_func;

static int kcomm_stop_np();
static int kcomm_stop_p();
static int kcomm_reset_np();
static int kcomm_reset_p();
static int kcomm_get_fdset_np(fd_set *rfds, fd_set *wfds, fd_set *efds);
static int kcomm_get_fdset_p( fd_set *rfds, fd_set *wfds, fd_set *efds );
static int kcomm_get_max_sock_np( int curr_max );
static int kcomm_get_max_sock_p( int curr_max );
static int kcomm_is_new_connection_np( fd_set *rfds );
static int kcomm_is_new_connection_p( fd_set *rfds );
static int kcomm_is_req_arrived_np( fd_set *rfds );
static int kcomm_is_req_arrived_p( fd_set *rfds );
static int kcomm_is_ready_to_send_np( fd_set *wfds );
static int kcomm_is_ready_to_send_p( fd_set *wfds );
static int kcomm_exception_np(fd_set *efds);
static int kcomm_exception_p(fd_set *efds);


/****************************************************************************
 * The structure holding the functions that are used to communicate with
 * the information provider. These functions are defined in
 * kernel_comm_protocol.h, and are loaded from the supplied module.
 ***************************************************************************/

kcomm_module_func_t kcomm_mf;

/******************************************************************************
 * Loading specific kernel functions. Those are the only one that should know
 * About the description the kernel provides us and how to handle the kernel
 * Messages. All the conversion between basic info and the real kernel info
 * will be done using those functions.
 *****************************************************************************/

/****************************************************************************
 * Load the specific functions from the module. The module name is given as
 * an argument.
 ***************************************************************************/
static int 
load_provider_module( char *kcomm_module ) {

	void *hndl;
	char *error;

	/* Check if the internal mosix provider was requested */ 
	if( !strcmp( kcomm_module, INFOD_INTERNAL_PROVIDER_MODE )) {
		kcomm_mf = mosix_provider_funcs; 
		return 1;
	}
	
	/* open the file holding the funcs */ 
	if( !( hndl = dlopen(kcomm_module, RTLD_NOW) )) {
		debug_r( "Error: %s\n", dlerror());
		return 0;
	}

	// Note that  the correct way to check error on dlsym is to
	// check the return value of dlerror(). See man dlsym.
	kcomm_mf.init = dlsym( hndl, KCOMM_SF_INIT_FUNC );

	if( (error = dlerror()) != NULL ) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_INIT_FUNC, error );
		return 0;
	}
    
	kcomm_mf.reset = dlsym( hndl, KCOMM_SF_RESET_FUNC );
	if((error = dlerror()) != NULL) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_RESET_FUNC, error );
		return 0;
	}

	kcomm_mf.print_msg = dlsym( hndl, KCOMM_SF_PRINT_MSG_FUNC );
	if((error = dlerror()) != NULL) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_PRINT_MSG_FUNC, error );
		return 0;
	}

	kcomm_mf.parse_msg = dlsym( hndl, KCOMM_SF_PARSE_MSG_FUNC );
	if((error = dlerror()) != NULL) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_PARSE_MSG_FUNC, error );
		return 0;
	}
    
	kcomm_mf.handle_msg = dlsym( hndl, KCOMM_SF_HANDLE_MSG_FUNC );
	if((error = dlerror()) != NULL) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_HANDLE_MSG_FUNC, error );
		return 0;
	}

	kcomm_mf.periodic_admin = dlsym( hndl, KCOMM_SF_PERIODIC_ADMIN_FUNC );
	if((error = dlerror()) != NULL) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_PERIODIC_ADMIN_FUNC,
			 error );
		return 0;
	}

	kcomm_mf.get_stats = dlsym( hndl, KCOMM_SF_GET_STATS_FUNC );
	if((error = dlerror()) != NULL) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_GET_STATS_FUNC,
			 error );
		return 0;
	}
    
	kcomm_mf.get_infodesc = dlsym( hndl, KCOMM_SF_GET_INFODESC_FUNC);
	if((error = dlerror()) != NULL) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_GET_INFODESC_FUNC, error );
		return 0;
	}

	kcomm_mf.get_infosize = dlsym( hndl, KCOMM_SF_GET_INFOSIZE_FUNC);
	if ((error = dlerror()) != NULL) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_GET_INFOSIZE_FUNC, error );
		return 0;
	}
    
	kcomm_mf.get_info = dlsym( hndl, KCOMM_SF_GET_INFO_FUNC);
	if ((error = dlerror()) != NULL) {
		debug_r( "Error: loading %s\n%s\n",
			 KCOMM_SF_GET_INFO_FUNC, error );
		return 0;
	}
	
	
	// Not an obligatory function
	kcomm_mf.set_pe = dlsym( hndl, KCOMM_SF_SET_PE_FUNC);
	if ((error = dlerror()) != NULL) {
		debug_g( "Error: loading %s\n%s\n",
			 KCOMM_SF_SET_PE_FUNC, error );
		kcomm_mf.set_pe = NULL;
	}

	
	// Not an obligatory function
	kcomm_mf.set_map_change_hndl =
		dlsym( hndl, KCOMM_SF_SET_MAP_CHANGE_HNDL_FUNC);
	if ((error = dlerror()) != NULL) {
		debug_g( "Error: loading %s\n%s\n",
			 KCOMM_SF_SET_MAP_CHANGE_HNDL_FUNC, error );
		kcomm_mf.set_map_change_hndl = NULL;
	}
	
	
	return 1;
}


/* Nothing to do since no communication need to be setup */
int kcomm_init_np(char *kcomm_sock_name, int max_send)
{
	kcomm_status = KCOMM_UP;
	debug_lb(KCOMM_DEBUG, "Kcomm initialized (noprovider)\n" );
	return 1;
}

int kcomm_init_p(char *kcomm_sock_name, int max_send) {
	
	struct sockaddr_un addr;

	debug_lb( KCOMM_DEBUG, "Initializing kcomm (provider)\n" );
	/* Initialize the heap */
	if( !heap_init( &kcomm_send_heap, max_send )) {
		debug_lr( KCOMM_DEBUG, "Error: initializing send heap\n" );
		goto failed;
	}
	
	/* This is done to avoid the "address already in use" error */ 
	unlink(kcomm_sock_name);
	
	/* get the socket name and create the socket */ 
 	strncpy( kcomm_sockname, kcomm_sock_name, MAX_KCOMM_SOCK_NAME );
	if ( ( kcomm_sockfd = socket( PF_UNIX, SOCK_STREAM, 0)) == -1 ) {
		debug_lr( KCOMM_DEBUG,"Error: socket, %s\n", strerror(errno));
		goto failed;
	}

	addr.sun_family = AF_UNIX;
    	strcpy( addr.sun_path, kcomm_sock_name );
	
	if( bind( kcomm_sockfd, (struct sockaddr*)&addr,
		  sizeof( struct sockaddr_un )) == -1 ) {
		debug_lr( KCOMM_DEBUG, "Error: bind, %s\n", strerror(errno));
		goto failed;
	}
		
	if( listen( kcomm_sockfd, 5 ) == -1) {
		debug_lr( KCOMM_DEBUG, "Error: listen, %s\n", strerror(errno));
		unlink(kcomm_sockname);
		goto failed;
	}
	
	/* Initialize the send buffer */
        kcomm_inpr_send_buff_sz = KCOMM_BUFF_SZ;
	if( !( kcomm_inpr_send_buff = malloc( KCOMM_BUFF_SZ ))){
		debug_lr( KCOMM_DEBUG, "Error: malloc\n" );
		goto failed;

	}
	
	/* The comm is initialized. Waiting for connections */
	kcomm_status = KCOMM_WAIT_FOR_NEW;
	debug_lb( KCOMM_DEBUG, "Kcomm initialized (provider)\n" );
	return 1;
	
 failed:
	heap_free( &kcomm_send_heap );
	close( kcomm_sockfd );
	kcomm_sockfd = -1;
	kcomm_curr_sockfd = -1;
	kcomm_status = KCOMM_UN_INITILIZED;
    	return 0 ;
}


/****************************************************************************
 * Initilizing the kcomm:
 * 1. Load the specific functions of the kernel.
 * 2. Initialize the priority heap.
 * 3. Create the socket and wait for incoming connections.
 ***************************************************************************/
int
kcomm_init( char *kcomm_module, char *kcomm_sock_name, int max_send, 
	    int module_argc, char **module_argv) {
	
	//struct sockaddr_un addr;
	int noprovider;

	noprovider = (kcomm_sock_name == NULL) ? 1 : 0;
	debug_lr( KCOMM_DEBUG, "Kcomm init called (np %d) (module %s)\n",
		  noprovider, kcomm_module);

	/* Load the specific functions of the given module */ 
	if( !load_provider_module( kcomm_module ) ) {
		debug_lr( KCOMM_DEBUG, "Error: loading kcomm module %s\n",
			  kcomm_module );
		return 0;
	}
	
	if(noprovider)
	{
		kcomm_init_func               = kcomm_init_np;
		kcomm_stop_func               = kcomm_stop_np;
		kcomm_reset_func              = kcomm_reset_np;
		kcomm_get_fdset_func          = kcomm_get_fdset_np;
		kcomm_get_max_sock_func       = kcomm_get_max_sock_np;
		kcomm_is_new_connection_func  = kcomm_is_new_connection_np;
		kcomm_is_req_arrived_func     = kcomm_is_req_arrived_np;
		kcomm_is_ready_to_send_func   = kcomm_is_ready_to_send_np;
		kcomm_exception_func          = kcomm_exception_np;
	}
	else {
		kcomm_init_func               = kcomm_init_p;
		kcomm_stop_func               = kcomm_stop_p;
		kcomm_reset_func              = kcomm_reset_p;
		kcomm_get_fdset_func          = kcomm_get_fdset_p;
		kcomm_get_max_sock_func       = kcomm_get_max_sock_p;
		kcomm_is_new_connection_func  = kcomm_is_new_connection_p;
		kcomm_is_req_arrived_func     = kcomm_is_req_arrived_p;
		kcomm_is_ready_to_send_func   = kcomm_is_ready_to_send_p;
		kcomm_exception_func          = kcomm_exception_p;
	}
	/* Run the initiation function supplied by the module */ 
	if(!(*kcomm_mf.init)( noprovider, module_argc, module_argv )) {
                debug_lr(KCOMM_DEBUG, "Error: error initializing kcomm specific module\n");
		return 0;
        }
	
	if((*kcomm_init_func)(kcomm_sock_name, max_send) == 0) {
		debug_lr(KCOMM_DEBUG, "Error: error initializing kcomm\n");
		return 0;
	}
	return 1;
}	



/****************************************************************************
 * Initiate a client that uses the kcomm. The client should send its
 * credentials and control bytes so it could be identified by the infod.
 ***************************************************************************/
int kcomm_init_as_client( char *kcomm_sock_name, int max_send, char *kcomm_module) 
{
	int sock = 0;
	struct sockaddr_un addr;
    
	struct msghdr msg = {0};
	struct cmsghdr *cmsg;
	struct ucred my_cred;
	struct ucred *cred_ptr;
	char buf[CMSG_SPACE(sizeof(struct ucred))];
	
	struct iovec vec[1];
	char buff1[] = "m";

	if( !load_provider_module(kcomm_module ) ) {
		debug_lr( KCOMM_DEBUG, "Error: loading kcomm module %s\n",
			 kcomm_module );
		goto failed;
	}

	(*kcomm_mf.init)(1, 0, NULL );
	
	if( !heap_init(&kcomm_send_heap, max_send ) ) {
		debug_lr( KCOMM_DEBUG, "Error: initializing send heap\n");
		goto failed;
	}
    
	if( ( sock = socket(PF_UNIX, SOCK_STREAM, 0)) == -1 ) {
		debug_lr( KCOMM_DEBUG, "Error: opening socket\n%s\n",
			  strerror(errno));
		goto failed;
	}

	/* Initiate the socket */ 
	addr.sun_family = AF_UNIX;
	strcpy( addr.sun_path, kcomm_sock_name ) ;
    
	if( connect( sock, (struct sockaddr *)&addr,
		     sizeof( struct sockaddr_un)) == -1 ) {
		debug_lr( KCOMM_DEBUG, "Error: connect\n%s\n",
			  strerror(errno));
 		goto failed;
	}

	// construct the message 
	my_cred.pid = getpid();
	my_cred.uid = getuid();
	my_cred.gid = getgid();
    
	msg.msg_control = buf;
	msg.msg_controllen = sizeof( buf );

	// returns a pointer to the first cmsghdr
	cmsg             = CMSG_FIRSTHDR( &msg ) ;

	// specifies that we sent ancillary data (credentials)
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type  = SCM_CREDENTIALS;
	cmsg->cmsg_len   = CMSG_LEN(sizeof(struct ucred));
	
	/* Initialize the payload: */
	cred_ptr = (struct ucred *)CMSG_DATA(cmsg);
	memcpy( cred_ptr, &my_cred, sizeof(struct ucred) ) ;
	
	/* Sum of the length of all control messages in the buffer: */
	msg.msg_controllen = cmsg->cmsg_len;
    
	vec[0].iov_base = buff1;
	vec[0].iov_len= 1;
    
	msg.msg_iov = vec;
	msg.msg_iovlen = 1;

	// Sending the message holding both the credentials
	// of the process and (in cmsg) and the control bytes (iovec)
	if( sendmsg( sock, &msg, 0 ) == -1 ) {
		debug_lr( KCOMM_DEBUG, "Error: sendmsg\n%s\n",
			  strerror(errno));
		goto failed;
	}
    
	kcomm_status = KCOMM_UP;
	kcomm_curr_sockfd = sock;
	kcomm_client_mode = 1;

	/* Initialize the send buffer */
        kcomm_inpr_send_buff_sz = KCOMM_BUFF_SZ;
	if( !( kcomm_inpr_send_buff = malloc( KCOMM_BUFF_SZ ))){
		debug_lr( KCOMM_DEBUG, "Error: malloc\n" );
		goto failed;

	}

	debug_lb( KCOMM_DEBUG, "Kcomm initialized client\n" );
	return 1;
	
 failed:
	if( sock != 0 )
		close(sock);
	heap_free( &kcomm_send_heap );
	return 0;
}

/****************************************************************************
 * Stop the kcomm subsystem.
 ***************************************************************************/
static int kcomm_stop_np() {
	return 0;
}
static int kcomm_stop_p()
{
	debug_lb( KCOMM_DEBUG, "Kcomm closing\n" );
	close( kcomm_sockfd );
	close( kcomm_curr_sockfd );
	
	kcomm_sockfd = -1;
	kcomm_curr_sockfd = -1;
	kcomm_status = KCOMM_UN_INITILIZED;
	
	if( !kcomm_client_mode ) {
		if( unlink( kcomm_sockname ) == -1 ) {
			debug_lr( KCOMM_DEBUG, "Error: unlink, %s\n",
				  kcomm_sockname );
			return 0;
		}
	}
	return 1;
}

int kcomm_stop() {
	return (*kcomm_stop_func)();
}


/****************************************************************************
 * Resetting kcomm closing only current socket. 
 ***************************************************************************/
static int kcomm_reset_np() {
	return 1;
}
	
static int kcomm_reset_p() {
	
	debug_lb( KCOMM_DEBUG, "Kcomm Reset\n" );
	if( (kcomm_status != KCOMM_UP ) &&
	    ( kcomm_status !=  KCOMM_UP_NOT_AUTH_YET ))
		return 0;
    
	close( kcomm_curr_sockfd ) ;
	kcomm_curr_sockfd = -1;

	// In client mode - uninit.
	// In server mode - wait for new connection.
	if( kcomm_client_mode)
		kcomm_status = KCOMM_UN_INITILIZED;
	else 
		kcomm_status = KCOMM_WAIT_FOR_NEW;;

    
	kcomm_inprrecv = 0;
	kcomm_inprsend = 0;
	heap_reset( &kcomm_send_heap );

	// run the specified reset function of the module. 
	(*kcomm_mf.reset)();
    
	return 1;
}

int kcomm_reset() {
	return (*kcomm_reset_func)();
}
	

/*****************************************************************************
 * Insert a socket to the rfds.
 * In KCOMM_WAIT_FOR_NEW   insert  kcomm_sockfd
 * In KCOMM_WAIT_UP then   insert  kcomm_curr_sockfd 
 * In KCOMM_UN_INITILIZED  insert  nothing
 *
 * Return value: the number of sockfd set in on the rfds
 *****************************************************************************/
static int kcomm_get_fdset_np(fd_set *rfds, fd_set *wfds, fd_set *efds) {
	return 0;
}

static int kcomm_get_fdset_p( fd_set *rfds, fd_set *wfds, fd_set *efds ) {
	int res = 0;
	if( kcomm_status == KCOMM_WAIT_FOR_NEW ) {
		FD_SET( kcomm_sockfd, rfds );
		res = 1;
	}
	else if( (kcomm_status == KCOMM_UP) ||
		 (kcomm_status == KCOMM_UP_NOT_AUTH_YET)) {
		FD_SET( kcomm_curr_sockfd, rfds );
		FD_SET( kcomm_curr_sockfd, efds );
		if( kcomm_inprsend )
			FD_SET( kcomm_curr_sockfd, wfds ) ;
		res = 1;
	}
	return res;
}

int kcomm_get_fdset(fd_set *rfds, fd_set *wfds, fd_set *efds) {
	return (*kcomm_get_fdset_func)(rfds, wfds, efds);
}


/*****************************************************************************
 * Returning the maximum sockfd we are currently working with compared with
 * the curr_max socket.
 *****************************************************************************/
static int kcomm_get_max_sock_np( int curr_max ) {
	return curr_max;
}
static int kcomm_get_max_sock_p( int curr_max ) {
	int working_sock;
    
	switch( kcomm_status ) {
	    case KCOMM_WAIT_FOR_NEW:
		    working_sock = kcomm_sockfd;
		    break;
	    case KCOMM_UP:
	    case KCOMM_UP_NOT_AUTH_YET:
		    working_sock = kcomm_curr_sockfd;
		    break;
	    default:
		    working_sock =  -1;
	}

	return (curr_max > working_sock)? curr_max : working_sock;
}

int kcomm_get_max_sock( int curr_max ) {
	return (*kcomm_get_max_sock_func)(curr_max);
}

/*****************************************************************************
 * Checking if kcomm_sockfs is on the rfds we got from the select. If yes this
 * means we can call accept on this socket
*****************************************************************************/
static int kcomm_is_new_connection_np( fd_set *rfds ) {
	return 0;
}
static int kcomm_is_new_connection_p( fd_set *rfds ) {
	if( (kcomm_status == KCOMM_WAIT_FOR_NEW) &&
	    FD_ISSET( kcomm_sockfd, rfds ) )
		return 1;
	return 0;
}

int kcomm_is_new_connection( fd_set *rfds ) {
	return (*kcomm_is_new_connection_func)(rfds);
}

 
/****************************************************************************
 * An connection was requested. Call accept to establish the connection
 * on the main socket and move kcomm to KCOMM_UP state
 ****************************************************************************/
int kcomm_setup_connection() {
	
	int new_sock;
	int conn_addr_len = sizeof(struct sockaddr_un);
	struct sockaddr_un conn_addr;

	debug_lb( KCOMM_DEBUG, "Kcomm connection setup\n" );
	if( ( new_sock = accept( kcomm_sockfd, (struct sockaddr *)&conn_addr,
				(socklen_t *) &conn_addr_len) ) == -1 )	{
		debug_lr( KCOMM_DEBUG, "Error in accept\n%s\n",
			  strerror(errno));
		return 0;
	}

	/* Set the socket to a nonblocking */
	fcntl( new_sock, F_SETFL, O_NONBLOCK );

	/* A new connection was established but not yet authorized */
	kcomm_status = KCOMM_UP_NOT_AUTH_YET;
	kcomm_curr_sockfd = new_sock;


	return 1;
}

/****************************************************************************
 * Checking if the kcomm_curr_sockfd is in rfds returend from the select.
 * If true, get the kcomm request.
 ****************************************************************************/
static int kcomm_is_req_arrived_np( fd_set *rfds ) {
	return 0;
}
static int kcomm_is_req_arrived_p( fd_set *rfds ) {

	if( ( (kcomm_status == KCOMM_UP) ||
	      (kcomm_status == KCOMM_UP_NOT_AUTH_YET))
	    && FD_ISSET( kcomm_curr_sockfd, rfds) )
		return 1;
	return 0;
}
int kcomm_is_req_arrived( fd_set *rfds ) {
	return (*kcomm_is_req_arrived_func)(rfds);
}

/****************************************************************************
 * Checking if the kcom_curr_sockfd is in the wfds set. Which means that
 * it is possible to send a message  (if we are in the middle of a send)
 ****************************************************************************/
static int kcomm_is_ready_to_send_np( fd_set *wfds ) {
	return 0;
}

static int kcomm_is_ready_to_send_p( fd_set *wfds ) {
	if( ( (kcomm_status == KCOMM_UP) ||
	      (kcomm_status == KCOMM_UP_NOT_AUTH_YET))
	    && FD_ISSET( kcomm_curr_sockfd, wfds) )
		return 1;
	return 0;
}

int kcomm_is_ready_to_send( fd_set *wfds ) {
	return (*kcomm_is_ready_to_send_func)(wfds);
}


/****************************************************************************
 * Checking if kcomm fd is one of the exceptions fds.
 ****************************************************************************/
static int kcomm_exception_p( fd_set *efds ) {
	if( ( (kcomm_status == KCOMM_UP) ||
	      (kcomm_status == KCOMM_UP_NOT_AUTH_YET))
	    && FD_ISSET( kcomm_curr_sockfd, efds) ) 
		return 1;
	return 0;
}

static int kcomm_exception_np( fd_set *efds ) {
	return 0;
}

int kcomm_exception(fd_set *efds) {
	return (*kcomm_exception_func)(efds);
}
/*****************************************************************************
 * Checking if the channel is authenticated. This relate only when working
 * on server mode and we expect the authentication to be sent to us by the
 * client
 ****************************************************************************/

int kcomm_is_auth() {
	if( !kcomm_client_mode && kcomm_status == KCOMM_UP )
		return 1;
	return 0;
}

int
kcomm_is_up() {
	if( kcomm_status == KCOMM_UP )
		return 1;
	return 0;
}

/****************************************************************************
 * Get the authentication message.
 ***************************************************************************/
int
kcomm_recv_auth() {
	
	int recv_len;
	int flag = 1;
	char auth_chr;
    
	struct msghdr msgh;
	struct cmsghdr *cmsg;
	struct ucred *cred_ptr;
	struct ucred received_cred;
	struct iovec vec[1];

	// Preparing the message 
	msgh.msg_name    = NULL;
	msgh.msg_namelen = 0;
	msgh.msg_control = kcomm_buff;
	msgh.msg_controllen = KCOMM_BUFF_SZ;

	// We expect only one char 'm'
	vec[0].iov_base = &auth_chr;
	vec[0].iov_len=  1;
    
	msgh.msg_iov = vec;
	msgh.msg_iovlen = 1;

	// Enabling receiving of credentials of the sending process 
	setsockopt( kcomm_curr_sockfd, SOL_SOCKET, SO_PASSCRED, &flag,
		    sizeof(int));

	// Get the message 
	recv_len = recvmsg( kcomm_curr_sockfd, &msgh, MSG_NOSIGNAL );
	if( recv_len <= 0 ) {
		if( recv_len < 0 )
			debug_lr( KCOMM_DEBUG, "Error: recv\n%s",
				  strerror(errno));
		else
			debug_lr( KCOMM_DEBUG, "Error: recv zero bytes\n");
		goto failed;
	}

	
	/* Checking that we got 'm' */
	if( *((char*)(msgh.msg_iov[0].iov_base)) != KCOMM_AUTH_CHAR) {
		debug_lr( KCOMM_DEBUG, "Error: first char is not auth char\n");
		goto failed;
	}
    
	/* Receive auxiliary data in msgh */
	for( cmsg = CMSG_FIRSTHDR( &msgh );
	     cmsg != NULL;
	     cmsg = CMSG_NXTHDR( &msgh, cmsg ) ) {
		if( (cmsg->cmsg_level == SOL_SOCKET) &&
		    (cmsg->cmsg_type == SCM_CREDENTIALS )) {
			cred_ptr = (struct ucred *) CMSG_DATA(cmsg);
			received_cred = *cred_ptr;
			debug_lb( KCOMM_DEBUG, "Kcomm got cred\n" );
			break;
		}
	}
	
	if( cmsg == NULL ) {
		debug_lr( KCOMM_DEBUG, "Error: Did not get cred\n");
		goto failed;
	}
	
	else {
		debug_lg( KCOMM_DEBUG, "Cred are (pid %d) (uid %d) (gid %d)\n",
			  received_cred.pid,
			  received_cred.uid,
			  received_cred.gid);

		// Validate that the credentials are those of the root or
		// the same as of the running process 
		if( !( (received_cred.uid == 0) ||
		       (received_cred.uid == getuid()) )) {
			debug_lr( KCOMM_DEBUG, "Error: cred's not valid\n");
			goto failed;
		}
	}

	kcomm_status = KCOMM_UP;
	return 1;
    
 failed:
	kcomm_reset();
	return 0;
}

/*****************************************************************************
 * Modify the buffer so the extra data will be at the start
 *****************************************************************************/
inline int
unread_data( struct inpr_recv_data *data, int extra) {
	char *extra_start;

	if( extra <= 0 )
		return 0;
    
	extra_start = data->base + data->curr_len - extra;

	// moving the extra to the start of base
	memmove( data->base, extra_start, extra );
	data->curr_len = extra;
	data->ptr = data->base + extra;
    
	kcomm_is_there_extra = 1;
	return 1;
}

/*****************************************************************************
 * Read the message from the socket. If kcomm is in KCOMM_UP_NOT_AUTH_YET
 * state, then the only message we accept is the authentication message.
 * once we passed this stage any message can be received.
 * If the recv is not compleate then it is mark to indicate that not all the
 * message was received.
 *
 * First we read the magic (actually the type of the message).
 * Once we get it (and of course we can get it in parts) then we read
 * the rest of the message we know we should get and decide if we got it all
 * or we still need to wait for more.
 *
 * For example: we can get a REQUEST_MAGIC msg.
 * First we get the magic it 2 parts (each 2 bytes long)
 * 
 * After we got the magic we know that we need to get another int (number of
 * nodes) so we wait for another int. Once we get that int we know that we
 * need to wait for an array of int in the size we just got.
 *
 *****************************************************************************/

/****************************************************************************
 * magic is the message type,
 * buff  is the place for the message buffer
 * len   is the size of buff and is changed to the size of the data
 *
 * Return value: is 0 if we need to recv again. 1 is ok -1 is error
 *****************************************************************************/
int
kcomm_recv( int *magic, void *user_buff, int *user_len ) {
	int len, left;
	int buff_len;
	int extra = 0;
	int res = -1;

	// moving to inprogress recv mode
	if( !kcomm_inprrecv ) {
		kcomm_inprrecv = 1;
		inpr_recv_data.base = kcomm_buff;
		inpr_recv_data.max_len = KCOMM_BUFF_SZ;
		inpr_recv_data.ptr = inpr_recv_data.base;
		inpr_recv_data.curr_len = 0;
	}

	// Already in the middle of getting messages. 
	// Check if we have one ready (in case we read several
	// messages at once).
	else if( kcomm_is_there_extra ) {
		left = kcomm_mf.parse_msg( inpr_recv_data.base,
					   inpr_recv_data.curr_len,
					   &extra );
		if( left == -1 ) {
			debug_lr( KCOMM_DEBUG,"Error: in parse_msg_buff()\n" );
			kcomm_is_there_extra = 0;
			kcomm_inprrecv = 0;
			goto exit;
		}

		// Got a compleate msg without reading from the sock. 
		// Give the user its data
		else if( left == 0 ) {
			int msg_len = inpr_recv_data.curr_len - extra;
			memcpy( user_buff, inpr_recv_data.base, msg_len );
			*user_len = msg_len;
			*magic = ((unsigned long *)user_buff)[0];
			res = 1;

			// Remove the message from the buffer 
			if( extra ) 
				unread_data( &inpr_recv_data, extra );
			else
				kcomm_is_there_extra = 0;
			
			goto exit;
		}
	
	}

	// Compute the available buffer space
	buff_len = inpr_recv_data.max_len - inpr_recv_data.curr_len;

	// Perform the recv when we know that something is waiting on the sock
	// so len == 0 implies that the socket was closed
	len = recv( kcomm_curr_sockfd, inpr_recv_data.ptr,
		    buff_len, MSG_NOSIGNAL );

	if( len == 0 ) {
		debug_lr( KCOMM_DEBUG, "Error: recv == 0\n" );
		goto exit_and_close;
	}
		
	if( len == -1 ) {
		debug_lr( KCOMM_DEBUG, "Error: in recv\n%s\n",
			  strerror(errno));
		goto exit_and_close;
	}

	inpr_recv_data.curr_len += len;
	inpr_recv_data.ptr      += len;

	// Compute how much data we still need to read
    	left = kcomm_mf.parse_msg( inpr_recv_data.base,
				   inpr_recv_data.curr_len, &extra );

	if( left == -1 ) {
		debug_lr( KCOMM_DEBUG, "Error: how_much_left\n");
		kcomm_is_there_extra = 0;
		kcomm_inprrecv = 0;
		res = -1;
		goto exit;
	}
	
	else if( left > 0 )
		res = 0;
	else {
		int msg_len = inpr_recv_data.curr_len - extra;

		// Finished receiving
		if( msg_len > *user_len ) {
			debug_lr( KCOMM_DEBUG,
				 "Error: msg larger > user buff (%d) (%d)\n",
				  inpr_recv_data.curr_len, *user_len);
			
			// FIXME we must take care of extra data here.
			goto exit;
		}
	
		// Give the user its data
		memcpy( user_buff, inpr_recv_data.base, msg_len);
		*user_len = msg_len;
		*magic = ((unsigned long*)user_buff)[0];
		res = 1;
		
		// Unreading data: usually we read as much data as we can.
		// In normal operation we will read one message at a time.
		// But we might read several in one recv. For this we will
		// need to return 1 so the user will know that one message
		// is available of the buffer that was supplied but we need
		// to fix the inpr_recv_data to reflect that there is data
		// waiting on it.
		if( extra )
			unread_data(&inpr_recv_data, extra);
		else {
			// we go out of kcomm_inprrecv;
			kcomm_is_there_extra = 0;
			kcomm_inprrecv = 0;
		}
	}

 exit:
	return res;
    
 exit_and_close:
	kcomm_reset();
	return -1;
}

/****************************************************************************
 * Resize the 
 ***************************************************************************/
inline int
resize_kcomm_inpr_send_buff( int size ) {

	kcomm_inpr_send_buff_sz = size;
	if( !( kcomm_inpr_send_buff = realloc( kcomm_inpr_send_buff, size ))){
		debug_lr( KCOMM_DEBUG, "Error: realloc\n" );
		return 1;
	}
	return 0;
}

/*****************************************************************************
 * Send a message on the kcomm channell.
 *
 * If we are not in the middle of sending a message we try to send it.
 * If the send is partial then the unsent data is copied to a temporary
 * buffer which will be sent later (when kcomm_finish_send is called)
 *
 * If we are already in the middle of an actuall send then we add our message
 * to a heap of send messages.
 *
 ****************************************************************************/
int
kcomm_send( void *user_buff, int user_len ) {
	int res;

	if( kcomm_status != KCOMM_UP )
		return 0;
	if( !user_buff || !user_len )
		return 0;

	// Perform an actuall send
	if( !kcomm_inprsend ) {
		res = send( kcomm_curr_sockfd, user_buff, user_len,
			    MSG_NOSIGNAL );

		if( res == user_len )
			return res;
		else if( (res == -1) && (errno != EAGAIN)) {
			debug_lr( KCOMM_DEBUG, "Error: send\n%s\n",
				  strerror(errno));
			goto exit_with_reset;
		}
		
		// we need to continue this send later
		else {
			int len_left;
	    
			if( errno == EAGAIN )
				res = 0;
		
			len_left = user_len - res;

			// Resize the send buffer if needed
			if( len_left > kcomm_inpr_send_buff_sz )
				resize_kcomm_inpr_send_buff( len_left );
			
			kcomm_inprsend = 1;
			inpr_send_data.base    = kcomm_inpr_send_buff;
			inpr_send_data.max_len = kcomm_inpr_send_buff_sz;
			inpr_send_data.len     = len_left;
			
			memcpy( inpr_send_data.base, (char*)user_buff + res,
				len_left );
		}
	}
	
	// We add the data to the heap
	else {
		char *buff;
	
		if( heap_is_full( &kcomm_send_heap) ) {
			debug_lr( KCOMM_DEBUG, "Error: Heap is full\n");
			goto exit_no_reset;
		}

		// Allocate memory and copy the message so it can
		// be send later
		if( !( buff = malloc( user_len ) ) ) {
			debug_lr( KCOMM_DEBUG, "Error: malloc\n");
			goto exit_no_reset;
		}
		
		memcpy( buff, user_buff, user_len );

		// FIXME add priorities to the messages
		if( !heap_insert( &kcomm_send_heap, 1, buff, user_len ) ) {
			debug_lr( KCOMM_DEBUG, "Error: heap_insert failed\n");
			free( buff ) ;
			return 0;
		}
	}
	
	return 1;
    
 exit_no_reset:
	return 0;
	
 exit_with_reset:
	kcomm_reset();
	return 0;
}

/*****************************************************************************
 * Sending the max prio message from the queue.
 * Assuming that:
 *       1) there is something on the queue
 *       2) There is noting in inpr_send_data (we finished sending that)
 *
 * Try to send, if all is ok we return 1 if error -1 if partial send we
 * return 0 and update inpr_send_data to hold the rest of the message
 ****************************************************************************/
static int
send_queued_msg() {
	int  msglen, res;
	char *msg;

	heap_extract_max( &kcomm_send_heap, (void **)&msg, &msglen );
    	res = send( kcomm_curr_sockfd, msg, msglen, MSG_NOSIGNAL );
	
	if( res == msglen )
		return 1;
	
	// Bad error 
	else if( (res == -1) && (errno != EAGAIN)) {
		debug_lr( KCOMM_DEBUG, "Error: send\n%s\n",
			  strerror(errno));
		goto exit_with_reset;
	}
	
	// Partial send
	else {
		
		int len_left;
	    
		if( errno == EAGAIN )
			res = 0;
		
		len_left = msglen - res;

		// resize 
		if( len_left > inpr_send_data.max_len )
			resize_kcomm_inpr_send_buff( len_left );

		kcomm_inprsend = 1;
		inpr_send_data.base    = kcomm_inpr_send_buff;
		inpr_send_data.max_len = kcomm_inpr_send_buff_sz;
		inpr_send_data.len     = len_left;

		memcpy( inpr_send_data.base, msg + res, len_left );
	}
	
	// Free the message 
	free( msg );
	return 0;
	
 exit_with_reset:
	kcomm_reset();
	return 0;
}

/****************************************************************************
 * Finish a send which was previously started
 * If we still cant finish it we continue to try.
 ****************************************************************************/
int
kcomm_finish_send() {
	int res;
    
	if( kcomm_status != KCOMM_UP )
		return 0;
	
	if( !kcomm_inprsend ) {
		debug_lr( KCOMM_DEBUG, "kcomm_finish_send() called "
			  "while not in inpr_send\n");
		return 0;
	}
    
	// we send the inpr_send_data
	res = send( kcomm_curr_sockfd, inpr_send_data.base,
		    inpr_send_data.len, MSG_NOSIGNAL);
	
	if( res == inpr_send_data.len ) {
		kcomm_inprsend = 0;
		
		// We should check if we have more to send and if so send it
		while( !heap_is_empty( &kcomm_send_heap ) ) {
			if( ( res = send_queued_msg()) == 1 )
				continue;
			else if( res == -1 ) 
				goto exit_with_reset;
			else 
				break;
		}
	}
	
	// We have a problem on the channel so we reset
	else if( (res == -1) && ( errno != EAGAIN)) 
		goto exit_with_reset;
	
	// Ok, we sent some data but not all
	else {
		inpr_send_data.len -= res;
		memmove( inpr_send_data.base, inpr_send_data.base + res,
			 inpr_send_data.len );
		kcomm_inprsend = 1;
	}

	return 1;

 exit_with_reset:
	kcomm_reset();
	return(0);
}

/****************************************************************************
 * Sending the kernel SPEEDCHG message telling it to change its speed.
 * This might happend due to a mosctl request from the infod.
 * The infodctl will use this function to tell the kernel to change its speed
 * It the kernel doesnt support speed changes it should ignore this message
 ***************************************************************************/
int
kcomm_send_speed_change( unsigned long new_speed ) {
	unsigned long msg[2];

	msg[0] = SPEEDCNG_MAGIC;
	msg[1] = new_speed;
    
	return kcomm_send( msg, sizeof(msg));
}

/****************************************************************************
 * Sending the kernel CONSTCHG message telling it to change its constants
 * This will happend due to a mosctl request from the infod. The infodctl
 * subsystem will forward the message to the kernel
 ***************************************************************************/
int
kcomm_send_cost_change( unsigned long ntopology, unsigned long *costs ) {
	char *msg_buff;
	struct costcng_msg *msg;
	int len, res;
    
	len = sizeof(struct costcng_msg) +
		ntopology * ALL_TUNE_CONSTS * sizeof(unsigned long);
    
	if( !( msg_buff = malloc(len) ) )
		return 0;
	
	msg = (struct costcng_msg *) msg_buff;
	
	msg->magic = CONSTCNG_MAGIC;
	msg->ntopology = ntopology;
	memcpy(msg->costs, costs,
	       ntopology * ALL_TUNE_CONSTS * sizeof(unsigned long));

	res = kcomm_send(msg, len);
	free(msg_buff);
	return res;
}

/****************************************************************************
 * Wrapper functions for the specific module
 ***************************************************************************/

/*
 * Printing the received message for debugging
 */
void
kcomm_print_msg( void *msg, int len ) {
	(*kcomm_mf.print_msg)( msg, len );
}

/*
 * Handle the received message (and responding to it if necessary)
 */
int
kcomm_handle_msg( unsigned long magic, char *req_buff, int req_len,
		  ivec_t vec) {
	return (*kcomm_mf.handle_msg)( magic, req_buff, req_len, vec );
}

/*
 * Periodic admin of kernel frequency request
 */
int
kcomm_periodic_admin( ivec_t vec ) {
	if( !kcomm_is_up())
		return 0;
	return (*kcomm_mf.periodic_admin)( vec );
}

/*
 * Periodic admin of kernel frequency request
 */
int
kcomm_get_stats( ivec_t vec, infod_stats_t *stats ) {
	if( !kcomm_is_up())
		return 0;
	return (*kcomm_mf.get_stats)( vec, stats );
}

/*
 * Get the information description.
 */
char*
kcomm_get_infodesc() {
	return (*kcomm_mf.get_infodesc)();
}

/*
 * Get the size of the information
 */
int
kcomm_get_infosize() {
	return (*kcomm_mf.get_infosize)();
}

/*
 * Get the local information from the kernel
 */
int
kcomm_get_info(void *buff, int size) {
	return (*kcomm_mf.get_info)(buff,size);
}

/*
 * Set the local pe
 */
void
kcomm_set_pe(unsigned long pe) {
	if(kcomm_mf.set_pe) (*kcomm_mf.set_pe)(pe);
}

/*
 * Set the map change handler
 */
void
kcomm_set_map_change_hndl(map_change_hndl_t hndl) {
	if(kcomm_mf.set_map_change_hndl)
		(*kcomm_mf.set_map_change_hndl)(hndl);
}

/****************************************************************************
 *                               E O F
 ***************************************************************************/
