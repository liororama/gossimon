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

/******************************************************************************
 *    File: infod.c
 *****************************************************************************/
#ifndef __USE_GNU
#define __USE_GNU
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <signal.h>
#include <errno.h>

#include <netinet/in.h>
#include <netdb.h>   
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sched.h>

#include <site_protection.h>
#include <msx_common.h>
#include <msx_proc.h>
#include <msx_debug.h>
#include <msx_error.h>
#include <easy_args.h>
#include <pe.h>
#include <Mapper.h>
#include <MapperBuilder.h>
#include <ColorPrint.h>
#include <ModuleLogger.h>

#include <info.h>
#include <info_reader.h>
#include <info_iter.h>

#include <infoVec.h>
#include <infod.h>
#include <comm.h>
//#include <distance_graph.h>

//#include <distance_graph.h>
#include <provider.h>
#include <ctl.h>
#include <infodctl.h>
#include <gossip.h>


#define MAX_STATS_STR_LEN             (4096)
#define KCOMM_BUFF_SIZE               (4096)

#ifdef INFOD2
#define MSX_INFOD_SCHED_PR            (70)
#else
#define MSX_INFOD_SCHED_PR            (10)
#endif

/****************************************************************************
 * setup and stop functions
 ***************************************************************************/
int  usage(void *void_str);
int  infod_init( int argc, char** argv );
void infod_random_sleep();
int  map_setup();
int  comm_setup();
int  general_setup();
int  info_setup();
int  info_stop();
int  init_data_structures( int init_comm_flg );
int  resolve_my_ip(struct in_addr *myIP);
void doTimeStep();
void infod_periodic_admin();

/****************************************************************************
 * Functions for local information
 ***************************************************************************/

int    read_local_info();
int    infod_reply_client( ivec_entry_t** ivecptr, int size,
			   comm_inprogress_recv_t* comm_msg );

/****************************************************************************
 * Message handling functions
 ***************************************************************************/

void handle_msg( msx_comm_t *comm, mapper_t map,
		 comm_inprogress_recv_t* comm_msg ) ;

int handle_info( msx_comm_t *comm, mapper_t map,
		 comm_inprogress_recv_t* comm_msg );

int handle_info_pull( msx_comm_t *comm, mapper_t map,
		 comm_inprogress_recv_t* comm_msg );

int handle_client_request( msx_comm_t *comm, mapper_t map,
			   comm_inprogress_recv_t* comm_msg );

int handle_ginfod( comm_inprogress_recv_t* comm_msg );

/****************************************************************************
 * Util functions
 ***************************************************************************/
void    adjust_time( struct timeval *cur, struct timeval *time );
node_t* infod_names2pes( char *names, mapper_t map,  int *size );
int     infod_pes2names( char *buff );
char   *get_infod_uptime_str();

int prepare_select_data(fd_set *rfds, fd_set *wfds, fd_set *efds, int *max_sock);
int handle_sighup();
int handle_select_error();

int handle_kcomm_event(fd_set *rfds, fd_set *wfds, fd_set *efds);
int handle_ctl_event(fd_set *rfds, fd_set *wfds, fd_set *efds);
int handle_comm_event(fd_set *rfds, fd_set *wfds, fd_set *efds);



/****************************************************************************
 * Signal and timer handling functions
 ***************************************************************************/
void sig_alarm_hndl( int sig );
void sig_hup_hndl( int sig );
void sig_int_hndl( int sig );
void sig_segv_hndl( int sig );
void sig_abrt_hndl( int sig );
void sig_term_hndl( int sig );

int install_timer();
int uninstall_timer();

inline int  block_sigalarm();
inline int  unblock_sigalarm();



/***************************************************************************
 *  Communication functinos
 ***************************************************************************/
int doGossipStep();
int receive_info( fd_set *set, comm_inprogress_recv_t* comm_msg );
int send_inpr_info( fd_set *set, struct in_addr *ip );

int comm_finish_send_mosix( msx_comm_t *comm, mapper_t map,
			    fd_set *set, struct in_addr *ip);
int comm_send_mosix( msx_comm_t *comm, struct in_addr *ip, unsigned short port,
		     void *buff, int type, int size, int mv2recv);

/***************************************************************************
 * Ctl handling
 ***************************************************************************/
static int   do_ctl_action( infodctl_action_t *action );

/****************************************
 * Map change message handler
 ****************************************/
void map_change_hndl();


/***************************************************************************
 * Global data 
 **************************************************************************/ 
infod_cmdline_t globOpts;

mapper_t        glob_msxmap    = NULL;       
msx_comm_t     *glob_msxcomm = NULL;
ivec_t          glob_vec     = NULL;

struct in_addr  glob_IP;
unsigned int    glob_timeSteps = 0;
unsigned short  glob_infod_port        = MSX_INFOD_DEF_PORT ;
unsigned short  glob_infod_lib_port    = MSX_INFOD_LIB_DEF_PORT ; 
unsigned short  glob_infod_ginfod_port = MSX_INFOD_GINFOD_DEF_PORT ;

int             glob_got_sighup = 0  ;
int             glob_got_sigalarm = 0;
char           *glob_kcomm_module_name = NULL;
int             glob_kernel_topology = 1;

/*
 * Define the pid of of the working infod
 */
pid_t           glob_working_pid = 0;
pid_t           glob_parent_pid  = 0;
int             glob_daemonMode = 0;
void           *glob_gossip_data = NULL;

/****************************************************************************
 * Global buffers and their sizes
 ***************************************************************************/
int             global_buffer_size = 0 ;
void           *global_buffer = NULL;

int             global_kcomm_buffer_size = 0;
void           *global_kcomm_buffer = NULL;

int             glob_local_info_size = 0;
void           *glob_local_info_buff = NULL;
node_info_t    *glob_local_info;

int             glob_desc_size = 0 ;
char           *glob_local_desc = NULL;

variable_map_t *glob_mapping = NULL;

/* determines if the infod should work in quiet mode or not */ 
int             glob_quiet_mode = 0;

/* The yardstick for mon and others who need to calculate load */
int             glob_yardstick = 0;

/* Do exit, got sig term and we should go out */
int             glob_infodExit = 0;

struct infod_runtime_info  runTimeInfo;


/*****************************************************************************
 * Main:
 ****************************************************************************/ 
int
main( int argc, char** argv ) {
	
	int res ;

	/* initiate the daemon */ 
	infod_init( argc, argv );
	infod_random_sleep();

	// Main loop
	while(1) {
                
	     fd_set rfds, wfds, efds;
	     int max_sock;
	     
	     block_sigalarm();
	     
	     // Checking if we need to exit
	     if(glob_infodExit)
		  infod_exit();
	     
	     prepare_select_data(&rfds, &wfds, &efds, &max_sock);
	     unblock_sigalarm();
	     errno = 0;
	     
	     int timeStep = globOpts.opt_timeStep;
	     struct timeval timeout;
	     timeout.tv_sec = timeStep / 1000;
	     timeout.tv_usec = (timeStep % 1000) * 1000;
	     res = select( max_sock + 1, &rfds, &wfds, &efds, &timeout );
	     
	     // Handling a SIGHUP received 
	     if( glob_got_sighup ) {
		  handle_sighup();
		  continue;
	     }

	     // An error occured on the socket 
	     if( res == -1 ) {
		  handle_select_error();
		  continue;
	     }

	     if( handle_kcomm_event(&rfds, &wfds, &efds) ||
		 handle_ctl_event(&rfds, &wfds, &efds)   ||
		 handle_comm_event(&rfds, &wfds, &efds)) {
		  debug_lb(INFOD_DEBUG, "handled event\n");
	     }
	     
	     comm_admin( glob_msxcomm, 1 );
	     kcomm_periodic_admin( glob_vec );
	     infod_periodic_admin();
	}

	comm_close( glob_msxcomm );
	return 0;
}

int prepare_select_data(fd_set *rfds, fd_set *wfds, fd_set *efds, int *max_sock) {
     
     FD_ZERO( rfds );
     FD_ZERO( wfds );
     FD_ZERO( efds );
     
     /*
      * The infod is not supposed to be listening  in quiet mode
      * or when there is no valid, initialized map object
      */
     if( !glob_quiet_mode && glob_msxmap )
	  comm_get_listening_fdset( glob_msxcomm, rfds );
     
     comm_get_listening_fdset( glob_msxcomm, rfds );
     comm_get_inprogress_recv_fdset( glob_msxcomm, rfds );
     infodctl_get_fdset( rfds );
     kcomm_get_fdset( rfds, wfds, efds );
		
     /* Initializing the wfds: inprogress send sockets. */
     comm_get_inprogress_send_fdset( glob_msxcomm, wfds );

     /* Initializeing the efds: the sockets that should be close */
     comm_get_close_fdset( glob_msxcomm, efds );
     
     /* compute the minimum file descriptor */ 
     *max_sock = comm_get_max_sock( glob_msxcomm );
     int res = infodctl_get_max_sock();
     if( res > *max_sock )
	  *max_sock = res;
     *max_sock = kcomm_get_max_sock(*max_sock);
     
     return 1;
}

int handle_sighup() {
     
     glob_got_sighup = 0;
     
     infod_log(LOG_INFO, "Got SIGHUP\n" );
     
     /* first try to re-initiate the map */ 
     if( map_setup()) {
	  infod_log(LOG_INFO, "Reloaded map\n" );
	  
	  info_stop();
	  info_setup();
     } 
     return 1;
}

int handle_select_error() {
     if( (errno != EINTR) && (errno != EINPROGRESS)) {
	  comm_admin( glob_msxcomm, 0 );
	  infod_log( LOG_ERR, "Error: in select()\n" );
     }
     kcomm_periodic_admin( glob_vec );
     return 1;
}

