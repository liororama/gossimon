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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <msx_debug.h>
#include <msx_error.h>
#include <msx_proc.h>
#include <msx_common.h>
#include <parse_helper.h>

#include "readproc.h"
#include "debug_util.h"

int  globBuffSize = 4096;
char globBuff[4096];

void print_proc_entry(proc_entry_t *e)
{
	char whereStr[256];
	char fromStr[256];

	sprintf(fromStr, "%s", e->fromIsHere ? "here" : inet_ntoa(e->fromAddr));
	sprintf(whereStr,"%s", e->whereIsHere ? "here" : inet_ntoa(e->whereAddr));
	
	printf("PID %-5d: Name %-10s st: %1c wh: %-12s fr: %-12s frz: %d\n",
	       e->pid, e->name, e->state, whereStr, fromStr, e->freezer);
        printf("          tracedPid: %5d uid: %5d  euid: %5d\n",
               e->tracedPid, e->uid, e->euid);
        
}

void print_mosix_processes(proc_entry_t *procArr, int size) {
        int i;

        printf("Process List (%d)\n", size);
        printf("========================\n");
        for(i=0; i < size ; i++) {
                print_proc_entry(&procArr[i]);
        }
}

int read_process_stat(const char *proc_dir_name, proc_entry_t *e)
{
	FILE   *f;
	
	int pid;
	char state;
	int ppid;
	int pgrp;
	int session;
	int tty_nr;
	int tpgid;
	unsigned long  flags;
	unsigned long  mintflt;
	unsigned long  cmintflt;
	unsigned long  majflt;
	unsigned long  cmajflt;
	unsigned long  utime;
	unsigned long  stime;
	long  cutime;
	long  cstime;
	long  priority;
	long  nice;
	long  zero;
	long  itrealvalue;
	long  starttime;
	unsigned long vsize;
	long rss;
	unsigned long rlim;
	unsigned long startcode;
	unsigned long endcode;
	unsigned long startstack;
	unsigned long kstkesp;
	unsigned long kstkeip;
	unsigned long signal;
	unsigned long blocked;
	unsigned long sigignore;
	unsigned long sigcatch;
	unsigned long wchan;
	unsigned long nswap;
	unsigned long cnswap;
	int exit_signal;
	int processor;
	char    name[100];

	char    path[100];

	
	snprintf(path, 99, "%s/%s", proc_dir_name, "stat"); 
	path[99] = '\0';
	
	if((!(f = fopen(path, "r"))))
	{
		debug_lr(READPROC_DEBUG,  "Error opening where\n");
		return 0;
	}
	
	fscanf(f,"%d %s %c %d %d %d %d %d %lu %lu "
	       "%lu %lu %lu %lu %lu %ld %ld %ld %ld "
	       "%ld %ld %lu %lu %ld %lu %lu %lu %lu %lu "
	       "%lu %lu %lu %lu %lu %lu %lu %lu %d %d\n",
	       &pid,
	       name,
	       &state,
               &ppid,
	       &pgrp,
	       &session,
	       &tty_nr,
	       &tpgid,
	       &flags,
	       &mintflt,
	       &cmintflt,
	       &majflt,
	       &cmajflt,
	       &utime,
	       &stime,
	       &cutime,
	       &cstime,
	       &priority,
	       &nice,
	       &zero,
	       &itrealvalue,
	       &starttime,
	       &vsize,
	       &rss,
	       &rlim,
	       &startcode,
	       &endcode,
	       &startstack,
	       &kstkesp,
	       &kstkeip,
	       &signal,
	       &blocked,
	       &sigignore,
	       &sigcatch,
	       &wchan,
	       &nswap,
	       &cnswap,
	       &exit_signal,
	       &processor
		);

	e->state = state;
	e->virt_mem = vsize;
	e->rss_sz = rss;

        e->utime = utime;
        e->stime = stime;

        // Note that we remove the ( ) around the name
        strncpy(e->name, name+1, MAX_PROC_NAME -2);
        e->name[strlen(e->name)-1] = '\0';

	e->name[MAX_PROC_NAME -1] = '\0';
        e->pid = pid;
        fclose(f);
	return 1;
}

int parseStatusBuff(char *buff, int size, proc_entry_t *e)
{
        int     res = 0;

        char   *buffCurrPos = (char*) buff;
        int     lineBuffSize = 256;
        char    lineBuff[256];
        int     itemsFound = 0;
        int     itemsToFind = 2;
        
        while(buffCurrPos)
        {
                int   lineLen;
                char *newPos;

                newPos = buff_get_line(lineBuff, lineBuffSize, buffCurrPos);

                if(!newPos)
                        break;
                lineLen = newPos - buffCurrPos;
                debug_lg(READPROC_DEBUG, "Processing line (%d): %s\n", lineLen, lineBuff);


                buffCurrPos = newPos;
		if(strncmp("TracerPid:", lineBuff, 10) == 0) {
                        if(sscanf(lineBuff+10, "%d", &e->tracedPid) != 1)
                                e->tracedPid = -1;
                        itemsFound++;
                }
		
                if(strncmp("Uid:", lineBuff, 4) == 0) {
                        if(sscanf(lineBuff+4, "%d %d", &e->uid, &e->euid) != 2) {
                                e->uid  = -1;
                                e->euid = -1;
                        }
                        itemsFound++;
                }
                if(itemsFound == itemsToFind)
                        break;
        }
        
        res = 1;
        return res;
}

