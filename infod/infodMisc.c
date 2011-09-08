/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

/****************************************************************************
 *
 * Authors: Lior Amar
 *
 ****************************************************************************/

#ifndef __USE_GNU
#define __USE_GNU
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <msx_debug.h>
#include <msx_error.h>

#include "infod.h"

void log_init(){
	openlog( "INFOD", 0, LOG_DAEMON ) ;
}

// FIXME shoud get ... (varargs)
void infod_log( int priority, char *fmt, ... ){
     int  len;
     int  newline = 0;
     char buf[1024];

     len = strlen(fmt);
     if(fmt[len-1] == '\n') 
	  newline = 1;

     va_list ap;
     
     va_start(ap, fmt);
     vsprintf(buf, fmt, ap);
     if(priority > LOG_WARNING)
	  debug_lg(INFOD_DEBUG, "LOG: %s%s", buf, newline ? "" : "\n");
     else {
	  if(glob_daemonMode) {
	       debug_lr(INFOD_DEBUG, "LOG: %s%s", buf, newline ? "" : "\n");
	  } else {
	       fprintf(stderr, "%s%s", buf, newline ? "" : "\n");
	  }
     }
     syslog(priority, "%s", buf ) ;
     va_end(ap);
}


void 
infod_critical_error( char *fmt, ... )
{
     char buf[4096];
     
     va_list ap;
     char *tmp;
     
     va_start(ap, fmt);
     vsprintf(buf, fmt, ap);
     tmp = ctime_fixed();
     fprintf(stderr, "\n%s  %s\n", tmp, buf);
     syslog( LOG_ERR, "%s", buf );
     
     va_end(ap);
     
     //if( errno != 0 ){
     //  strcat( tmp, ": " );
     //  strcat( tmp, strerror(errno));
     //}
     
     //fprintf(stderr, buf);
     //syslog( LOG_ERR, buf );
     infod_exit();
     
}


/*****************************************************************************
 * Write the pid file of the infod
 ****************************************************************************/
int createPidFile() {
     FILE          *fh = NULL ;
     int            pid = getpid();
     struct stat    st;
     
     // Checking if the file already exists
     if(stat(INFOD_DEF_PID_FILE, &st) == 0) {
          debug_lg(INFOD_DEBUG, "Pid file: %s : already exists\n", INFOD_DEF_PID_FILE);
          return 0;
     }
     
     if( !( fh = fopen( INFOD_DEF_PID_FILE, "w" )))
          infod_critical_error( "Error: Failed opening pid file\n" );
     

     fprintf( fh, "%d\n", pid ) ;
     fclose(fh);
     debug_lg(INFOD_DEBUG, "Created pid file: %s\n", INFOD_DEF_PID_FILE);
     return 1;
}

int removePidFile()
{
     int res;
     res = unlink(INFOD_DEF_PID_FILE);
     if(res == -1) {
          infod_log(LOG_ERR, "Error removing pid file");
          return 0;
     }
     return 1;
}

/* Check for the existance of the pid file
 * Return value: 
 *  0 if pid file does not exists
 */
int pidFileExists() {
  if(access(INFOD_DEF_PID_FILE, F_OK) == 0)
    return 1;
  return 0;
}