int handle_kcomm_event(fd_set *rfds, fd_set *wfds, fd_set *efds)
{
     int res;
     
     if( kcomm_is_new_connection( rfds )) {
	  if( !kcomm_setup_connection() ) {
	       infod_log( LOG_ERR, "Error: setup new kcomm connection\n");
	  }
     }
     
     /* a new request has arrived */
     else if( kcomm_is_req_arrived( rfds )) {
	  if( !kcomm_is_auth() ) {
	       debug_lg( KCOMM_DEBUG, "Getting kcomm auth\n");
	       if(!kcomm_recv_auth())
		    debug_lr( KCOMM_DEBUG,
			      "kcomm auth failed\n");
	  }
	  
	  else {
	       int magic;
	       int req_len = global_kcomm_buffer_size;
	       
	       res = kcomm_recv( &magic,
				 global_kcomm_buffer,
				 &req_len);
	       if( res == -1 ) {
		    //debug_lr( KCOMM_DEBUG,
		    //  "Error: kcomm_recv\n");
	       }
	       
	       else if( res == 0 ) {
		    //debug_lr( KCOMM_DEBUG,
		    //  "Kcomm got partial message");
	       }
	       else {
		    debug_lg( KCOMM_DEBUG,
			      "Kcomm message %d %x\n",
			      req_len, magic);
		    kcomm_print_msg( global_kcomm_buffer,
				     req_len);
		    kcomm_handle_msg( magic,
				      global_kcomm_buffer,
				      req_len, glob_vec);
	       }
	  }
     }
     
     else if( kcomm_is_ready_to_send( wfds )) {
	  if(!kcomm_finish_send()) {
	       //debug_lr( KCOMM_DEBUG,
	       //  "Error: kcomm finish send\n");
	  }
     }
     
     else if(kcomm_exception( efds ) ) {
	  // debug_lr( KCOMM_DEBUG, "Kcomm got exception\n");
	  kcomm_reset();
     }
     else {
	  return 0;
     }
     
     return 1;
}
int handle_ctl_event(fd_set *rfds, fd_set *wfds, fd_set *efds)
{
     int res;
     
     if( infodctl_is_new_connection( rfds )) {
	  debug_lg( CTL_DEBUG, "New CTL connection\n" );
	  res = infodctl_setup_connection();
     }
     
     /* A ctl request has arrived */
     else if( infodctl_req_arrived( rfds )) {
	  infodctl_action_t action;
	  
	  debug_ly( CTL_DEBUG, "CTL request arrived\n");
	  if( !infodctl_recv( &action ))
	       debug_lr( CTL_DEBUG,
			 "Error: get ctl request\n");
	  else
	       do_ctl_action( &action );
     }
     else {
	  return 0;
     }
     return 1;
}

int handle_comm_event(fd_set *rfds, fd_set *wfds, fd_set *efds)
{
     int            res;
     struct in_addr ip;

     /* maybe we've got a new connection pending */
     if( comm_is_new_connection( glob_msxcomm, rfds )){
	  debug_ly( INFOD_DEBUG, "Setting up new connection\n");
	  res = comm_setup_connection( glob_msxcomm, rfds,(char *) &ip );
	  
	  if( res == -1 )
	       debug_lr( INFOD_DEBUG,
			 "Error: setting new connection\n");
     }
     
     /* maybe one of the write fd's is ready */ 
     else if( comm_inprogress_send_active( glob_msxcomm, wfds )){
	  debug_lg( INFOD_DEBUG, "Sending inprogress information\n");
	  send_inpr_info( wfds, &ip );
     }
     
     /* maybe one of the read fd's is ready */
     else if( comm_inprogress_recv_active( glob_msxcomm, rfds )){
	  comm_inprogress_recv_t comm_msg;
	  bzero( &comm_msg, sizeof( comm_inprogress_recv_t ));
	  
	  debug_lg( INFOD_DEBUG, "Receiving inprogress information\n");
	  
	  if ( ( res = receive_info( rfds, &comm_msg ) ) == 1 )
	       handle_msg( glob_msxcomm, glob_msxmap,
			   &comm_msg );
     }
     
     /* one of the exception fd's is ready */ 
     else if( !(comm_close_on_socket( glob_msxcomm, efds ))) {
	  //debug_lr( INFOD_DEBUG, "Error, out of select. Why???\n" );
     }
     else {
	  return 0;
     }
     
     return 1;
}
		 

/*****************************************************************************
 * Run as a realtime process
 ****************************************************************************/
int
run_real_time() {
	struct sched_param p;
	//char str[] = "1\n";
	/* lock all pages in memory and disable paging */
	if( mlockall( MCL_CURRENT | MCL_FUTURE ) < 0 )
		infod_critical_error( "Error: mlockall.\n");
    
	p.sched_priority = MSX_INFOD_SCHED_PR ; 
	/* set the priority to real time priority */
	sched_setscheduler( 0, SCHED_RR, &p );

	return 1;
}

/****************************************************************************
 * Performing general setup. Set up variables and data that will consistent
 * throughout the operation of the infod
 * 1. Set limits and current working dierctory
 * 2. Running as a real time process
 ****************************************************************************/
int
general_setup() {
	
	struct timeval tim;
	struct rlimit limitcore = { RLIM_INFINITY, RLIM_INFINITY };
    
	/* initialize logging */
	log_init();
	infod_log( LOG_INFO, "Initalizing infod\n" );

	infod_log(LOG_INFO, "module: %s", glob_kcomm_module_name);
	infod_log(LOG_INFO, "map: %s", globOpts.opt_mapBuff);
	infod_log(LOG_INFO, "IP: %s", inet_ntoa(globOpts.opt_myIP));
	
	/* change the current working directory */
	if( chdir( INFOD_CWD ) < 0 )
		infod_critical_error( "Error: chdir current working dir\n" );
	
	/* remove core file if exists */
	char path[256];
	sprintf(path, "%s/%s", INFOD_CWD, INFOD_NATIVE_CORE_FILE);
	unlink(path);
	
	/* set the core dump size to unlimited */
	if( setrlimit( RLIMIT_CORE, &limitcore ) <  0)
		infod_critical_error( "Error: setting coredumpsize limit\n");
	
	/*  run as a real-time process */ 
	if( getuid() == 0) 
		run_real_time();

	/* Initilizing random seed */
	gettimeofday(&tim, NULL);
	srand( tim.tv_usec);

        if(!createPidFile()) {
//             infod_log(LOG_ERR, "Error creating infod pid file");
             infod_critical_error("Error creating pid file\n");
        }
        return 1;
}

/*****************************************************************************
 * Setting up communication channels:
 * 1. msx_comm       (tcp_ip). Different ports are used in order to prioritize
 *                   the messages.
 * 2. ctl            (unix sock)
 * 3. kcomm          (unix sock)
 *****************************************************************************/
int
comm_setup() {
	unsigned short ports[] = { glob_infod_port,
				   glob_infod_lib_port,
				   glob_infod_ginfod_port,
				   0,
				   0 } ;

	// Currently we take 2 arguments so we need 4 places
	char *args[60];
	int i = 0;
	
	/* setting ctl subsystem */
	if( infodctl_init( MSX_INFOD_CTL_SOCK_NAME ) == -1)
		infod_critical_error("Error: initilizing ctl subsystem\n");
	infod_log(LOG_INFO, "Initiated control channel\n" );
    
	/* initiate the msx_comm_t object */
	for( i = 0 ; i <= MSX_INFOD_DEF_BIND_GIVEUP; i++) {
	     if(( glob_msxcomm = comm_init( ports,
					    MSX_INFOD_DEF_COMM_TIMEOUT ))) {
		  break;
	     }
	     else {
		  if(i == 0 )
		       infod_log(LOG_INFO, "Error while initializing comm (probably in bind), retrying\n");
		  sleep(1);
	     }
	}
	
	if( i > MSX_INFOD_DEF_BIND_GIVEUP )
             infod_critical_error( "Error: Initiating communicator, can not bind to port\n");
        
	infod_log(LOG_INFO, "Initiated communication\n" );

	/* Initilazing kcomm  */
	if(!( global_kcomm_buffer = malloc( KCOMM_BUFF_SIZE )))
		infod_critical_error( "Error: Initiating kcomm buffer\n" );
	global_kcomm_buffer_size = KCOMM_BUFF_SIZE;

	{
		int argc = 0;
		char tmpbuf[30];
		sprintf(tmpbuf, "%d", glob_kernel_topology);
		args[argc++] = "MAX_MOSIX_TOPOLOGY";
		args[argc++] = strdup(tmpbuf);

                if(globOpts.opt_providerWatchFs) {
			args[argc++] = "WATCH_FS";
			args[argc++] = globOpts.opt_providerWatchFs;
		}
                if(globOpts.opt_providerInfoFile) {
			args[argc++] = "INFO_FILE";
			args[argc++] = globOpts.opt_providerInfoFile;
		}
                if(globOpts.opt_providerProcFile) {
			args[argc++] = "PROC_FILE";
			args[argc++] = globOpts.opt_providerProcFile;
		}
		if(globOpts.opt_providerEcoFile) {
			args[argc++] = "ECO_FILE";
			args[argc++] = globOpts.opt_providerEcoFile;
		}
		if(globOpts.opt_providerJMigFile) {
			args[argc++] = "JMIG_FILE";
			args[argc++] = globOpts.opt_providerJMigFile;
		}
		if(globOpts.opt_providerWatchNet) {
			args[argc++] = ITEM_NET_WATCH_NAME;
			args[argc++] = globOpts.opt_providerWatchNet;
		}

		
                args[argc++] = "INFOD_DEBUG";
                args[argc++] = (char *) &runTimeInfo;

		args[argc++] = "CONFIG_FILE";
                args[argc++] = globOpts.opt_confFile;

		
		args[argc++] = "PROVIDER_TYPE";
		switch (globOpts.opt_providerType) {
		    case INFOD_EXTERNAL_PROVIDER:
			 args[argc++] = "external";
			 break;
		    case INFOD_LP_LINUX:
			 args[argc++] = "linux";
			 break;
		    case INFOD_LP_MOSIX:
		    default:
                         args[argc++] = "mosix";
                         break;
		}
		// The default provider to use is the mosix builtin one (which has
		// support for noprovider mode)
		if( glob_kcomm_module_name == NULL )
		     glob_kcomm_module_name = strdup( INFOD_INTERNAL_PROVIDER_MODE );
		
		if( !kcomm_init( glob_kcomm_module_name,
				 globOpts.opt_providerType != INFOD_EXTERNAL_PROVIDER ?
				 NULL : MSX_INFOD_KCOMM_SOCK_NAME,
				 MSX_INFOD_KCOMM_MAX_SEND,
				 argc, args ))
		     infod_critical_error( "Error: kcomm_init failed" );
		kcomm_set_map_change_hndl(map_change_hndl);
		infod_log(LOG_INFO, "Initiated kcomm\n" );
	}
	return 1;
}

