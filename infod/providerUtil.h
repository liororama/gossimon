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


#ifndef _PROVIDER_UTIL_H
#define _PROVIDER_UTIL_H


/****************************************************************************
 * Some utility functions and structures for providers. Such as finding the
 * Disk io done and the network usage
 ***************************************************************************/
int read_proc_file(char *file);

#define PROC_BUFF_SZ      (16384)
extern char *proc_file_buff;

char *sgets(char *buff, int buff_len, char *data);

typedef struct __disk_io_info {
	unsigned long long read_bytes;
	unsigned long long write_bytes;
} disk_io_info_t;

typedef struct __net_io_info {
	unsigned long long rx_bytes;
	unsigned long long tx_bytes;
}net_io_info_t;

typedef struct __io_rates {
	unsigned int d_read_kbsec;
	unsigned int d_write_kbsec;
	unsigned int n_rx_kbsec;
	unsigned int n_tx_kbsec;
}io_rates_t;




int get_disk_io_info(disk_io_info_t *dio);
int get_net_io_info(net_io_info_t *nio);
void calc_io_rates(disk_io_info_t *d_old, disk_io_info_t *d_new,
		   net_io_info_t *n_old, net_io_info_t *n_new, 
		   struct timeval *t_old, struct timeval *t_new,
		   io_rates_t *rates);

typedef struct __nfs_client_info {
	unsigned long  rpc_ops;
	unsigned long  read_ops;
	unsigned long  write_ops;
	unsigned long  getattr_ops;
	unsigned long  other_ops;
} nfs_client_info_t;

int get_nfs_client_info(nfs_client_info_t *nci);
int calc_nfs_client_rates(nfs_client_info_t *nci_old,
			  nfs_client_info_t *nci_new,
			  struct timeval *t_old, struct timeval *t_new, 
			  nfs_client_info_t *nci_rates);


#define   CPU_INFO_PATH    "/proc/cpuinfo"
#define   CPU_MHZ_STR      "cpu MHz"

typedef struct __cpu_info {
	unsigned long   mhz_speed;        // Processor speed in Mhz
	int             ncpus;            // Number of cpus
	char            ht_on;            // is hyper threading on
	int             siblings;         // if ht is on what is the number of
	                                  // virtuall cpus for real one
	char            cpu_type[100];    // A string describing the cpu
} cpu_info_t;
int get_cpu_info(cpu_info_t *cpu_info);


#define LOADAVG_PATH    "/proc/loadavg"

typedef struct __linux_loadinfo {
	float      load[3];   // The load avg in 1 5 15 minutes
} linux_load_info_t;

int get_linux_loadavg(linux_load_info_t *load);


// The path to the proc file of loadavg2 module
#define LOADAVG2_PATH    "/proc/loadavg2"
int detect_linux_loadavg2(int *use_loadavg2);
int get_linux_loadavg2(linux_load_info_t *load);


#define RUNNING_ON_VM_PATH  "/etc/vm"
int running_on_vm();

int get_machine_arch_size(unsigned char *machine_arch_size);


#include <FreezeInfo.h>

// Free memory allocated by freeze_info_t
void free_freeze_dirs(freeze_info_t *fi);
// Find the list of freeze dirs
int get_freeze_dirs(freeze_info_t *fi, char *conf_file);
// Calculate the space used/total
int get_freeze_space(freeze_info_t *fi);


int get_kernel_release(char *buff);

#define PROC_STAT_FILE     "/proc/stat"
int update_io_wait_percent(char* io_wait_percent);



#endif /* _PROVIDER_UTIL_H */