int read_process_status(const char *proc_dir_name, proc_entry_t *e)
{
        int            res = 0;
        int            fd = -1;
        int            fileSize = 0;
        char           fileName[100];
        
        
        snprintf(fileName, 99, "%s/%s", proc_dir_name, "status"); 
	fileName[99] = '\0';
        
	if((fd = open(fileName, O_RDONLY)) == -1)
                goto out;
        
	if((fileSize = read(fd, globBuff, globBuffSize)) == -1)
		goto out;
	
	globBuff[fileSize]='\0';
	close(fd);
        res = parseStatusBuff(globBuff, fileSize, e);
        
 out:
        if(fd != -1) close(fd);
        return res;
}

// We get information about local MOSIX processes, remote processes are ignored
int getMosixProcessEntry(char *proc_dir_name, mosix_procs_type_t ptype,
                         char *isMosixProc, proc_entry_t *e)
{
	int     freezer;
	char    path[100];
	int     strSize = 20;
	char    fromStr[20];
	char    whereStr[20];
	char    fromVal;
	char    whereVal;

        
	// First checking the from of the process and making sure
	// it is a MOSIX process that run on the local node
	snprintf(path, 99, "%s/%s", proc_dir_name, MSX_PROC_PID_FROM); 
	path[99] = '\0';
        
	if(!msx_readstr(path, fromStr, &strSize)) {
		debug_lr(READPROC_DEBUG, "Failed to read from value %s", path);
		return 0;
	}
	// Taking out the trailing \n
	if(fromStr[strSize-1] == '\n')
		fromStr[strSize-1] = '\0';
	
	// Not a mosix process
	if(strcmp(fromStr, MSX_PROC_PID_NO_MSX_STR) == 0) {
                *isMosixProc = 0;
                return 1;
	}
        *isMosixProc = 1;
        
	// Checking from where the process came
	if(strcmp(fromStr, MSX_FROM_IS_HERE_STR) == 0)
		fromVal = 0;
	else {
		fromVal = 1;
		// The address is not in the number and dots notation
		if(inet_aton(fromStr, &(e->fromAddr)) == 0)
			return 0;
	}
	
	// Filtering out
	if (ptype == MOS_PROC_GUEST && fromVal == 0)
		return 1;
	if (ptype == MOS_PROC_LOCAL && fromVal > 0)
		return 1;

	// Now reading the where an making sure it is running here
	snprintf(path, 99, "%s/%s", proc_dir_name, MSX_PROC_PID_WHERE); 
	path[99] = '\0';
	strSize = 20;
	if(!msx_readstr(path, whereStr, &strSize ))
		return 0;
	// Taking out the trailing \n
	if(whereStr[strSize-1] == '\n')
		whereStr[strSize-1] = '\0';
	
	if(strcmp(whereStr, MSX_WHERE_IS_HERE_STR) == 0) {
		whereVal = 0;
	}
	else {
		// The address is not in the number and dots notation
		if(inet_aton(whereStr, &(e->whereAddr)) == 0)
			return 0;
		whereVal = 1;
	}
	// More filtering out
	if (ptype == MOS_PROC_DEPUTY &&
	    (fromVal != 0 || (fromVal == 0 && whereVal == 0)))
		return 1;
	if (ptype == MOS_PROC_NON_DEPUTY &&
	    (fromVal != 0 || (fromVal == 0 && whereVal > 0 )))
		return 1;
	
	// Reading the freezer to see if it is running or not
	snprintf(path, 99, "%s/%s", proc_dir_name, MSX_PROC_PID_FREEZER); 
	path[99] = '\0';
	if(!msx_readval(path, &freezer))
		return 0;
	if (ptype == MOS_PROC_FROZEN && freezer <= 0)
		return 1;
	// Finaly we have the MOSIX process we are interested in
        debug_lg(READPROC_DEBUG, "Reading process: (from %d where %d freezer: %d path:%s\n",
                 fromVal, whereVal, freezer, path);
	if(read_process_stat(proc_dir_name, e) == 0) {
		return 0;
	}
	
	//printf("Got name %s\n", name);
	e->fromIsHere = (fromVal == 0);
	e->whereIsHere = (whereVal == 0);
	e->freezer = freezer;
	*isMosixProc = 2;

        
	return 1;
}

void updateTracerEntry(proc_entry_t *tracer, proc_entry_t *e) {
        
        tracer->tracedPid = e->pid;
        tracer->uid       = e->uid;
        tracer->euid      = e->euid;
}