/*****************************************************************************
 Obtaining the cluster configuration (possibilities):
 1) Regular map file (linux mode, mosix1 mode)
 2) Map retrival command
 3) Calling setpe
****************************************************************************/
int
map_setup()
{
	mapper_t new_map = NULL;
	char     *buff = globOpts.opt_mapBuff;
	int       len  = globOpts.opt_mapBuffSize;
	int       inputType = globOpts.opt_mapSourceType;
	
	switch(globOpts.opt_mapType)
	{
	    case INFOD_USERVIEW_MAP:
		    if(!(new_map = BuildUserViewMap(buff, len, inputType))) {
			    infod_log(LOG_ERR,
				      "Failed initalizing userview map: %s\n",
				      buff);
			    goto failed;
		    }
		    break;
	    default:
		    infod_log(LOG_ERR, "Unsupported map type\n" );
		    goto failed;
	}

	/* We managed to get a correct map object */
	mapperDone( glob_msxmap );
	glob_msxmap = new_map;

	if(!mapperSetMyIP(glob_msxmap, &globOpts.opt_myIP)) {
		infod_log(LOG_ERR, "Error: My ip %s is not in map",
			  inet_ntoa(globOpts.opt_myIP));
		goto failed;
	}
	globOpts.opt_myPE = mapperGetMyPe(glob_msxmap);
	
	debug_lg( INFOD_DEBUG, "My IP: %s\n", inet_ntoa(globOpts.opt_myIP) );
	if(globOpts.opt_debug)
		mapperFPrint(stderr, glob_msxmap, PRINT_TXT);
	globOpts.stat_mapOk = 1;
	return 1;

 failed:
	globOpts.stat_mapOk = 0;
	return 0;
}

/****************************************************************************
 * Handle all the signals: SIGHUP, SIGALRM, SIGINT, SIGSEGV, SIGALRM,
 *                         SIGSEGV.
 ***************************************************************************/
int
signal_setup() {

	struct sigaction act;

        /* setting the signal handler of SIGALRM */
	act.sa_handler = sig_alarm_hndl ;
	sigemptyset ( &( act.sa_mask ) ) ;
	sigaddset ( &( act.sa_mask) , SIGHUP ) ;
	act.sa_flags = 0; //SA_RESTART ;
        
	if ( sigaction ( SIGALRM, &act, NULL ) < -1 )
		infod_critical_error( "Error: sigaction on SIGALRM\n" ) ;
        
	/* setting the signal handler of SIGHUP */
	act.sa_handler = sig_hup_hndl ;
	sigemptyset ( &( act.sa_mask ) ) ;
	sigaddset ( &( act.sa_mask) , SIGALRM ) ;
	act.sa_flags = SA_RESTART ;
    
	if ( sigaction ( SIGHUP, &act, NULL ) < -1 )
		infod_critical_error( "Error: sigaction on SIGHUP\n" ) ;

	/* setting the signal handler of SIGINT */
	act.sa_handler = sig_int_hndl ;
	sigfillset ( &( act.sa_mask ) ) ;
	act.sa_flags = SA_RESTART ;

	if ( sigaction ( SIGINT, &act, NULL ) < -1 )
		infod_critical_error( "Error: sigaction on SIGINT\n" ) ;

	/* setting the signal handler of SIGTERM */
	act.sa_handler = sig_term_hndl ;
	sigemptyset ( &( act.sa_mask ) ) ;
	act.sa_flags = SA_RESTART ;

	if ( sigaction ( SIGTERM, &act, NULL ) < -1 )
          infod_critical_error( "Error: sigaction on SIGTERM\n" ) ;
	siginterrupt( SIGALRM, 0 );

        /* setting the signal handler of SIGSEGV */
	/* act.sa_handler = sig_segv_hndl ; */
/* 	sigemptyset ( &( act.sa_mask ) ) ; */
/* 	act.sa_flags = SA_RESTART ; */

/* 	if ( sigaction ( SIGSEGV, &act, NULL ) < -1 ) */
/* 		infod_critical_error( "Error: sigaction on SIGSEGV\n" ) ; */
	
        /* setting the signal handler of SIGABRT */
	/* act.sa_handler = sig_abrt_hndl ; */
/* 	sigemptyset ( &( act.sa_mask ) ) ; */
/* 	act.sa_flags = SA_RESTART ; */
        
/* 	if ( sigaction ( SIGABRT, &act, NULL ) < -1 ) */
/* 		infod_critical_error( "Error: sigaction on SIGABRT\n" ) ; */
        
	return 1;
}

/****************************************************************************
 * Allcate the buffer for the local information 
 ***************************************************************************/
void
allocate_local_info_buffer() {

	if( glob_local_info_buff ) {
		free( glob_local_info_buff );
		glob_local_info_buff = NULL;
	}
	
	if( !(glob_local_info_buff = malloc( glob_local_info_size )))
		infod_critical_error( "Error: malloc\n" );
	
	glob_local_info = (node_info_t*)(glob_local_info_buff);
	bzero( glob_local_info_buff, glob_local_info_size );
}


int autoCalcWindowSize(int n) {

     int w;
     if(globOpts.opt_desiredAvgAge) {
          debug_lg(INFOD_DEBUG, "Auto calc window: n: %d, desired avgage %.3f\n",
                   n, globOpts.opt_desiredAvgAge);
          w = calcWinsizeGivenAv(n, globOpts.opt_desiredAvgAge);
     }
     else if(globOpts.opt_desiredAvgMax) {
          debug_lg(INFOD_DEBUG,
		   "Auto calc window: n: %d, desired avgmax %.3f\n",
                   n, globOpts.opt_desiredAvgMax);
          w = calcWinsizeGivenMax(n, globOpts.opt_desiredAvgMax);
     }
     else if(globOpts.opt_desiredUptoAge) {
	     debug_lg(INFOD_DEBUG,
		      "Auto calc window: n: %d, entries %d age %.3f\n",
		      n, globOpts.opt_desiredUptoEntries,
		      globOpts.opt_desiredUptoAge);
          w= calcWinsizeGivenUptoageEntries(n,
                                            globOpts.opt_desiredUptoAge,
                                            globOpts.opt_desiredUptoEntries);
     }
     
     if(w > 0) {
          globOpts.opt_winParam = w;
          globOpts.opt_winType = INFOVEC_WIN_FIXED;
          infod_log(LOG_INFO, "Window size autoconfigured to %d (n=%d)\n",
		    w, n);
     }
     else {
          infod_log(LOG_ERR,
		    "Faild to autoconfigure window size. Using default %d\n",
		    INFOD_DEF_WINSIZE);
          
          globOpts.opt_winParam = INFOD_DEF_WINSIZE;
          globOpts.opt_winType = INFOVEC_WIN_FIXED;
     }
     
     return 0;
}

/*****************************************************************************
 * Initiate all the data structures
 ****************************************************************************/
int
info_setup() {
	int num = 0;

	switch (globOpts.opt_gossipDistance) {
	    case GOSSIP_DIST_CLUSTER:
		    num = mapperClusterNodeNum(glob_msxmap); break;
	    case GOSSIP_DIST_GRID:
		    num = mapperTotalNodeNum(glob_msxmap); break;
	    default:
		    infod_critical_error( "Error unsupported gossip distance");
	}
//        msx_set_debug(INFOD_DEBUG);
	debug_lg(INFOD_DEBUG, "Gossip distance: %d\n", globOpts.opt_gossipDistance);
	debug_lg(INFOD_DEBUG, "Total node number %d\n", num);


	if( globOpts.opt_maxAge == 0 ) {
		int timeScale = globOpts.opt_timeStep/(1000);
		if(timeScale == 0)
			timeScale = 1;
		globOpts.opt_maxAge = 4 * num * MILLI * timeScale;
                debug_lg(INFOD_DEBUG, "num %d MILLI %d  timeScale %d step %d\n", 
                         num, MILLI, timeScale, globOpts.opt_timeStep);
	}
	infod_log(LOG_INFO, "Vector Max age: %d\n", globOpts.opt_maxAge);
        glob_local_info_size = MSX_INFOD_MAX_ENTRY_SIZE;
        allocate_local_info_buffer();
	
	/* Read the description of the information */
	if(!(glob_local_desc =  kcomm_get_infodesc()))
             infod_critical_error( "Error: getting description\n" );
	glob_desc_size = strlen( glob_local_desc ) + 1;
        
	/* create the map of the data, holding information of a single node */
	if( !(glob_mapping = create_info_mapping( glob_local_desc )))
		infod_critical_error( "Error: creating mapping from desc\n" );

	/* Initialize the global buffer */
	global_buffer_size =  (num + 10) * (3*glob_desc_size) + 4096;
	if( !(global_buffer =  malloc( global_buffer_size )))
		infod_critical_error("Error: malloc, global buffer\n" );

        /* Trying to autoconfigure window size */
        if(globOpts.opt_winAutoCalc) 
             autoCalcWindowSize(num);

	/* Initiate the vector */
	if( !( glob_vec = infoVecInit( glob_msxmap, globOpts.opt_maxAge,
				       globOpts.opt_winType,
				       globOpts.opt_winParam,
				       glob_local_desc,
                                       1)))
		infod_critical_error( "Error: Initiating infovec\n" );
	
	infod_log(LOG_INFO, "Initiated info vector\n" );
	
	/* update the vector */
	//infoVecUpdate( glob_vec, glob_local_info, glob_local_info_size, 0 ); 

	/* Setting the local node as dead until the mosixd connect to infod */
	infoVecPunish( glob_vec, &globOpts.opt_myIP, INFOD_DEAD_PROVIDER ) ;
		
	return 1;
}

/*****************************************************************************
 * Perform reconfiguration of the data-structures
 * FIXME maybe we need to flush/clear msxcomm as well
 ****************************************************************************/
int
info_stop() {

	/* free the global buffer */
	if( global_buffer ) {
		free( global_buffer );
		global_buffer = NULL;
		global_buffer_size = 0;
	}

	/* free the local description  */
	if( glob_local_desc ){
 		free( glob_local_desc );
	 	glob_local_desc = NULL;
		glob_desc_size = 0;
	}
	
 	if( glob_mapping ) {
	 	destroy_info_mapping( glob_mapping );
		glob_mapping = NULL;
	}

	/* clear the vector */
	if(glob_vec)
             infoVecFree( glob_vec ) ;
	glob_vec = NULL;
	return 1;
}

/*******************************************************************
 * Setting up the selected gossip algorithm. This means running the
 * init function
 ******************************************************************/
int gossip_setup()
{
	if(!globOpts.opt_gossipAlgo) {
		infod_critical_error( "Error: no gossip algorithm selected\n");
	}

	// Running the init function
	(*globOpts.opt_gossipAlgo->initFunc)(&glob_gossip_data);
	infod_log(LOG_INFO, "Initiated gossip algorithm: %s\n",
	       globOpts.opt_gossipAlgo->algoName );
	return 1;
}



/*****************************************************************************
 * Init function
 ****************************************************************************/
