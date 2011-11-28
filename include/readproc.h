/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __MOSIX2_READ_PROC_
#define __MOSIX2_READ_PROC_

#ifdef  __cplusplus
extern "C" {
#endif


#define MAX_PROC_NAME (32)

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef struct  proc_entry_
{
	int             pid;
        int             tracedPid;
	char            name[MAX_PROC_NAME];

        int             uid, euid;
        
	char            state;
	unsigned long   virt_mem;
	long            rss_sz;

        unsigned long   utime;
        unsigned long   stime;

	char            whereIsHere;
	char            fromIsHere;
	struct in_addr  whereAddr;
	struct in_addr  fromAddr;
	int             freezer;

	struct proc_entry_ *next, *prev;
} proc_entry_t;



void print_proc_entry(proc_entry_t *e);
void print_mosix_processes(proc_entry_t *procArr, int size);


/* Read the status of a single process */
int read_process_stat(const char *proc_dir_name, proc_entry_t *e);
int read_process_status(const char *proc_dir_name, proc_entry_t *e);


typedef enum {
	MOS_PROC_ALL,        // All Mosix proceses 
	MOS_PROC_GUEST,      // Only guest processes
	MOS_PROC_LOCAL,      // Only local mosix processes from this node
	MOS_PROC_DEPUTY,     // Only local deputies (processes that are out)
	MOS_PROC_NON_DEPUTY, // Only local non deputies
	MOS_PROC_FROZEN      // Frozen local processes
} mosix_procs_type_t;
// Read /proc/pid and retrun information about all the LOCAL mosix processes
// The result is stored in buff which should have num entries, The number of total entries
// is stored in the num argument.
// Return 0 on error 1 on success
int get_mosix_processes(char *proc_name, mosix_procs_type_t ptype,
			proc_entry_t *buff, int *num);

int is_mosix_process_local(proc_entry_t *e);
int is_mosix_process_frozen(proc_entry_t *e);
int is_mosix_process_deputy(proc_entry_t *e);
int is_mosix_process_non_deputy(proc_entry_t *e);

int is_mosix_process_guest(proc_entry_t *e);


#ifdef  __cplusplus
}
#endif

#endif /* __MOSIX2_READ_PROC_ */
