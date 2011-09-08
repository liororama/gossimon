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
 *   Authors: Lior Amar
 *
 *****************************************************************************/


/******************************************************************************
 *    File: infod_parent.c
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
#include <string.h>

#include <signal.h>
#include <errno.h>

#include <netinet/in.h>
#include <netdb.h>   
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sched.h>

#include <msx_common.h>
#include <msx_debug.h>
#include <msx_error.h>

#include <infod.h>
#include <ctl.h>

int handle_child_exit();

/****************************************************************************
 * Function related to the handling of the infod parent that is only
 * responsible for monitoring the state of the infod.
 ***************************************************************************/
void infod_parent_exit(int status) {
     removePidFile();
     exit(status);
}

void infod_parent_critical_error( char *msg )
{
	/* send the same signal to the child */
	infod_log(LOG_ERR, msg );
	kill( glob_working_pid, SIGTERM );
        infod_parent_exit(EXIT_FAILURE);
}


/****************************************************************************
 * Forwoard the received signals to the worker daemon and exit
 ***************************************************************************/
void sig_parent_hndl( int sig ) {
	kill( glob_working_pid, sig );

	// The father exit on sigterm and sigint (sig hup is only passed on)
	if(sig == SIGINT || sig == SIGTERM) {
		//infod_log(LOG_INFO, "Exiting infod, got termination request" );
		infod_parent_exit( EXIT_FAILURE );
	}
}

int infod_parent_signals(){

	struct sigaction act;
	static int flg = 0;

	if( flg++ )
		return 1;
	
	/* setting the signal handler of SIGHUP */
	act.sa_handler = sig_parent_hndl ;
	sigemptyset ( &( act.sa_mask ) );
	sigaddset ( &( act.sa_mask) , SIGALRM ) ;
	act.sa_flags = SA_RESTART ;
    
	if ( sigaction ( SIGHUP, &act, NULL ) < -1 )
		infod_parent_critical_error( "Error: parent on SIGHUP\n" ) ;

	/* setting the signal handler of SIGINT */
	act.sa_handler = sig_parent_hndl ;
	sigfillset ( &( act.sa_mask ) ) ;
	act.sa_flags = SA_RESTART ;

	if ( sigaction ( SIGINT, &act, NULL ) < -1 )
		infod_parent_critical_error( "Error: parent on SIGINT\n" ) ;

	/* setting the signal handler of SIGTERM */
	act.sa_handler = sig_parent_hndl ;
	sigemptyset ( &( act.sa_mask ) ) ;
	act.sa_flags = SA_RESTART ;

	if ( sigaction ( SIGTERM, &act, NULL ) < -1 )
		infod_parent_critical_error( "Error: parent on SIGTERM\n" ) ;

	return 1;
}


int infod_parent() { 
     while( 1 ) {
	  // Creating another process
	  runTimeInfo.childNum++;
	  
	  // Waiting before successive forks 
	  if(runTimeInfo.childNum > 1) {
	       infod_log(LOG_INFO, "Sleeping for %d seconds before respawning\n", 10);
	       sleep(10);
	  }

	  if( ( glob_working_pid = fork()) < 0 ) 
	       infod_critical_error( "Error: forking\n" );
	  
	  // The main process wait for the child to exit (guardian)
	  if( glob_working_pid > 0 ) {
	       infod_log(LOG_INFO, "Parent pid %d\n", getpid());
	       infod_parent_signals();	       
	       handle_child_exit();
	  }
	  else {
	       // The child exit the while loop and continue to run
	       glob_parent_pid = getppid();
	       break;
	  }
     }
     return 1;
}


int handle_child_exit() {

     pid_t      pid = 0;
     int        status = 0, sig = 0;
     char       tmp[ SMALL_BUFF_SIZE ];
     static int counter  = 0;
     
     while( 1 ) {
	  
	  pid = wait( &status );
	  
	  /* Some kind of error */
	  if( pid == -1 )
	       infod_parent_exit( EXIT_FAILURE );
	  
	  /* A child has exited */
	  if( pid != 0 ) {
	       
	       if( pid != glob_working_pid )
		    continue;
	       
	       /* get the true nature of the exit status */
	       if( WIFEXITED( status ) ) {
		    status = WEXITSTATUS( status );
		    infod_log(LOG_ERR, "Child exited with %d", status); 
		    infod_parent_exit( status );
	       }
	       
	       if( WIFSIGNALED( status ) ) {
		    sig = WTERMSIG( status );
		    infod_log(LOG_INFO, "Child exited with signal %d", sig ); 
		    
		    // For SIGSEGV and SIGABRT infod parent continue to run and 
		    // will spawn another child
		    if( sig == SIGSEGV || sig == SIGABRT ) {
			 bzero( tmp, SMALL_BUFF_SIZE );
			 counter++;
			 
			 if(counter > INFOD_MAX_CRASH_NUM) {
			      infod_log(LOG_ERR, "Too much segmentation faults exiting");
			      infod_parent_exit(status);
			 }
			 
			 // Moving the corefile
			 sprintf( tmp, "%s/%s.%d", INFOD_CWD, INFOD_CORE_FILE_PREFIX, counter);

			 rename( INFOD_NATIVE_CORE_FILE, tmp );
			 // Cleaning the pipe of ctl
			 unlink(MSX_INFOD_CTL_SOCK_NAME);
			 return 1;
		    }
		    // Any other case the signal is raised for the parent as well
		    else 
			 raise( sig );
	       }
	  }
     }
}