int
infod_init( int argc, char** argv){
	infod_cmdline_t opts;

        mlog_init();
        mlog_addOutputFile("/dev/tty", 1);
        mlog_addOutputSyslog("INFOD", LOG_PID, LOG_DAEMON);
        mlog_registerColorFormatter((color_formatter_func_t)sprintf_color);
	
 	initOptsDefaults(&opts);
	parseInfodCmdLine(argc, argv, &opts);
	globOpts = opts;

        infod_log(LOG_INFO, "------- Starting : version %s -------\n", GOSSIMON_VERSION_STR);
        
	bzero(&runTimeInfo, sizeof(struct infod_runtime_info));

        if(pidFileExists()) {
          infod_critical_error("!! Error pid file %s already exists\n"
                               "   Make sure there is no other daemon running\n",
                               INFOD_DEF_PID_FILE);
        }


	// Setting the map before all. So we can report problem before infod
	// becomes a daemon
	int mapOK =0;
	if( map_setup()) {
	  infod_log(LOG_INFO, "cluster map setup complete");
	  mapOK = 1 ;
	}
	else {
	     if(globOpts.opt_allowNoMap) {
		  infod_log(LOG_ERR, "Error: Starting without map" );
	     } else {
		  infod_critical_error("Error: problem encounterd while setting cluster map");
	     }
	}

        


	/* Going daemon */
	if( !globOpts.opt_debug ) {
		/* Do not change the current working directory! */ 
		daemon( 1, 0 );
		glob_daemonMode = 1;
		infod_log(LOG_INFO, "Running as Daemon\n" );
	}
	else  
		msx_read_debug_file(NULL);
	
	
	/* 1. Setup specific data that will not change */
	general_setup();
	// Creating infod parent if not on debug mode
        if(!globOpts.opt_debug) {
	  infod_parent();
	}

        // Taking startup time
        gettimeofday(&runTimeInfo.startTime, NULL);

	/* 2. Set all the signal handlers */ 
	signal_setup();
	
	/* 3. Setup the communication channels */ 
	comm_setup();
	
	/* 5. Setup all the data structures and install the timer */ 
	if(mapOK) 
	  info_setup();

	gossip_setup();

	infod_log(LOG_INFO, "Initiation done. Success\n" );
	return 1;
}

/*****************************************************************************
 * Read the local load info.
 ****************************************************************************/
int
read_local_info() {

	//int cur_size = kcomm_get_infosize();
	int priority = -1;
	
	//if( cur_size != glob_local_info_size ) {
        //glob_local_info_size = cur_size;
        //allocate_local_info_buffer();
	//}

        debug_ly(KCOMM_DEBUG, "--------------> About to read local info\n");
                 
	if( ( priority = kcomm_get_info( glob_local_info_buff,
					 glob_local_info_size)) == -1 ) {
                
                debug_lr(KCOMM_DEBUG, "Got -1 in priority\n");
                return -1;
        }
        debug_ly(KCOMM_DEBUG, "Got local info: Age %d %d\n",
                 glob_local_info->hdr.time.tv_sec,
                 glob_local_info->hdr.time.tv_usec);
		 
	/* update the ip field */
	//memcpy( glob_local_info->hdr.ip, glob_ip, COMM_IP_VER );
	
	// Updating the ip field of the information
	glob_local_info->hdr.IP = globOpts.opt_myIP;
	glob_local_info->hdr.pe = globOpts.opt_myPE;
	
	/* Check if the local pe is different the one known to the kcomm
	   (mosixd). This is done only when kcom is up */
	/*if(kcomm_is_up()) {
		if( glob_pe != glob_local_info->hdr.pe ) 
			infod_set_pe_diff_mode();
		else
			infod_unset_pe_diff_mode();
	} else {
		infod_unset_pe_diff_mode();
	}
	*/
	return priority;
}

/*****************************************************************************
 * Doing a step of the selected gossip algorithm. This might be sending our
 * window (push), or might be rquesting other node to send us its window (pull)
 *
 * FIXME  Is blocking and unblocking if SIGALRM needed?
 ****************************************************************************/
int doGossipStep() {
	int    res = 0;
        struct gossipAction ga;
	
	// Calling the gossip algorithm step function
	res = (*globOpts.opt_gossipAlgo->stepFunc)(glob_vec, glob_gossip_data, &ga);
	// Not doing anything if error or no where to send/ask
	if(!res)
		return 0;

	// Performing the gossip step action
	block_sigalarm();
	if( ( res = comm_send_mosix( glob_msxcomm, &(ga.randIP),
				     glob_infod_port, ga.msgData,
				     ga.msgType, ga.msgLen, ga.keepConn )) == 1 )
		debug_lb( INFOD_DEBUG,
			  "Success, init send to random node %s\n", inet_ntoa(ga.randIP));
	else{
		debug_lr( INFOD_DEBUG, "Failure, init send to random node\n" );
		infoVecPunish( glob_vec, &(ga.randIP), INFOD_DEAD_CONNECT ) ;
	}
	unblock_sigalarm();
	return 1;
}

/*****************************************************************************
 * Continue sending information stored in comm - in send_inprogress
 ****************************************************************************/
int
send_inpr_info( fd_set *set, struct in_addr *ip ){

	int res = 0;
	
	block_sigalarm();
    
	res = comm_finish_send_mosix( glob_msxcomm, glob_msxmap, set, ip );
      
	if( res == -1 ) {
		debug_lr( INFOD_DEBUG, "Error: Failed sending data to %s\n",
			  inet_ntoa(*ip) );
		infoVecPunish( glob_vec, ip, INFOD_DEAD_CONNECT );
	}
    	else if( res == 0 ) 
		debug_lb( INFOD_DEBUG, "Message only partially send\n");
	
	else
		debug_lg( INFOD_DEBUG, "Finished sending the message %s.\n",
			  inet_ntoa(*ip));

	// Send if finished so we measure the time it took
	if(globOpts.opt_measureSendTime &&
	   runTimeInfo.sentMsgsSamples < runTimeInfo.sentMsgsMaxSamples)
	{
		gettimeofday(&runTimeInfo.sendFinishTime, NULL);
		// Calcluclating the accumulated time
		runTimeInfo.sendFinishTime.tv_sec -= runTimeInfo.sendStartTime.tv_sec;
		runTimeInfo.sendFinishTime.tv_usec -= runTimeInfo.sendStartTime.tv_usec;
		if(runTimeInfo.sendFinishTime.tv_usec < 0)
			runTimeInfo.sendFinishTime.tv_usec += 1000000;

		debug_lg(MEASURE_DEBUG, "Measurment: Send took (%d , %d)\n",
			 runTimeInfo.sendFinishTime.tv_sec, runTimeInfo.sendFinishTime.tv_usec);
		
		runTimeInfo.sendAccumulatedTime.tv_sec += runTimeInfo.sendFinishTime.tv_sec;
		runTimeInfo.sendAccumulatedTime.tv_usec += runTimeInfo.sendFinishTime.tv_usec;
		if(runTimeInfo.sendAccumulatedTime.tv_usec > 1000000 ) {
			runTimeInfo.sendAccumulatedTime.tv_usec -= 1000000;
			runTimeInfo.sendAccumulatedTime.tv_sec++;
		}
		runTimeInfo.sentMsgs++;
		runTimeInfo.sentMsgsSamples++;
	}
	unblock_sigalarm();
	
	return res ;
}
     
/*****************************************************************************
 * Receive information sent to this node
 ****************************************************************************/
int
receive_info( fd_set *set, comm_inprogress_recv_t* comm_msg ){

	int res;
      
	/* There should be data available for reading */ 
	block_sigalarm();
	res = comm_recv( glob_msxcomm, set, comm_msg ); 
	unblock_sigalarm();
      
	return res ;
}

/*****************************************************************************
 *  Handle the message
 ****************************************************************************/
void
handle_msg( msx_comm_t *comm, mapper_t map,
	    comm_inprogress_recv_t* comm_msg ) { 

	int type = 0;
        
	if( !comm_msg ) {
             debug_lr( INFOD_DEBUG, "Error: args, handle_msg()\n" );
             return;
	}

	type = comm_msg->hdr.type;  

	/* Information dissemination messages */ 
	switch (type) {
	    case INFOD_MSG_TYPE_INFO:
                 runTimeInfo.desiminationNum++;
//#ifdef INFOD2
                 runTimeInfo.infoMsgs++;
//#endif
                 handle_info( comm, map, comm_msg ) ;
                 shutdown( comm_msg->sock, SHUT_RDWR );
                 close( comm_msg->sock );
                 break;
	    case INFOD_MSG_TYPE_INFO_PULL:
                 if(!handle_info_pull( comm, map, comm_msg )) {
                      // Closing the socket only on error
                      shutdown( comm_msg->sock, SHUT_RDWR );
                      close( comm_msg->sock );
                 }
                 break;
	    case INFOD_MSG_TYPE_INFOLIB:
	    case MSG_TYPE_STR:
                 debug_lb( INFOD_DEBUG, "Got infolib message\n" ) ;
                 runTimeInfo.reqNum++;
                 if( handle_client_request( comm, map, comm_msg ) < 0 ) {
                      shutdown( comm_msg->sock, SHUT_RDWR );
                      close( comm_msg->sock );
                 }
		    
                 break;
	    case INFOD_MSG_TYPE_GINFOD:
                 debug_lb( GINFOD_DEBUG, "Got a ginfod message\n" ) ; 
                 handle_ginfod( comm_msg );
                 
                 shutdown( comm_msg->sock, SHUT_RDWR );
                 close( comm_msg->sock );
                 break;
	    default:
                 debug_lr( INFOD_DEBUG, "Error: unknown message type\n" ) ;
                 shutdown( comm_msg->sock, SHUT_RDWR );
                 close( comm_msg->sock );
	}
	
	free( comm_msg->data );	
	comm_msg->data = NULL;
}

/*****************************************************************************
 * Handle information message 
 ****************************************************************************/
int
handle_info(  msx_comm_t *comm, mapper_t map,
	      comm_inprogress_recv_t* comm_msg ) {

	unsigned int pe ;
	struct in_addr ipaddr;

	memcpy(&ipaddr.s_addr, comm_msg->ip, sizeof(struct in_addr));
	/* First extract the pe */
	if( !( mapper_addr2node( map, &ipaddr, &pe))){
		debug_lr( INFOD_DEBUG, "Error: extracting pe from ip\n");
		return 0;
	}

	debug_lb( INFOD_DEBUG, "Got PUSH information message from %s\n",
		  inet_ntoa(ipaddr));
			    
	// FIXME delete this line completely
	//block_sigalarm();
    
	/* updating the vector */
	debug_lb( INFOD_DEBUG, "Updating the vector.\n" ) ;
	infoVecUseRemoteWindow( glob_vec, comm_msg->data, comm_msg->hdr.size );
	return 1;
}


/*****************************************************************************
 * Handle information pull message 
 ****************************************************************************/