/*
 * We have a traced process and we want to find the mosix process which trace
 * it. The best guess is to go from the end of the current detected mosix
 * processes since the pid of the traced process is always > pid of mosix
 * process.
 *
 * In case of strace and gdb we will go to the end of the array....
 *
 */
static
int updateTracerProcess(proc_entry_t *procArr, int size, proc_entry_t *e)
{
        int   i;

        if(!procArr || size <= 0 || !e)
                return 0;
        
        for(i = size -1 ; i >= 0 ; i--) {
                if(procArr[i].pid == e->tracedPid) {
                        debug_lg(READPROC_DEBUG, "Found tracer process %d", e->tracedPid);
                        updateTracerEntry(&procArr[i], e);
                        break;
                }
        }
        return 1;
}
        
int get_mosix_processes(char *proc_name, mosix_procs_type_t ptype,
			proc_entry_t *e , int *num)
{
        int            res = 0;
	DIR           *proc_dir;
	struct dirent *dir;
	int            pid;
	int            mosproc_num = 0;
	proc_entry_t   entry;
	char           buff[100];
	
	if(!(proc_dir = opendir(proc_name)))
	{
		debug_lr(READPROC_DEBUG, "Error opening dir %s\n", proc_name);
		return 0;
	}
	
	while( (dir = readdir(proc_dir)) && mosproc_num < *num )
	{
                char  isMosixProcess;
		if ((pid = atoi(dir->d_name)) == 0) continue;

		// We have a pid dir and we obtain the process information
		//printf("Got %s\n", dir->d_name);
		sprintf(buff, "%s/%s", proc_name, dir->d_name);
		entry.pid = pid;
		if(!getMosixProcessEntry(buff, ptype, &isMosixProcess, &entry))
		{
			debug_lr(READPROC_DEBUG, "Error getting process %s entry\n",
				 dir->d_name);
			goto out;
		}
		// The process is not MOSIX process or not local
		// (running elsewere)
		if(isMosixProcess == 0  ) {
                       	debug_ly(READPROC_DEBUG, "Process is not a mosix process %d\n", pid);
                        if(!read_process_status(buff, &entry)) {
                                debug_lr(READPROC_DEBUG, "Error cant read process status file\n");
                                goto out;
                        }
                        // This is process traced by a mosix process
                        if(entry.tracedPid > 0)
                                debug_lg(READPROC_DEBUG, "Process is traced by %d\n",
                                         entry.tracedPid);
                                updateTracerProcess(e, mosproc_num, &entry);
                        continue;
                }
                else if(isMosixProcess == 2) { 
                        //print_proc_entry(&entry);
                        e[mosproc_num++] = entry;
                }
                // Ignoring the case where isMosixProcess is 1 since it is a
		// mosix process but not of the requested type
                

        }

	// The caller of the function should check that the return value is
	// no more then num, we check it here only for printing
	if(mosproc_num >= *num) {
		debug_lr(READPROC_DEBUG, "Error too many mosix processes (%d %d)\n",
                         mosproc_num, *num);
		goto out;
	}

        debug_lb(READPROC_DEBUG, "Got %d mosix processes\n", mosproc_num);
	*num = mosproc_num;
        res = 1;

 out:
	closedir(proc_dir);
	return res;
}

inline int is_mosix_process_local(proc_entry_t *e) {
        return e->fromIsHere;
}
inline int is_mosix_process_frozen(proc_entry_t *e) {
        return (e->fromIsHere && e->freezer > 0);
}
inline int is_mosix_process_deputy(proc_entry_t *e) {
        return (e->fromIsHere && !e->whereIsHere);
}
inline int is_mosix_process_non_deputy(proc_entry_t *e) {
        return (e->fromIsHere && e->whereIsHere);
}
inline int is_mosix_process_guest(proc_entry_t *e) {
        return (!e->fromIsHere && e->whereIsHere);
}

#if 0
int test_get_mosix_processes(char *proc_name)
{
	proc_entry_t buff[1000];
	int size = 1000;
	int i,j;
        unsigned long beforeMem, afterMem; 

        beforeMem = getMyMemSize();
        
	for(j=0; j< 1000; j++) {
                size = 1000;
                if(!get_mosix_processes(proc_name, MOS_PROC_ALL, buff, &size))
                {
                        printf("get_mosix_processes() failed %d\n", j);
                        return 0;
                }
        }
        afterMem = getMyMemSize();
        printf("Before %ld After %ld\n", beforeMem, afterMem);
	//printf("Got %d mosix process\n", size);
	//for( i=0 ; i<size ; i++)
	//	print_proc_entry(&buff[i]);
	
	return 1;
}




int main(int argc, char **argv)
{
	//msx_set_debug(READPROC_DEBUG);
	test_get_mosix_processes("/proc");
	return 0;
}
#endif
