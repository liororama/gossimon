/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __PROCESS_WATCH_INFO
#define __PROCESS_WATCH_INFO


typedef enum {
	PS_NO_INFO = 0,
	PS_NO_PID_FILE,
	PS_ERR_PID_FILE,
	PS_ERR_NO_PID,
	PS_ERR,
	PS_NO_PROC,
	PS_OK,
} proc_status_t;

#define PROC_NAME_LEN    (128)
typedef struct proc_watch_entry {
	char           procName[PROC_NAME_LEN];
	pid_t          procPid;
	proc_status_t  procStat;
} proc_watch_entry_t;

/*
Format of xml 
<proc name="priod" pid=1923 stat="no-info" />

*/

char *procStatusStr(proc_status_t st);
int   strToProcStatus(char *str, proc_status_t *stat);

// Translating from a string to a list of proc-watch entries
int  getProcWatchInfo(char *str, proc_watch_entry_t *procArr, int *size);
// Converting to a string a list of proc-watch entries
int  writeProcWatchInfo(proc_watch_entry_t *procArr, int size, int no_root, char *buff);

int checkProcPidFile(char *pidFile, proc_watch_entry_t *ent);

#endif