int
handle_info_pull(  msx_comm_t *comm, mapper_t map,
		   comm_inprogress_recv_t* comm_msg ) {
	
	int                  res;
	unsigned int         pe;
	struct in_addr       ipaddr;
	info_pull_msg_t     *pullMsg;
	struct gossipAction  ga;
	
	memcpy(&ipaddr.s_addr, comm_msg->ip, sizeof(struct in_addr));
	/* First extract the pe */
	if( !( mapper_addr2node( map, &ipaddr, &pe))){
		debug_lr( INFOD_DEBUG, "Error: extracting pe from ip\n");
		return 0;
	}

	// FIXME delete this line completely
	//block_sigalarm();
	
	pullMsg = (info_pull_msg_t *)comm_msg->data;
	debug_lb( INFOD_DEBUG, "Got valid PULL from -----> %d param (%d)\n",
		  pe, pullMsg->param) ;
	// Preparing a window to send back on the socket
	ga.msgData = infoVecGetWindow( glob_vec, &(ga.msgLen), 1);
	ga.msgType = INFOD_MSG_TYPE_INFO;
	ga.keepConn = 0;
	
	// Sending the window back to the pulling node
	res = comm_send_on_socket( glob_msxcomm, comm_msg->sock,
				   (char*)(ga.msgData), ga.msgType,
				   ga.msgLen, ga.keepConn);
	if( !res ) {
		debug_lr( INFOD_DEBUG, "Failed sending pull answer\n" );
		return 0;
	}
	return 1;
}

/*****************************************************************************
 * Handle a specific request from a cliet 
 ****************************************************************************/
int
handle_client_request(  msx_comm_t *comm, mapper_t map,
			comm_inprogress_recv_t* comm_msg ){

	char    *msgargs      = NULL ; 
	infolib_req_t request = 0;

	/* will hold the answer from the information vector */
	ivec_entry_t **vecptr = NULL ;
	infod_stats_t    stats;
	
	/* used to hold the arguments of the requets */ 
	//cont_pes_args_t  *cont  = NULL; 
	//subst_pes_args_t *subst = NULL;
//	char             *names = NULL;
//	node_t           *pes   = NULL;
//	unsigned long    age    = 0;
	int size = 0, ret = -1;

	msgargs = ((infolib_msg_t*)(comm_msg->data))->args;
	request =((infolib_msg_t*)(comm_msg->data))->request; 

	debug_lb( INFOD_DEBUG, "The Request is %d\n", request ) ;
	/* Hanlde the request */
	switch( request ) {

	    case INFOLIB_ALL:
		    vecptr = infoVecGetAllEntries( glob_vec );
		    size   = infoVecGetSize( glob_vec );
		    break;
	    case INFOLIB_WINDOW:
		    vecptr = infoVecGetWindowEntries( glob_vec, &size );
		    break;
			    
	    /* case INFOLIB_CONT_PES: */
/* 		    cont   = (cont_pes_args_t*)(msgargs) ; */
/* 		    vecptr = ivec_cont_ret_ni( glob_vec, cont->pe, cont->size); */
/* 		    size = cont->size; */
/* 		    break; */
	       
/* 	    case INFOLIB_SUBST_PES: */
/* 		    subst  = (subst_pes_args_t*)(msgargs); */
/* 		    vecptr = ivec_ret_ni( glob_vec, subst->pes, subst->size ); */
/* 		    size   = subst->size;  */
/* 		    break; */

/* 	    case INFOLIB_NAMES: */
/* 		    names  = (char*)(msgargs); */
/* 		    pes    = infod_names2pes( names, map, &size );  */
/* 		    vecptr = ivec_ret_ni( glob_vec, pes, size ) ; */
/* 		    free( pes ) ; */
/* 		    pes = NULL; */
/* 		    break; */
	       
/* 	    case INFOLIB_AGE: */
/* 		    age = *((unsigned long*)(msgargs)); */
/* 		    vecptr = ivec_ret_up2age( glob_vec, age, &size ) ; */
/* 		    break; */

/* 	    case INFOLIB_GET_NAMES: */
/* 		    size = global_buffer_size;  */
/* 		    if( !( infod_pes2names( global_buffer ))) */
/* 			    return -1; */
	       
/* 		    ret = comm_send_on_socket( glob_msxcomm, comm_msg->sock, */
/* 					       global_buffer, */
/* 					       comm_msg->hdr.type, */
/* 					       strlen( global_buffer ) + 1, 0); */
/* 		    if( !ret ){ */
/* 			    debug_lr( INFOD_DEBUG, "Failed sending names\n" ); */
/*  			    return -1; */
/* 		    } */

/* 		    return 0; */
		    
	    case INFOLIB_STATS:

		    if( ((ret = infoVecStats( glob_vec, &stats )) == -1 ) ||
			((ret = kcomm_get_stats( glob_vec, &stats ))) == -1 ) {
			    debug_lr( INFOD_DEBUG, "Failed stats\n");
			    return -1;
		    }
		    // We force a yardstick only if ctl got one from user
		    if(glob_yardstick) 
			    stats.sspeed = glob_yardstick;
		    
		    ret = comm_send_on_socket( glob_msxcomm, comm_msg->sock,
					       (char*)(&stats),
					       comm_msg->hdr.type,
					       STATS_SZ, 0 ) ;
		    if( !ret ) {
			    debug_lr( INFOD_DEBUG, "Failed sending stats\n" );
			    return -1;
		    }
		    return 0;

	    case INFOLIB_DESC:
                    debug_lb( INFOD_DEBUG, "Got description request (%d)\n",
                              glob_desc_size) ;
                    
		    if( glob_local_desc == NULL ) {
                            debug_lr( INFOD_DEBUG, "Error no description\n");
                            return -1;
                    }

                    ret = comm_send_on_socket( glob_msxcomm, comm_msg->sock,
					       glob_local_desc,
					       comm_msg->hdr.type,
					       glob_desc_size, 0);
		    if( !ret ) {
			    debug_lr( INFOD_DEBUG, "Failed sending desc\n" ) ;
			    return -1;
		    }
		    return 0;
	  
	    default:
		    return -1;
	}

	if( vecptr == NULL )
		return -1;

	ret = infod_reply_client( vecptr, size, comm_msg );

	if( vecptr )
		free( vecptr );
	return ret;
}

/****************************************************************************
 * Compute the size of the reply
 ***************************************************************************/
int
infod_reply_size( ivec_entry_t** ivecptr, int size ) {
	int rep_size = 0, i = 0;

	rep_size = IDATA_SZ;
	
	for( i = 0; i < size ; i++ ) {
		rep_size += IDATA_ENTRY_SZ;
		if( ivecptr[i] != NULL )
			rep_size += ivecptr[i]->info->hdr.fsize;
	}
	return rep_size;
}

/****************************************************************************
 * Send a reply to the client
 ***************************************************************************/
int
infod_reply_client( ivec_entry_t** ivecptr, int size,
		    comm_inprogress_recv_t* comm_msg )
{
	idata_t    *rep      = NULL;
	void       *rep_buff = NULL;
    	int         ret = 0;
	int         tmpSize = global_buffer_size;

	debug_lb(INFOD_DEBUG, "Replaying client %d\n", size); 
	rep_buff = infoVecPackQueryReplay(ivecptr, size, global_buffer, &tmpSize);
	if(!rep_buff) {
		debug_lr(INFOD_DEBUG, "Error packing query replay\n");
		ret = -1;
		goto done;
	}
	/* Finally, send the reply */
	rep = (info_replay_t *) rep_buff;

        ret = comm_send_on_socket( glob_msxcomm, comm_msg->sock, rep,
				   comm_msg->hdr.type,
				   rep->total_sz ,0 );
    

	if( !ret ){
		debug_lr( INFOD_DEBUG, "Failed replying client\n" ) ;
		ret = -1;
		goto done;
	}
	ret = 1; 

 done:
	if( rep_buff != global_buffer ) {
		free( rep_buff );
		rep_buff = NULL;
	}
	
	return ret;
}

/*****************************************************************************
 * Translate names to pes 
 ****************************************************************************/

#define   DEF_SIZE   (16)
node_t*
infod_names2pes( char *names, mapper_t map,  int *size  ) {

	node_t *pes ;
	node_t curpe; 
	char *ptr1, *ptr2;
	int maxsize = DEF_SIZE;
	*size = 0 ;
      
	if( !( pes = (node_t*) malloc( maxsize * NODE_SZ ))) {
		debug_lr( INFOD_DEBUG, "Error: malloc\n" );
		*size = -1;
		return NULL;
	}
      
	for( ptr1 = names ; *ptr1 ; ptr1 = ptr2 ){

		/* get all the spaces */ 
		for( ; *ptr1 && isspace( *ptr1 ) ; ptr1++  )
			*ptr1 = '\0';
	    
		/* get the name */ 
		for( ptr2 = ptr1 ; *ptr2 && !isspace (*ptr2) ; ptr2++ );

		for(  ; *ptr2 && isspace(*ptr2) ; ptr2++ )
			*ptr2 = '\0';

                
		/* get the pe from name */ 
		if( mapper_hostname2node( map, ptr1, (mnode_t *)&curpe ) ){
			if( *size == maxsize ) {
				maxsize *=2 ;
				if( !( pes = (node_t*)
				       realloc( pes, maxsize * NODE_SZ ))) {
					free( pes );
					pes = NULL;
					debug_lr( INFOD_DEBUG,
						  "Error: malloc\n" );
					*size = -1;
					return NULL;
				}
			}
			pes[ (*size)++ ] = curpe ;
		}
		else {
			pes[ (*size)++ ] = 0 ;
			debug_lr( INFOD_DEBUG,"Error: pe from name\n" );
		}
	}
	return pes;
}

/*****************************************************************************
 * Translate pe's to names (in IPv4 format)
 ****************************************************************************/
int
infod_pes2names( char *buff  ) {

	int i = 0, size = 0;
	ivec_entry_t **vecptr = NULL ;

	buff[ 0 ]='\0';
	size  = infoVecGetSize( glob_vec ) ;
	
	if( !( vecptr = infoVecGetAllEntries( glob_vec )))
		return 0; 
    
	for( i = 0 ; i < size ; i++ ) {
		strcat( buff, vecptr[i]->name );
		strcat( buff, " " );
	}

	if( vecptr )
		free( vecptr );

	return 1;
}

/****************************************************************************
 * Get a request from a ginfod, and update the data structures and the kernel.
 * Note that the message format is exactly the same as the message sent by the
 * infod to a client (using the info libraray ). 
 ****************************************************************************/
int
handle_ginfod( comm_inprogress_recv_t* comm_msg ){

	return 1;
}

/****************************************************************************
 * Handling control messages
 ***************************************************************************/

/****************************************************************************
 *  Making sure that the action credentials uid is equal to the process uid
 ***************************************************************************/
int
verify_action_cred( infodctl_action_t *action ) {
	int uid = getuid();
	if( action->action_cred.uid == uid )
		return 1;
	return 0;
}

/***************************************************************************
 * Handle the control command
 ***************************************************************************/
void
ctl_log_message( char* msg ) {
  syslog(LOG_INFO,"%s",  msg );
  debug_lb( CTL_DEBUG, "%s", msg );
}

/****************************************************************************
 * Cahnging the cluster map
 ***************************************************************************/
int
ctl_setpe( infodctl_action_t *action ) {

	ctl_log_message( "Changing cluster map - NOT supported yet");
	
	/* the map is in action (a) on static buffer */
	/* if( map_setup( action->action_data, action->action_datalen )) { */
/* 		block_sigalarm(); */
/* 		info_stop(); */
/* 		info_setup(); */
/* 		if( !glob_quiet_mode ) */
/* 			install_timer(); */
/* 		unblock_sigalarm(); */
/* 	} */
	
	return 1;
}

/****************************************************************************
 * Stoping the infod - map no longer valid
 ***************************************************************************/
int
ctl_setpe_stop( infodctl_action_t *action ) {

	ctl_log_message( "SETPE_STOP request. Stop infod\n" );
	uninstall_timer();
	mapperDone(glob_msxmap);
	glob_msxmap = NULL;
	info_stop();
	return 1;
}

/****************************************************************************
 * Setting the kernel speed. Sending kcomm speed change msg
 * and only if ok sending then we ackwnoledge
 ***************************************************************************/
int
ctl_set_speed( infodctl_action_t *action ){

	int new_speed;
	ctl_log_message( "Changing kernel speed\n");
	
	if( action->action_datalen >= sizeof(int)) {
		new_speed = ((int*)action->action_data)[0];
		debug_ly( CTL_DEBUG, "New speed %d\n", new_speed );
		if( kcomm_send_speed_change( new_speed ) )
			return 0;
	}
	return -1;
}

/****************************************************************************
 * Change the topology costs
 ***************************************************************************/
int
ctl_consts_change( infodctl_action_t *action ) {
	int i = 0, j = 0;
	unsigned long new_topology;
	unsigned long *consts;

	new_topology = ((int*)action->action_data)[0];
	debug_ly( CTL_DEBUG, "Const change topology = %d\n", new_topology);
	consts = &((unsigned long*)action->action_data)[1];
	    
	for( i = 0; i < new_topology; i++ ) {
		for( j = 0; j < ALL_TUNE_CONSTS; j++ )
			debug_ly( CTL_DEBUG, "%d ", consts[i*ALL_TUNE_CONSTS+j] ); 
		debug_ly( CTL_DEBUG, "\n");
	}
	
	if( kcomm_send_cost_change(new_topology, consts ))
		return 0;

	return -1;
}

/****************************************************************************
 * Set the yard stick
 ***************************************************************************/
int
ctl_set_yard( infodctl_action_t *action ) {

	int new_yard = 0;
	debug_ly( CTL_DEBUG, "Changing yard (standard speed)\n");
	
	if( action->action_datalen >= sizeof(int) ) {
		new_yard = ((int*)action->action_data)[0];
		debug_ly(CTL_DEBUG, "New yard %d\n", new_yard);
	}
	
	if(new_yard > 0) {
		glob_yardstick = new_yard;
		return 0;
	}

	return -1;
}

/****************************************************************************
 * Set the debug level of the infod
 ***************************************************************************/
int
ctl_set_debug( infodctl_action_t *action ) {

	int dbg = 0;
	debug_ly( CTL_DEBUG, "Changing debug mode\n");
	if( action->action_datalen >= sizeof(int) )
		dbg = ((int*)action->action_data)[0];
	if( dbg & CLEAR_DEBUG ) {
		globOpts.opt_clear = 1;
		dbg &= ~CLEAR_DEBUG;
	}
	else {
		globOpts.opt_clear = 0;
	}
	msx_set_debug( dbg );

	return 1;
}

/****************************************************************************
 *  Return the ctl status
 ***************************************************************************/
int
ctl_get_status( infodctl_action_t *action ) {

	char sbuff[MAX_STATS_STR_LEN];
        char *ptr = sbuff;
        char tmp[100];
	ptr += sprintf( sbuff,
                        "Infod status:\n"
                        "PID         %d\n"
                        "IP          %s\n"
                        "Uptime      %s\n"
                        "Debug       %x\n"
                        "Nodes       %d\n"
                        "Live        %d\n"
                        "map status  %d\n"
                        "kernel stat %d %d\n"
                        "Module      %s\n" 
                        "Kcomm sock  %s\n" 
                        "Ctl sock    %s\n" 
                        "Comm Port   %d\n"
                        "Lib  Port   %d\n"
                        "Ginfod Port %d\n"
                        "Window Type %d\n"
                        "Window Parm %d\n"
                        "Gossip Algo %s\n"
                        "Max age     %d\n"
                        "Info Desim  %d\n"
                        "Info req    %d\n",
                        getpid(),
                        inet_ntoa(globOpts.opt_myIP),
                        get_infod_uptime_str(),
                        msx_get_debug(),
                        infoVecGetSize( glob_vec ),
                        infoVecNumAlive(glob_vec),

                        globOpts.stat_mapOk,
                        kcomm_is_up(), kcomm_is_auth(),
                        glob_kcomm_module_name,
                        MSX_INFOD_KCOMM_SOCK_NAME,
                        MSX_INFOD_CTL_SOCK_NAME,
                        glob_infod_port, glob_infod_lib_port,
                        glob_infod_ginfod_port,
                        globOpts.opt_winType,
                        globOpts.opt_winParam,
                        globOpts.opt_gossipAlgo->algoName,
                        globOpts.opt_maxAge,
                        runTimeInfo.desiminationNum,
                        runTimeInfo.reqNum);
        // Adding comm statistics
        comm_print_status(glob_msxcomm, ptr, 2048);

        if(globOpts.opt_measureAvgAge) {
		ivec_age_measure_t im;
		infoVecGetAgeMeasure(glob_vec, &im);
		sprintf(tmp, "Avg Age     %.4f (samples %d max %d)\n",
			im.im_avgAge, im.im_sampleNum, im.im_maxSamples);
		strcat(sbuff, tmp);
		sprintf(tmp, "Avg Max Age %.4f (samples %d max %d)\n",
			im.im_avgMaxAge, im.im_sampleNum, im.im_maxSamples);
		strcat(sbuff, tmp);
		sprintf(tmp, "Max Max Age %.4f (samples %d max %d)\n",
			im.im_maxMaxAge, im.im_sampleNum, im.im_maxSamples);
		strcat(sbuff, tmp);
		
	}
        if(globOpts.opt_measureEntriesUptoage) {
             ivec_entries_uptoage_measure_t im;
             infoVecGetEntriesUptoageMeasure(glob_vec, &im);
             for(int i=0 ; i<im.im_size ; i++) {
                  sprintf(tmp, "Entries Uptoage %.4f age %.4f (samples %d max %d)\n",
                          im.im_entriesUpto[i], im.im_ageArr[i],
                          im.im_sampleNum, im.im_maxSamples);
                  strcat(sbuff, tmp);
             }
        }
	if(globOpts.opt_measureWinSize)
	{
		ivec_win_size_measure_t im;
		infoVecGetWinSizeMeasure(glob_vec, &im);
		sprintf(tmp, "Avg WinSize %.4f (samples %d max %d)\n",
			im.im_avgWinSize, im.im_sampleNum, im.im_maxSamples);
		strcat(sbuff, tmp);
	}
	if(globOpts.opt_measureInfoMsgsPerSec)
	{
		sprintf(tmp, "Info Per Sec    %.4f (samples %d max %d)\n",
			runTimeInfo.infoMsgsPerSec,
			runTimeInfo.infoMsgsSamples, runTimeInfo.infoMsgsMaxSamples); 
		strcat(sbuff, tmp);
	}
	if(globOpts.opt_measureSendTime)
	{
		double sendTime;
		double avgTimeUnit;

		sendTime  = runTimeInfo.sendAccumulatedTime.tv_sec;
		sendTime += (double)runTimeInfo.sendAccumulatedTime.tv_usec/1000000.0;
		sendTime /= (double) runTimeInfo.sentMsgs;

		// We decrement 1 from the time step since we did not take the
		// first measurment into account (prevTime is not correct)
		avgTimeUnit = runTimeInfo.totalTime / (runTimeInfo.timeSteps -1);

		sprintf(tmp,
			"Avg Send Time   %.4f (samples %d max %d)\n"
			"Avg time unit   %.4f\n",
			sendTime, runTimeInfo.sentMsgsSamples, runTimeInfo.sentMsgsMaxSamples,
			avgTimeUnit);
		strcat(sbuff, tmp);
	}
	
	infodctl_reply(sbuff, strlen(sbuff) + 1);
	return 1;
}

int ctl_avg_age_measurment(infodctl_action_t *action)
{
	int samplesNum = 0;
	
	debug_ly( CTL_DEBUG, "Setting Average age measurment mode\n");
	samplesNum = action->action_arg1;
	if(samplesNum >= 0) {
		globOpts.opt_measureAvgAge = 1;
		infoVecClearAgeMeasure(glob_vec);
		infoVecSetAgeMeasureSamplesNum(glob_vec, samplesNum);
	} else {
		globOpts.opt_measureAvgAge = 0;
	}
	return 1;
}

int ctl_entries_uptoage_measurment(infodctl_action_t *action)
{
	int samplesNum = 0;

	debug_ly( CTL_DEBUG, "Setting entries-uptoage measurment mode\n");
	samplesNum = action->action_arg1;

        int size   = action->action_arg2;
        float *ageArr = (float *)action->action_data;
	debug_ly( CTL_DEBUG, "Samples %d Size %d age[0] %.3f\n", samplesNum, size, ageArr[0]);
	if(samplesNum >= 0) {
             globOpts.opt_measureEntriesUptoage = 1;
             infoVecClearEntriesUptoageMeasure(glob_vec);
             infoVecSetEntriesUptoageMeasure(glob_vec, samplesNum, ageArr, size);
	} else {
             globOpts.opt_measureEntriesUptoage = 0;
	}
	return 1;
}


int ctl_window_size_measurment(infodctl_action_t *action)
{
	int samplesNum = 0;
	
	debug_ly( CTL_DEBUG, "Setting window size measurment mode\n");
	
	samplesNum = action->action_arg1;
	if(samplesNum >= 0) {
		infoVecClearWinSizeMeasure(glob_vec);
		infoVecSetWinSizeMeasureSamplesNum(glob_vec, samplesNum);
		globOpts.opt_measureWinSize = 1;
	} else {
		infoVecClearWinSizeMeasure(glob_vec);
		globOpts.opt_measureWinSize = 0;
	}
	return 1;
}

int ctl_measure_info_msgs(infodctl_action_t *action)
{
	int samplesNum = 0;
	
	debug_ly( CTL_DEBUG, "Setting info messages measurment mode\n");
	
	samplesNum = action->action_arg1;
	if(samplesNum >= 0) {
		globOpts.opt_measureInfoMsgsPerSec = 1;
		gettimeofday(&runTimeInfo.infoMsgsStartTime, NULL);
		runTimeInfo.infoMsgsMaxSamples = samplesNum;
		runTimeInfo.infoMsgsSamples = 0;
		runTimeInfo.infoMsgs = 0;
	} else {
		globOpts.opt_measureInfoMsgsPerSec = 0;
		runTimeInfo.infoMsgsMaxSamples = 0;
		runTimeInfo.infoMsgsSamples = 0;
	}
	return 1;
}


int ctl_measure_send_time(infodctl_action_t *action)
{
	int samplesNum = 0;
	
	debug_ly( CTL_DEBUG, "Setting send time measurment mode\n");
	
	samplesNum = action->action_arg1;
	if(samplesNum >= 0) {
		globOpts.opt_measureSendTime = 1;
		runTimeInfo.sentMsgsMaxSamples = samplesNum;
		runTimeInfo.sentMsgsSamples = 0;
		runTimeInfo.sentMsgs = 0;
		runTimeInfo.timeSteps = 0;
		runTimeInfo.totalTime = 0.0;
	} else {
		globOpts.opt_measureSendTime = 0;
		runTimeInfo.sentMsgsMaxSamples = 0;
		runTimeInfo.sentMsgsSamples = 0;
	}
	return 1;
}

int ctl_death_log_clear(infodctl_action_t *action)
{
	infoVecClearDeathLog(glob_vec);
	return 1;
}

int ctl_death_log_get(infodctl_action_t *action)
{
	
	char             sbuff[MAX_STATS_STR_LEN];
	char             tmp[100];
	int              i;
	ivec_death_log_t dl;
	
	infoVecGetDeathLog(glob_vec, &dl);
	
	sprintf( sbuff, "Death log: (%d)\n", dl.size);
	for(i=0 ; i<dl.size ; i++) {
		sprintf(tmp, "%s %.3f\n",
			inet_ntoa(dl.ips[i]),dl.deathPropagationTime[i]);
		strcat(sbuff, tmp);
	}
	infodctl_reply(sbuff, strlen(sbuff) + 1);
	return 1;
}



int ctl_gossip_algo( infodctl_action_t *action) {
	int                     res;
	int                     gossipAlgoIndex;
	struct infodGossipAlgo *iga;

	// We expect the user to give numbers starting from 1 but
	// the index starts from 0
	gossipAlgoIndex   = action->action_arg1 - 1;

	debug_ly( CTL_DEBUG, "Setting gossip algorithm %d\n", gossipAlgoIndex);
	// Only positive values are allowd
	if(gossipAlgoIndex < 0 )
		return 0;

	// Checking the upper bound

	// Setting the gossip algorithm to the given number
	iga = getGossipAlgoByIndex(gossipAlgoIndex);
	if(!iga) {
		debug_lr( CTL_DEBUG, "Error given gossip algo index is wrong\n");
		res = 0;
	} else {
		globOpts.opt_gossipAlgo = iga;
		gossip_setup();
		res = 1;
	}
	return res;
}


int ctl_win_type( infodctl_action_t *action) {
	int winType, winParam;

	winType   = action->action_arg1;
	winParam  = action->action_arg2;

	debug_ly( CTL_DEBUG, "Setting wintype %d %d\n", winType, winParam);
	// Only positive values are allowd
	if(winParam <=0 )
		return 0;
	if(winType == 0)
		winType = INFOVEC_WIN_FIXED;
	else
		winType = INFOVEC_WIN_UPTOAGE;

	// Updating the options to reflect the new parameters
	globOpts.opt_winType = winType;
	globOpts.opt_winParam = winParam;
	// Restarting infomration stuff only if we can (if we have a valid map)
	if(globOpts.stat_mapOk) {
		info_stop();
		info_setup();
	}
	return 1;
}

int
do_ctl_action( infodctl_action_t *action ) {
	int res = 0;

	/* Test that the channel is authorized */ 
	if( !verify_action_cred( action )) {
		ctl_log_message( "Un authorised CTL request\n" );
		infodctl_reset();
	}
        
	switch( action->action_type ) {
	    case INFOD_CTL_QUIET:
		    ctl_log_message( "Moving to Quiet mode.\n" );
		    uninstall_timer();
		    glob_quiet_mode = 1;
		    res = 1;
		    break;
	    
	    case INFOD_CTL_NOQUIET:
		    ctl_log_message( "Moving to NOQUIET mode.\n" );
		    install_timer();
		    glob_quiet_mode = 0;
		    res = 1;
		    break;
	    
	    case INFOD_CTL_SETPE:
		    res = ctl_setpe( action );
		    break;
	    
	    case INFOD_CTL_SETPE_STOP:
		    res = ctl_setpe_stop( action );
		    break;

	    case INFOD_CTL_SET_SPEED:
		    res = ctl_set_speed( action );
		    break;
		    
	    case INFOD_CTL_CONST_CHANGE:
		    res = ctl_consts_change( action );
		    break;
		    
	    case INFOD_CTL_SET_YARD:
		    res = ctl_set_yard( action );
		    break;
		    
	    case INFOD_CTL_GET_YARD:
		    res = glob_yardstick;
	    	    break;
		    
	    case INFOD_CTL_SET_DEBUG:
		    ctl_set_debug( action );
		    break;
		    
	    case INFOD_CTL_GET_STATUS:
		    res = ctl_get_status( action );
		    break;
	    case INFOD_CTL_MEASURE_AVG_AGE:
		    res = ctl_avg_age_measurment( action );
		    res = ctl_window_size_measurment( action );
		    break;
            case INFOD_CTL_MEASURE_UPTOAGE:
                 res = ctl_entries_uptoage_measurment(action);
                 break;
	    case INFOD_CTL_WIN_SIZE_MEASURE:
		    res = ctl_window_size_measurment( action );
		    break;
	    case INFOD_CTL_GOSSIP_ALGO:
		    res = ctl_gossip_algo(action);
		    break;
	    case INFOD_CTL_WIN_TYPE:
		    res = ctl_win_type(action);
		    break;
	    case INFOD_CTL_MEASURE_INFO_MSGS:
		    res = ctl_measure_info_msgs(action);
		    break;
	    case INFOD_CTL_MEASURE_SEND_TIME:
		    res = ctl_measure_send_time(action);
		    break;
	    case INFOD_CTL_DEATH_LOG_CLEAR:
		    res = ctl_death_log_clear(action);
		    break;
	    case INFOD_CTL_DEATH_LOG_GET:
		    res = ctl_death_log_get(action);
		    break;
	    default:
		    infod_log(LOG_INFO, "Unsupported ctl msg\n");
		    debug_lr(CTL_DEBUG, "Error: unsupported ctl msg\n");
		    res = -1;
	}
    
	infodctl_reply( (char *)&res, sizeof(int) );
        
	return 1;
}

/*****************************************************************************
 * Blocking the signal sigalarm is important when dealing with comm object
 * because the routines inside sigalarm may alter the msx_comm_t object data
 * structures.
 ****************************************************************************/
inline int
block_sigalarm() {
	sigset_t set;
    
	sigemptyset( &set );
	sigaddset( &set, SIGALRM );
	sigprocmask( SIG_BLOCK, &set, NULL );
	return 1 ;
}

inline int
unblock_sigalarm() {
	sigset_t set;
      
	sigemptyset( &set);
	sigaddset( &set, SIGALRM );
	sigprocmask( SIG_UNBLOCK, &set, NULL );
	return 1;
}

/*****************************************************************************
 * Signal handlers
 ****************************************************************************/

/*
 * SIGALRM handler
 */

inline double timeDiff(struct timeval *s, struct timeval *e) {
	double diff;
	diff = e->tv_sec - s->tv_sec;
	if(e->tv_usec < s->tv_usec) {
		diff -= 1.0;
		diff += (1000000.0 + e->tv_usec - s->tv_usec)/1000000.0;
	}
	else {
		diff += (e->tv_usec - s->tv_usec) / 1000000.0;
	}
	return diff;
}

void sig_alarm_hndl(int sig) {
        glob_got_sigalarm = 1;
        doTimeStep();
}


void doTimeStep()
{
	static struct timeval prevTime;
	int priority = -1;

        debug_ly( INFOD_DEBUG, "============ Doing TimeStep ============\n" ) ;
        glob_timeSteps++;

        // Not doing anything as long as the map is not configured
        if(globOpts.stat_mapOk == 0)
             goto after_gossip;
        
	if(globOpts.opt_measureSendTime) {
		double diff;
		gettimeofday(&runTimeInfo.sendStartTime, NULL);
		diff = timeDiff(&prevTime,&runTimeInfo.sendStartTime);  
		prevTime = runTimeInfo.sendStartTime; 
		debug_lr(MEASURE_DEBUG, "---------------> (%d , %d %.6f)\n",
			 runTimeInfo.sendStartTime.tv_sec,
			 runTimeInfo.sendStartTime.tv_usec,
			 diff);
		
		runTimeInfo.timeSteps++;
		// We skeep the first measurment since the prevTime was not
		// correct
		if(runTimeInfo.timeSteps > 1)
			runTimeInfo.totalTime += diff;
	}

	//install_timer();
	
	/* update the local entry in the vector */ 
	if(kcomm_is_up() &&  ((priority = read_local_info()) != -1) )
	{
		infoVecUpdate( glob_vec, glob_local_info,
			     glob_local_info_size, priority );
	} else {
		infoVecPunish( glob_vec, &globOpts.opt_myIP, INFOD_DEAD_PROVIDER );
	}
	/* send information only if we are not in quiet mode */
	if( !glob_quiet_mode )
		doGossipStep();

	if (globOpts.opt_measureAvgAge) {
		debug_ly( INFOD_DEBUG, "measure value = %d\n",
			  globOpts.opt_measureAvgAge ) ;
		infoVecDoAgeMeasure(glob_vec);
		/*
			if( globOpts.opt_debug) {
			infod_stats_t    stats;
			ivec_age_measure_t im;
			infoVecStats(glob_vec, &stats);
			infoVecGetAgeMeasure(glob_vec, &im);
			debug_ly(MEASURE_DEBUG, "AvgAge: %.4f  %.4f\n",
			stats.avgage, im.im_avgAge);
		}
		*/
	}
        if(globOpts.opt_measureEntriesUptoage) {
             infoVecDoEntriesUptoageMeasure(glob_vec);
	}
	if( globOpts.opt_measureInfoMsgsPerSec) {
             if(runTimeInfo.infoMsgsMaxSamples > 0 &&
                runTimeInfo.infoMsgsSamples < runTimeInfo.infoMsgsMaxSamples)
             {
                  struct timeval currTime;
                  double secs;
                  gettimeofday(&currTime, NULL);
                  secs = currTime.tv_sec - runTimeInfo.infoMsgsStartTime.tv_sec;
                  runTimeInfo.infoMsgsPerSec = (double)runTimeInfo.infoMsgs/secs;
                  runTimeInfo.infoMsgsSamples ++;
             }
	}
	
 after_gossip:
/*	if( badip == 0x45544 ) {
		int a = 7, b = 7;
		globOpts.opt_debug = globOpts.opt_debug/(a-b);
	}
*/
        glob_got_sigalarm = 0;
}


/*
 * SIGHUP handler
 */
void sig_hup_hndl( int sig ){

	infod_log(LOG_INFO, "Got SIGHUP\n" );
	glob_got_sighup = 1;
}

/*
 * Termination
 */
void
infod_exit(){

	/* close the map */
	//mapperDone( glob_msxmap ) ;

	/* clear the vector */
	//infoVecFree( glob_vec ) ;

	/* close msx_comm */
	comm_close( glob_msxcomm ) ;

	/* stopping infodctl subsystem */
	infodctl_stop();
    
	infod_log(LOG_INFO, "Terminating.\n" );

        // In debug mode there is no parent so we should also clean as if we
        // are the parent
        if(globOpts.opt_debug) 
             infod_parent_exit(0);
        
        exit(glob_infodExit);
}

/*
 *  SIGINT handler
 */
void sig_int_hndl( int sig ){

	infod_log(LOG_INFO, "Got SIGINT. Start terminating routines\n" );
        glob_infodExit = sig;
}


/*
 *  SIGTERM handler
 */
void sig_term_hndl( int sig ){
     debug_lr(INFOD_DEBUG, "Got SIGTERM ... starting termination squance\n");
     infod_log( LOG_CRIT, "Got SIGTERM. Start terminating routines\n" );
     glob_infodExit = sig;
}


/*
 *  SIGISEGV handler
 */
void sig_segv_hndl( int sig ){

	infod_log(LOG_INFO, "Got SIGSEGV. Start terminating routines\n" );
	signal( SIGSEGV, SIG_DFL );
	raise( sig );
}

/*
 *  SIGABRT handler
 */
void sig_abrt_hndl( int sig ){

	infod_log(LOG_INFO, "Got SIGABRT. Check if binaries were changed\n" );
	signal( SIGABRT, SIG_DFL );
	raise( sig );
}


/****************************************************************************
 * Installing and uninstall the timer
 ***************************************************************************/
int
install_timer()
{
/* 	struct itimerval timeout; */

/* 	int timeStep = globOpts.opt_timeStep; */
/* 	timeout.it_value.tv_sec = timeStep / 1000; */
/* 	timeout.it_value.tv_usec = (timeStep % 1000) * 1000; */
/* 	timeout.it_interval = timeout.it_value; */
/* 	setitimer( ITIMER_REAL, &timeout, NULL); */

	return 1;
}

int
uninstall_timer() {
//	setitimer( ITIMER_REAL, NULL, NULL);
	return 1;
}

/*****************************************************************************
 * MOSIX specific functions for send. 
 *****************************************************************************/



int
comm_send_mosix( msx_comm_t *comm, struct in_addr *ip, unsigned short port,
		 void *buff, int type, int size, int mv2recv ){
	
	//unsigned int p[4];
  //unsigned int ip_horder = ntohl(ip->s_addr);
	
	if( !comm ) {
		debug_lr( INFOD_DEBUG, "Error: args, comm_send_mosix\n");
		return -1;
	}
	// checking if ip is valid
	// This check is only good for ipv4 
	//p[0] = ((char *)ip)[3];
	//p[1] = ((char *)ip)[2];
	//p[2] = ((char *)ip)[1];
	//p[3] = ((char *)ip)[0];

	//debug_lr(INFOD_DEBUG, "checking ip %s\n", inet_ntoa(*ip));
	return comm_send(comm, (char *)ip, port, buff, type, size, mv2recv );
}

int comm_finish_send_mosix( msx_comm_t *comm, mapper_t map,
			fd_set *set, struct in_addr *IP)
{
	struct in_addr ipaddr;
	char ip[COMM_IP_VER];
	int res, pe;
	
	bzero( ip, COMM_IP_VER*sizeof(char));
    
	if( !comm ){
		debug_lr( INFOD_DEBUG, "Error: args, comm_finish_s_mosix\n");
		return -1;
	}

	/* calling the regular finish send routine */
	res = comm_finish_send( comm, set, ip );
      
	/* converting ip to mosix number */
	memcpy(&ipaddr.s_addr, ip, sizeof(in_addr_t)); 
	
	if( !mapper_addr2node( map, &ipaddr, (mnode_t *) &pe )) {
		debug_lr( INFOD_DEBUG, "Error: IP not in map. "
			  "%u.%u.%u.%u\n",
			  (unsigned char)ip[0], (unsigned char)ip[1],
			  (unsigned char)ip[2], (unsigned char)ip[3]);
		res = 0;
	}
	*IP = ipaddr;
      	return res;
}

/****************************************************************************
 * Utility functions
 ***************************************************************************/
void
adjust_time( struct timeval *cur, struct timeval *time ){

	time->tv_sec  = cur->tv_sec - time->tv_sec ;
	time->tv_usec = cur->tv_usec - time->tv_usec ;
	    
	if( time->tv_usec < 0 ){
		time->tv_sec-- ;
		time->tv_usec += 1000000 ;
	}
}

/**
 * Returning a static string containing a human readable string of the infod
 * uptime.
 */
char *get_infod_uptime_str()
{
	static char uptime_str[100];
	struct timeval curr_time;
	long sec;
	long d, h, m;
	
	gettimeofday( &curr_time, NULL );

	sec = curr_time.tv_sec - runTimeInfo.startTime.tv_sec;

	// Taking the days
	d = sec / (60*60*24);
	sec -= d * (60*60*24);
	// Taking the left hours
	h = sec / (60*60);
	sec -= h* (60*60);
	// The left minutes
	m = sec / 60;
	sec -= m*60;
	
	sprintf(uptime_str, "%ldd:%ldh:%ldm:%lds", d, h, m, sec);
	return uptime_str;
}

/******
 * Kcomm signal that there was a change in our world (map)
 * This function is called from within the kcomm in sigalarm blocked
 */
void map_change_hndl() {
	if( map_setup( NULL, 0)) {
		info_stop();
		info_setup();
		if( !glob_quiet_mode )
			install_timer();
	}
}

/***********************************************************************************
 * Performing a random sleep, to try and break symmetry in case all the infods
 * started too close to each other
 **********************************************************************************/
void infod_random_sleep() {
	struct timespec req, rem;

	req.tv_sec=1;
	req.tv_nsec = rand() % 999999999;
	if(nanosleep(&req, &rem) == -1) {
		debug_lr(INFOD_DEBUG, "nanosleep() interrupted\n");
	}
}

void infod_periodic_admin()
{
     static struct timeval prev = {0,0};
     struct timeval now;
     int timeStepMilli = globOpts.opt_timeStep;
     
     gettimeofday(&now, NULL);
     unsigned long long milliDiff = (now.tv_sec - prev.tv_sec) * 1000;
     milliDiff = milliDiff + (now.tv_usec - prev.tv_usec)/1000;

     //debug_lg(INFOD_DEBUG, "Now  %ld %ld\n", now.tv_sec, now.tv_usec);
     //debug_lg(INFOD_DEBUG, "Prev %ld %ld\n", prev.tv_sec, prev.tv_usec);
     //debug_lg(INFOD_DEBUG, "Milli diff %d %d\n", milliDiff, timeStepMilli);

     if(milliDiff >= timeStepMilli) {
          prev = now;
          doTimeStep();
     }
     
     static unsigned int prevMapReloadTimeStep = 0;
     static unsigned int prevPeriodicAdminTimeStep = 0;
     
     static time_t prevMTime = 0;
     
     
     // General periodic admin
     unsigned int p = ((glob_timeSteps - prevPeriodicAdminTimeStep)* timeStepMilli)/1000;
     if(p >= MSX_INFOD_PERIODIC_ADMIN_TIME) {
          prevPeriodicAdminTimeStep = glob_timeSteps;

          debug_lg(INFOD_DEBUG, "****** INFOD-PERIODIC-ADMIN ******\n");
          
          // Checking if our parent is still alive
          if( !globOpts.opt_debug && getppid() != glob_parent_pid )
               infod_critical_error( "Error: lost parent\n" );
          
          // Reading debug file
          if( globOpts.opt_debug ) {
               if( globOpts.opt_clear)
                    system( "clear" ); 
               infoVecPrint(stderr, glob_vec, 4 );
               msx_read_debug_file(NULL);
          }

          // Checking for IP change
          struct in_addr  myIP;
          if(!globOpts.opt_forceIP) {
	       if(resolve_my_ip(&myIP)) {
		    if(myIP.s_addr != globOpts.opt_myIP.s_addr) {
			 debug_lr(INFOD_DEBUG, "IP changed to %s\n",
				  inet_ntoa(myIP));
			 globOpts.opt_myIP = myIP;
			 if(map_setup()) {
			      info_stop();
			      info_setup();
			 }
		    }
	       }
	       else {
		    debug_lr(INFOD_DEBUG, "Error: Can not find IP\n");
		    globOpts.stat_mapOk = 0;
	       }
          }

          
          // Checking the map again if the globOpts.stat_mapOk == 0
          if(globOpts.stat_mapOk == 0) {
               if(map_setup()) {
                    info_stop();
                    info_setup();
               }
          }
     }

     // MAP reload 
     p = ((glob_timeSteps - prevMapReloadTimeStep)* timeStepMilli)/1000;
     if(p >= MSX_INFOD_RELOAD_MAP_TIME) {
          prevMapReloadTimeStep = glob_timeSteps;
          
          debug_lb(INFOD_DEBUG, "Reloading map if changed\n");
          if(globOpts.opt_mapSourceType == INPUT_FILE) {
               struct stat s;
               if(stat(globOpts.opt_mapBuff, &s) == -1) {
                    debug_lr(INFOD_DEBUG, "Error stating map file %s\n",
                             globOpts.opt_mapBuff);
                    return;
               }
               
               if(s.st_mtime  != prevMTime) {
                    if(prevMTime != 0) {
                         debug_ly(INFOD_DEBUG, "Reloading map using SIGHUP\n");
                         glob_got_sighup = 1;
                    }
                    prevMTime = s.st_mtime;
               }
          }
     }
     
     
}

/******************************************************************************
 *                                E O F 
 *****************************************************************************/








