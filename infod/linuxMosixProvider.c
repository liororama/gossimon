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
 * Author(s): Amar Lior, Ilan Peer
 *
 *****************************************************************************/

/******************************************************************************
 * File: mosix_provider.c. Implementation of the mosix specific functions
 *
 * This module can operate in two modes. With mosixd as porivder, and
 * without a provider. The no provider mode is to enable sharing of non
 * MOSIX nodes with MOSIX nodes in the same cluster.
 * The way to distinguish between such nodes will be that the status 
 *****************************************************************************/

#include <stdio.h>
//#define _
//#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <errno.h>

#include <info.h>
#include <msx_error.h>
#include <msx_debug.h>
#include <msx_proc.h>
#include <provider.h>
#include <infod.h>
#include <linuxMosixProvider.h>
//#include <mconf.h>
#include <parse_helper.h>
#include <infoModuleManager.h>
/****************************************************************************
 * Global variables
 ***************************************************************************/

static struct frequency_request  mosix_periodic_data;
static struct mosix_infod_data   myInfo;
static char glob_kernel_release_buff[200];
static char mosix_info_xml[8192];

// FIXME OK for the time being we hard coding topology size into mosix.
// We will make it a parameter passed to us via the init method.

typedef enum {
     MOSIX_LOCAL_PROVIDER=0,
     LINUX_LOCAL_PROVIDER,
     EXTERNAL_PROVIDER,
} mosix_provider_t;

char *providerNameList[] = {
     "mosix",
     "linux",
     "external"
};

static mosix_provider_t   mosix_provider_type = MOSIX_LOCAL_PROVIDER;

// Status of the provider 0=bad 1=ok
static int mosix_provider_status = 0;  
//static int            mosix_ntopology    = 0;
//static int            mosix_my_pe        = 0;
//static int            mosix_pe_diff_mode = 0;

static unsigned short mosix_base_status = 0;   // Base status (provider + vm);

static int            mosix_fixed_info_size     = 0;
//static int            mosix_const_size    = 0;
static int            mosix_infod_info_sz = 0;

//static mconf_t        mosix_mconf = NULL;
static char*          *mosix_dirs2watch[10]; // The directory we watch (statfs) 

// External information data
static char           *mosix_external_file_name = NULL;
static int             mosix_external_status = EXTERNAL_STAT_NO_INFO;
static time_t          mosix_external_mtime;   // The time external file was modified
static int             mosix_external_info_size = 0;
static int             mosix_external_buff_size = 0;
static char           *mosix_external_buff = NULL;


static map_change_hndl_t map_change_hndl = NULL;

/* A pointer to the perdiodic admin function, which is the only thing that
   should change when working with or without provider
*/
typedef int    (*periodic_admin_func_t)    (ivec_t vec);
periodic_admin_func_t        periodic_admin_func;

/* A pointer to the local provider information update functions, one for the
   regular (fast) update and one for the slow
*/
typedef void (*lp_update_info_func_t)   (struct mosix_infod_data *info, struct timeval *t);
lp_update_info_func_t        lp_fast_update_info_func;
lp_update_info_func_t        lp_slow_update_info_func;


// Linux provider info
static int             linux_use_loadavg2=0;

#define SELECTED_MAX_SIZE  (50)
node_t mosix_selected_nodes[ SELECTED_MAX_SIZE ];


/******************************************************************************
 ******************* Provider Modules *****************************************
 ******************************************************************************/

static char  *infod_config_file = NULL;

pim_t  mosix_pim;

/********************** Function Declaration ******************************/
static int lp_periodic_admin( ivec_t vec );
static int ep_periodic_admin( ivec_t vec );


inline void provider_status_ok() { mosix_provider_status = 1;};
inline void provider_status_fail() { mosix_provider_status = 0;};


/****************************************************************************
 * Computes the age of the information
 ***************************************************************************/
/* static double */
/* mosix_age( struct timeval *time, struct timeval *cur ) { */

/* 	double a, b; */
/* 	a = (double)(cur->tv_sec  - time->tv_sec);   */
/* 	b = (double)(cur->tv_usec - time->tv_usec); */
/* 	return (a * (double)(MILLI) + b); */
/* } */

/********************************************************************
 Compute the time difference between end and start in milli seconds
********************************************************************/
static inline
unsigned long milli_time_diff(struct timeval *start, struct timeval *end) {
     unsigned long time_diff;

     time_diff = 1000 * (end->tv_sec - start->tv_sec);
     if(time_diff == 0) 
	  time_diff = end->tv_usec/1000 - start->tv_usec/1000;
     else 
	  time_diff += end->tv_usec/1000 + (1000 - start->tv_usec/1000);
     return time_diff;
}
/****************************************************************************
 * Compare two entries in the vector according to their age. If the
 * timestamp is bigger than the entry is younger. The function is used by
 * qsort
 ***************************************************************************/
/*static
  int sort_by_age( const void *v1, const void *v2 ) {
	
  ivec_entry_t *n1 = *(ivec_entry_t**)v1;
  ivec_entry_t *n2 = *(ivec_entry_t**)v2;

  if( timercmp( &(n1->info->hdr.time), &(n2->info->hdr.time), >))
  return -1;
  else if( timercmp( &(n1->info->hdr.time), &(n2->info->hdr.time), <))
  return 1;

  return 0;
  }
*/
/****************************************************************************
 * Determine which nodes are to be sent to the mosix kernel. The criteria:
 * 1. Not mosix_my_pe (the local node).
 * 2. Not older than a specified age (done by the vector query).
 * 3. Not one that was sent recently.
 * Currently, use the magic field to mark entries that were sent
 * by us. Every time we get new info the value of the magic field in the
 * entry will be set to 1.
 ****************************************************************************/
static int
select_info_to_send( ivec_entry_t **vec_entries, int how_many,
		     node_t *selected_nodes, int *selected_size ) {
	
/*	int i = 0, j = 0;
	ivec_entry_t       *entry;
	node_hdr_t         *nhdr = NULL;

	
	if( how_many == 0 ) {
	*selected_size = 0;
	return 1;
	}
	
	if( *selected_size < mosix_periodic_data.ninfo ) {
	debug_lr( KCOMM_DEBUG, "Request (%d) nodes got (%d) spaces\n",
	mosix_periodic_data.ninfo, *selected_size );
	return 0;
	}

    	// Sort the entries by age 
	qsort( vec_entries, how_many, sizeof(ivec_entry_t*), sort_by_age );

	// Take the first entries that does not have their
	// extra magic marked as 1
	for( i = 0, j = 0; i <how_many ; i++ ) {
	entry = vec_entries[ i ]; 
	nhdr = (node_hdr_t*)(entry->info);
		
	if( !nhdr->pe ||
	(nhdr->pe == mosix_my_pe) ||
	(nhdr->status & MSX_STATUS_BLOCK ) ||
	(nhdr->status & MSX_STATUS_QUIET))
	continue;

	if( entry->reserved == 0 )	{
	entry->reserved = 1;
	selected_nodes[j++] = i;
	if( j == *selected_size )
	break;
	}
	}

	if( j == 0 ) {
	//	debug_lr( KCOMM_DEBUG, "Found nothing to send\n");
	;
	}
	else {
	// Found some candidates. Use permutation to break order
	// (to avoid some side effects on clusters with a small
	// number of nodes)
	for( i=0; i <mosix_periodic_data.ninfo; i++ ) {
	int tmp;
	int swap_with= random()%j;
			
	tmp = selected_nodes[i];
	selected_nodes[i] = selected_nodes[swap_with];
	selected_nodes[swap_with] = tmp;
	}
	}
	
	*selected_size = (j < mosix_periodic_data.ninfo)?
	j:mosix_periodic_data.ninfo;

*/
     return 1;
}

/***************************************************************
 * Called usually once to retrieve the kernel version
 ***************************************************************/
static int get_mosix_kernel_version(unsigned int *kernelVer) {
     int fd;
     char buff[100];
     char mosixStr[50];
     char versionStr[50];
     int a,b,c;

     if((fd = open(MOSIX_KERNEL_VER_FILENAME, O_RDONLY)) == -1)
	  return 0;

     read(fd, buff, 99);
     // Too much information in the version file
     if(fd >= 99)
	  return 0;
     // Parsing it
     if(!sscanf(buff, "%s %s %d.%d.%d", mosixStr, versionStr, &a, &b, &c)) {
	  //fprintf(stderr, "Error in scanf\n");
	  return 0;
     }
     //fprintf(stderr, "Got %s %s %d %d %d\n", mosixStr, versionStr, a, b, c);
     *kernelVer = ((a & 0xff)) <<16 | ((b & 0xff)) <<8 | ((c & 0xff));
     return 1;
}



static void update_io_rates(struct mosix_infod_data *info) {
     static disk_io_info_t d_new, d_old;
     static net_io_info_t n_new, n_old;
     static nfs_client_info_t nci_new, nci_old, nci_rates;
     static io_rates_t rates;
     static struct timeval t_new, t_old;

     // Getting io/net rates
     get_disk_io_info(&d_new);
     get_net_io_info(&n_new);

     bzero(&nci_new, sizeof(nfs_client_info_t));
     get_nfs_client_info(&nci_new);

     gettimeofday(&t_new, NULL);
     calc_io_rates(&d_old, &d_new,
		   &n_old, &n_new,
		   &t_old, &t_new,
		   &rates);

     calc_nfs_client_rates(&nci_old, &nci_new,
			   &t_old, &t_new,
			   &nci_rates);
	
     debug_ly( KCOMM_DEBUG,"Rate: Disk (R %u) (W %u) Net (RX %u) (TX %u)\n",
	       rates.d_read_kbsec, rates.d_write_kbsec,
	       rates.n_rx_kbsec, rates.n_tx_kbsec);
     n_old = n_new;
     d_old = d_new;
     nci_old = nci_new;
     t_old = t_new;

     info->extra.disk_read_rate = rates.d_read_kbsec;
     info->extra.disk_write_rate = rates.d_write_kbsec;
     info->extra.net_rx_rate = rates.n_rx_kbsec;
     info->extra.net_tx_rate = rates.n_tx_kbsec;
     info->extra.rpc_ops = nci_rates.rpc_ops;
}

/**
 * Updating the disk capacity part of the information. This require to run
 * statfs on the selected disk mount point
 */
static void update_disk_capacity(struct mosix_extra_data *extra) {
     struct statfs buf;
     double ratio;
     int i=0;
	
     extra->tdisk = 0;
     extra->fdisk = 0;
	
     // Nothing to do
     if(!mosix_dirs2watch[0]) return;
     while(mosix_dirs2watch[i]) {
	  if(statfs((const char *)mosix_dirs2watch[i], &buf) != -1) {
	       ratio = (double)(buf.f_bsize)/(double)(MEM_UNIT_SIZE);
	       extra->tdisk += (double)buf.f_blocks * ratio;
	       extra->fdisk += (double)buf.f_bavail * ratio;
			
	  }
	  i++;
     }
     debug_ly( KCOMM_DEBUG,"Disk Capacity T %ld F %ld\n",
	       extra->tdisk, extra->fdisk);
	
}

static int update_external_file(struct mosix_infod_data *info)
{
     int     res = 0;
     int     fd = -1;
     int     size;
     struct  stat st;
        
     mosix_external_info_size = 0;
     if(!mosix_external_file_name) {
	  res = 1;
	  mosix_external_status = EXTERNAL_STAT_NO_INFO;
	  goto out;
     }
        
     mosix_external_status = EXTERNAL_STAT_ERROR;
        
     fd = open(mosix_external_file_name, O_RDONLY);
     if(fd == -1) {
	  debug_lr( KCOMM_DEBUG, "Error opening external info file\n");
	  goto out;
     }
        
     size = read(fd, mosix_external_buff, mosix_external_buff_size-1);
     if(size == -1) {
	  debug_lr( KCOMM_DEBUG, "Error reading external info\n");
	  goto out;
     }

                
     debug_lg(KCOMM_DEBUG, "Got external file:\n%s\n", mosix_external_buff);
     mosix_external_buff[size] = '\0';
     mosix_external_info_size = size+1;
        
     // Doing stat for the mtime of the file
     if(fstat(fd, &st) == -1){
	  debug_lr( KCOMM_DEBUG, "Error stating external file\n");
	  goto out;
     }
        
     mosix_external_mtime = st.st_mtime;
     if(size > 0)
	  mosix_external_status = EXTERNAL_STAT_INFO_OK;
     else
	  mosix_external_status = EXTERNAL_STAT_EMPTY;
     res = 1;
out:
     if(fd != -1) close(fd);
     return res;
}

static int fix_64bit_mosix_data(struct mosix_data *data) {
     struct mosix_data_64bit data64 = *((struct mosix_data_64bit *)data);
     struct mosix_data data32;

     debug_lb(KCOMM_DEBUG, "Fixing from 64bit mosix data to 32bit\n");

     data32.freepages   = data64.freepages;
     data32.totpages    = data64.totpages;
     data32.load        = data64.load;
     data32.speed       = data64.speed;
     data32.status      = data64.status;
     data32.level       = data64.level;
     data32.frozen      = data64.frozen;
     data32.util        = data64.utilization;
     data32.ncpus       = data64.ncpus;
     data32.nproc       = data64.nproc;
     data32.token_level = data64.tokenlevel;
     
     *data = data32;
     return 1;
}
int get_mosix_info_2_28_1(struct mosix_data* tmpInfo, struct timeval* currTime){
     int fd = -1;
     struct stat mosinfoStat;
	  
     fd = open(MOSIX_SELF_INFO_FILENAME, O_RDONLY);
     if(fd == -1) {
	  debug_lr( KCOMM_DEBUG, "Error opening self info file\n");
	  goto failed;
     }
  
     // Doing stat for the mtime of the file
     if(fstat(fd, &mosinfoStat) == -1){
	  debug_lr( KCOMM_DEBUG, "Error stating mosinfo file file\n");
	  goto failed;
     }
     
     // Checking it is not too old
     int sec = currTime->tv_sec - mosinfoStat.st_mtime;
     if(sec > 60) {
	  debug_lr( KCOMM_DEBUG, "Error, modification time of self mosinfo file is > 60 (%d)\n", sec);
	  goto failed;
     }
  
     struct mosix_data_2_28_1 data;
     int res;
     // Reading it 
     res = read(fd, &data, sizeof(struct mosix_data_2_28_1));
     if(res != sizeof(struct mosix_data_2_28_1)) {
	  debug_lr(KCOMM_DEBUG, "Error, self info file is not long enough 128 > %d\n", res);
	  goto failed;
     }
     close(fd);


     
     // Converting the fields to the mosix_data structure...
     tmpInfo->freepages = data.freepages;
     tmpInfo->totpages = data.totpages;
     /*   tmpInfo->INTERNAL_1 = data.INTERNAL_1;
	  tmpInfo->INTERNAL_2 = data.INTERNAL_2;*/
     tmpInfo->load = data.load;
     tmpInfo->speed = data.speed;
//     tmpInfo->INTERNAL_3 = data.INTERNAL_3;
     tmpInfo->status = data.status;
     tmpInfo->level = data.level;
     tmpInfo->frozen = data.frozen;
     tmpInfo->util = data.util;
     tmpInfo->ncpus = data.ncpus;
     tmpInfo->token_level = data.token_level;
     tmpInfo->nproc = data.nproc;
     tmpInfo->batches = data.batches;
     tmpInfo->totswap = data.totswap;
     tmpInfo->freeswap = data.freeswap;     


     // Generating the mosix info XML
     char *tmp = mosix_info_xml;
     
     tmp += sprintf(tmp, "<%s>\n", ITEM_MOSIX_INFO_NAME);
     tmp += sprintf(tmp, "\t<%s>\n", "openCL");
     tmp += sprintf(tmp, "\t\t<total_gpus> %u </total_gpus>\n", data.total_gpus);
     tmp += sprintf(tmp, "\t\t<avail_gpus> %u </avail_gpus>\n", data.avail_gpus);
     tmp += sprintf(tmp, "\t\t<total_cpus> %u </total_cpus>\n", data.total_cpus);
     tmp += sprintf(tmp, "\t\t<avail_cpus> %u </avail_cpus>\n", data.avail_cpus);
     tmp += sprintf(tmp, "\t</%s>\n", "openCL");
     tmp += sprintf(tmp, "</%s>", ITEM_MOSIX_INFO_NAME);

     return 1;

failed:
     close(fd);
     return 0 ;
}

int get_mosix_info_old(struct mosix_data* tmpInfo, struct timeval* currTime, struct in_addr *myaddr) {

     int fd=-1;
     int mypos;
     unsigned long offset;
     struct stat mosinfoStat;
     static mosmap_entry_t mapentry;
	
     // Opening the mosix info file. First we try the new file
     fd = open(MOSIX_INFO_FILENAME, O_RDONLY);
     if(fd == -1) {
	  debug_lr( KCOMM_DEBUG, "Error opening info file\n");
    
	  // Trying to open the old file/
	  fd = open(MOSIX_INFO_FILENAME_OLD, O_RDONLY);
	  if(fd == -1) {
	       debug_lr( KCOMM_DEBUG, "Error opening old info file\n");
	       goto failed;
	  }
     }
  
     // Doing stat for the mtime of the file
     if(fstat(fd, &mosinfoStat) == -1){
	  debug_lr( KCOMM_DEBUG, "Error stating mosinfo file file\n");
	  goto failed;
     }
  
     int sec = currTime->tv_sec - mosinfoStat.st_mtime;
     if(sec > 60) {
	  debug_lr( KCOMM_DEBUG, "Error, modification time of mosinfo file is > 60 (%d)\n", sec);
	  goto failed;
     }
  
     mypos = 0;
     do {
	  if(read(fd, &mapentry, sizeof(mosmap_entry_t)) == -1) {
	       debug_lr( KCOMM_DEBUG, "Error reading info file\n");
	       goto failed;
	  } 
    
	  debug_ly(KCOMM_DEBUG, "Got ip %ld N: %hd incluster %hd\n",
		   mapentry.ip, mapentry.n, mapentry.in_cluster);
	  if(mapentry.ip == 0)
	       break;
	  if(myaddr->s_addr >= mapentry.ip && myaddr->s_addr < (mapentry.ip + mapentry.n))
	  {
	       debug_lr(KCOMM_DEBUG, "Found my range\n");
	       mypos += myaddr->s_addr - mapentry.ip;
	       break;
	  }
	  mypos += mapentry.n;
     } while(mapentry.ip != 0);
  
     // Our ip was not found in the map
     if(mapentry.ip == 0) {
	  debug_lr(KCOMM_DEBUG, "Could not find my IP\n");
	  goto failed;
     }
     debug_lr(KCOMM_DEBUG, "Mypos is %d\n", mypos);
     // Reading my information
     offset = 2048 + mypos * sizeof(struct mosix_data);
     if(pread(fd, tmpInfo, sizeof(struct mosix_data), offset) == -1) {
	  debug_lr(KCOMM_DEBUG, "Error reading infofile at position");
	  goto failed;
     }
     close(fd);
     int wordSize = __WORDSIZE;
  
     // Following line is used only when a 32bit infod is used on a 64bit system
     if(wordSize == 32 && myInfo.extra.machine_arch == 64) {
	  fix_64bit_mosix_data(tmpInfo);
     }

     return 1; 
failed:
     close(fd);
     return 0 ;
}

static int get_mosix_local_info(struct mosix_infod_data *info, struct timeval *currTime) {
     struct in_addr myaddr;
     int fd=-1;
     struct mosix_data tmpInfo;

     char buff[100];
	
	
     fd = open(MOSIP_FILENAME, O_RDONLY);
     if(fd == -1) {
	  debug_lr( KCOMM_DEBUG, "Error opening mosip file\n");
	  goto failed;
     }
     if(read(fd, buff, 100) == -1) {
	  debug_lr( KCOMM_DEBUG, "Error reading mosip file\n");
	  goto failed;
     }
     close(fd); fd=-1;
	
     // Moving address to binary form
     if(inet_aton(buff, &myaddr) == -1) {
	  debug_lr( KCOMM_DEBUG, "Error converting string ip to ip\n");
	  goto failed;
     }
     myaddr.s_addr = ntohl(myaddr.s_addr);
     
     unsigned int version_2_28_1 = (2<<16)|(28<<8)|1; 
     
     if(myInfo.extra.kernel_version >= version_2_28_1) {
	  debug_lg(KCOMM_DEBUG, "Version is >= 2.28.1\n");
	  if(!get_mosix_info_2_28_1(&tmpInfo, currTime))
	       goto failed;
     }
     else {
	  debug_lg(KCOMM_DEBUG, "Version is < 2.28.1 (old version)\n");
	  if(!get_mosix_info_old(&tmpInfo, currTime, &myaddr)) 
	       goto failed;
     }
	
     debug_lg(KCOMM_DEBUG, "Got info: load %d speed %d ncpus: %hd util %hd \n",
	      tmpInfo.load, tmpInfo.speed, tmpInfo.ncpus, tmpInfo.util);
     info->data = tmpInfo;
     info->extra.tmem = tmpInfo.totpages;

     return 1;

failed:
     close(fd);
     return 0 ;
}
/*********************************************************************
  Updating information in no-provider mode.
**********************************************************************/
static void lp_mosix_fast_update_info(struct mosix_infod_data *info, struct timeval *currTime)
{
     // Getting the extra stuff
     struct sysinfo si;
     double ratio = 0.0;

     // Getting local mosix info
     if(!get_mosix_local_info(info, currTime)) {
	  provider_status_fail();
	  return;
     }
	
     // Get the total amount of memory, total swap and free swap
     if( sysinfo( &si ) == -1) {
	  info->extra.tswap = 0;
	  info->extra.fswap = 0;
     }
     else {
	  ratio = (double)(si.mem_unit)/(double)(MEM_UNIT_SIZE);
	  info->extra.tswap      = (double)si.totalswap * ratio; 
	  info->extra.fswap      = (double)si.freeswap * ratio;
	  info->extra.uptime     = si.uptime;
     }

     // Getting grid information
     int locals = -1, guests = -1, maxguests = -1;
     if(!msx_readval(LOCALS_FILENAME, &locals))
	  locals = -1;
     if(!msx_readval(GUESTS_FILENAME, &guests))
	  guests = -1;
     if(!msx_readval(MAXGUESTS_FILENAME, &maxguests))
	  maxguests = -1;
     info->extra.locals = locals;
     info->extra.guests = guests;
     info->extra.maxguests = maxguests;

     update_io_rates(info);
     provider_status_ok();
     pim_update(mosix_pim, currTime);
     return;

}

static void lp_linux_fast_update_info(struct mosix_infod_data *info, struct timeval *currTime)
{

     struct sysinfo si;
     double ratio = 0.0;
     linux_load_info_t load;
	
     // Get the total amount of memory, total swap and free swap
     if( sysinfo( &si ) == -1) {
	  info->extra.tmem  = 0;
	  info->extra.tswap = 0;
	  info->extra.fswap = 0;
     }
     else {
	  ratio = (double)(si.mem_unit)/(double)(MEM_UNIT_SIZE);
	  info->extra.tmem       = (double)si.totalram  * ratio; 
	  info->data.freepages   = (double)(si.freeram + si.bufferram) * ratio; 
	  info->extra.tswap      = (double)si.totalswap * ratio; 
	  info->extra.fswap      = (double)si.freeswap * ratio;
	  info->extra.uptime     = si.uptime;
	  debug_lg(KCOMM_DEBUG, "Got tmem  %d free %d\n",
		   info->extra.tmem, info->data.freepages);
	  debug_lg(KCOMM_DEBUG, "Got total %d ratio %f unit %d\n",
		   si.totalram, ratio, si.mem_unit);
     }

     if(linux_use_loadavg2)
	  get_linux_loadavg2(&load);
     else 
	  get_linux_loadavg(&load);
	
     info->data.load  =
	  ((load.load[0] * 100.0) / (float)info->data.ncpus) /
	  ((float)info->data.speed/ (float)MOSIX_STD_SPEED);
     debug_lg(KCOMM_DEBUG, "+++++ Got linuxload %.2f load %d speed %d ncpu %d (loadavg2 %d)\n",
	      load.load[0], 
	      info->data.load, info->data.speed, info->data.ncpus,
	      linux_use_loadavg2);
     update_io_rates(info);
     update_io_wait_percent(&(info->extra.io_wait_percent));
     provider_status_ok();
     pim_update(mosix_pim, currTime);
}

static void lp_linux_slow_update_info(struct mosix_infod_data *info, struct timeval *currTime)
{
     debug_lb(KCOMM_DEBUG, "LINUX slow update \n");
     
     update_disk_capacity(&info->extra);
     
     // Updating the external information
     update_external_file(info);
     detect_linux_loadavg2(&linux_use_loadavg2);
}


/** Updating extra information which is not urgent and can be updated in a
    slower rate
*/
static void
ep_slow_update_extra_info(struct mosix_extra_data *extra) {
     struct sysinfo si;
     double ratio = 0.0;
	
     // Get the total amount of memory, total swap and free swap
     if( sysinfo( &si ) == -1) {
	  extra->tmem  = 0;
	  extra->tswap = 0;
	  extra->fswap = 0;
		
     }
     else {
	  ratio = (double)(si.mem_unit)/(double)(MEM_UNIT_SIZE);
	  extra->tmem  = si.totalram  * ratio; 
	  extra->tswap = si.totalswap * ratio; 
	  extra->fswap = si.freeswap * ratio;
	  extra->uptime = si.uptime;
     }
	
     update_disk_capacity(extra);
}
/** Updating extra information which is urgent and should have come from mosixd
    but we add it ourselfs
*/
static void
ep_fast_update_extra_info(struct mosix_extra_data *extra)
{
     //static int mconf_check = 0;
     //static disk_io_info_t d_new, d_old;
     //static net_io_info_t n_new, n_old;
     //static io_rates_t rates;
     //static struct timeval t_new, t_old;

	
     int locals = -1, guests = -1, maxguests = -1;
     // Getting local
     if(!msx_readval(LOCALS_FILENAME, &locals))
	  locals = -1;
     if(!msx_readval(GUESTS_FILENAME, &guests))
	  guests = -1;
     if(!msx_readval(MAXGUESTS_FILENAME, &maxguests))
	  maxguests = -1;
     extra->locals = locals;
     extra->guests = guests;
     extra->maxguests = maxguests;
     // Trying to initialize the mconf if it is not already init.
     // FIXME we should check that off mode happend and close mconf
     //if(mconf_check == 0 && !mosix_mconf)
     //mosix_mconf = mconf_init(MCONF_FILENAME);

     // Trying to initialize mconf only once in 20 interations
     //mconf_check = (mconf_check + 1) % 20;

     // We enter here only if we have a valid mconf
     //if(mosix_mconf) {
     //mconf_is_manual_presedence_mode(mosix_mconf, &extra->manual_mode);
     //mconf_get_min_guest_dist(mosix_mconf, &extra->min_guest_dist);
     //mconf_get_max_guest_dist(mosix_mconf, &extra->max_guest_dist);
     //mconf_get_max_mig_dist(mosix_mconf, &extra->max_mig_dist);
     //mconf_get_min_run_dist(mosix_mconf, &extra->min_run_dist);
     //mconf_get_mos_procs(mosix_mconf, &extra->mos_procs);
     //mconf_get_ownerd_status(mosix_mconf, &extra->ownerd_status);
     //} else {

     //	extra->manual_mode = -1;
     //extra->min_guest_dist = -1;
     //extra->max_guest_dist = -1;
     //extra->max_mig_dist = -1;
     //extra->min_run_dist = -1;
     //extra->mos_procs = -1;
     //extra->ownerd_status = 0;
     //}
     provider_status_ok();
}

/****************************************************************************
 * Interface functions
 ***************************************************************************/

static int parse_provider_args(int argc, char **argv) {
     int i;
     // Handling the provider arguments. Which should come in pairs of name and value
     if( argc > 0 && ((argc % 2) == 0)) {
	  for(i=0 ; i<argc ; i+=2) {
	       //if(strcmp(argv[i], "MAX_MOSIX_TOPOLOGY") == 0) {	
	       //	mosix_ntopology = atol(argv[i+1]);
	       //	if( mosix_ntopology > MAX_MOSIX_TOPOLOGY )
	       //		mosix_ntopology = MAX_MOSIX_TOPOLOGY;
	       //	debug_lg(KCOMM_DEBUG, "Got ntopology %s\n", argv[i+1]);				
	       //}
	       if (strcmp(argv[i], "WATCH_FS") == 0) {
		    char *tmp = strdup(argv[i+1]);
		    // Parsing the string and looking for multiple dirs with a comman
		    split_str(tmp, (char **)mosix_dirs2watch, 10, ",");
				
		    int j=0;
		    while(mosix_dirs2watch[j]) { 
			 debug_lg(KCOMM_DEBUG, "Got FS to watch: %s\n",
				  mosix_dirs2watch[j]);
			 j++;
		    }
	       }
	       else if (strcmp(argv[i], "INFO_FILE") == 0)
	       {
		    mosix_external_file_name = strdup(argv[i+1]);
		    mosix_external_buff_size = MSX_INFOD_MAX_ENTRY_SIZE  - MOSIX_INFOD_DATA_SIZE - NODE_HEADER_SIZE;
		    mosix_external_buff = malloc(mosix_external_buff_size + 100);
		    if(!mosix_external_buff) {
			 debug_lr(KCOMM_DEBUG, "Error allocating buffer for external file\n");
		    }
	       }
	       else if (strcmp(argv[i], "PROC_FILE") == 0)
	       {
		    char *procFile = argv[i+1];
		    int   fd;
		    char *b; 
		    int   res;

		    fd = open(procFile, O_RDONLY);
		    if(fd == -1) {
			 msx_sys_error(stderr, "Error opening proc-file %s", procFile);
			 return 0;
		    }
		    if(!(b=malloc(1024)))
			 return 0;

		    res = read(fd, b, 1023);
		    if(res < 0) {
			 msx_sys_error(stderr, "Error reading proc-file: %s\n", procFile);
			 free(b);
			 return 0;
		    }
		    b[1023] = '\0';
		    pim_setInitData(pim_getDefaultPIMs(), ITEM_PROC_WATCH_NAME, b);
	       }
	       else if (strcmp(argv[i], "INFOD_DEBUG") == 0)
	       {
		    pim_setInitData(pim_getDefaultPIMs(), ITEM_INFOD_DEBUG_NAME, argv[i+1]);
	       }
	       else if (strcmp(argv[i], "ECO_FILE") == 0)
	       {
		    pim_setInitData(pim_getDefaultPIMs(), ITEM_ECONOMY_STAT_NAME, argv[i+1]);
	       }
	       else if (strcmp(argv[i], "JMIG_FILE") == 0)
	       {
		    pim_setInitData(pim_getDefaultPIMs(), ITEM_JMIGRATE_NAME, argv[i+1]);
	       }
	       else if (strcmp(argv[i], ITEM_NET_WATCH_NAME) == 0)
	       {
		    pim_setInitData(pim_getDefaultPIMs(), ITEM_NET_WATCH_NAME, argv[i+1]);
	       }
			
	       else if (strcmp(argv[i], "CONFIG_FILE") == 0) {
		    infod_config_file = strdup(argv[i+1]);
		    debug_lg(KCOMM_DEBUG, "Got config file: [%s]\n",
			     argv[i+1]);				
	       }
			
	       else if (strcmp(argv[i], "PROVIDER_TYPE") == 0) {
		    if(strcmp(argv[i+1], "mosix") == 0)
			 mosix_provider_type = MOSIX_LOCAL_PROVIDER;
		    else if (strcmp(argv[i+1], "linux") == 0)
			 mosix_provider_type = LINUX_LOCAL_PROVIDER;
		    else if (strcmp(argv[i+1], "external") == 0)
			 mosix_provider_type = EXTERNAL_PROVIDER;
		    else
			 mosix_provider_type = MOSIX_LOCAL_PROVIDER;
		    debug_lg(KCOMM_DEBUG, "Got provider type: %s\n", argv[i+1]);				
	       }
	       else {
		    debug_lg(KCOMM_DEBUG, "Kcomm got unsupported argument %s\n", argv[i]);				
	       }
	  }
     }
     return 1;
}
	

/****************************************************************************
 * Initiation.
 * If argc > 0, then the first argument holds the value of the topology
 ***************************************************************************/
int
mosix_sf_init( int local_provider, int argc, char **argv )
{
        
     if(!parse_provider_args(argc, argv))
	  return 0; 
	
     bzero( &myInfo, MOSIX_IDATA_SZ );

     // Set the size of the local information
     mosix_fixed_info_size = MOSIX_INFOD_DATA_SIZE;
     mosix_infod_info_sz = NINFO_SZ + MOSIX_INFOD_DATA_SIZE;

     // Setting up all the necessary things to handle the 3 providers this module
     // support.
     switch(mosix_provider_type) {
     case MOSIX_LOCAL_PROVIDER:
	  mosix_info_xml[0]='\0';
	  mosix_base_status |=
	       (MSX_STATUS_NO_PROVIDER | MSX_STATUS_CONF | MSX_STATUS_UP);
	  periodic_admin_func = lp_periodic_admin;
	  lp_fast_update_info_func = lp_mosix_fast_update_info;
	  lp_slow_update_info_func = lp_linux_slow_update_info;
	  get_mosix_kernel_version(&(myInfo.extra.kernel_version));
	  get_machine_arch_size((unsigned char *) &(myInfo.extra.machine_arch));
	  printf("Got archSize  : %hhd\n", myInfo.extra.machine_arch);
	  // Adding info modules 
	  mosix_pim = pim_init(pim_getDefaultPIMs(), infod_config_file);
	  if(!mosix_pim)
	       return 0;
	  break;
     case LINUX_LOCAL_PROVIDER:
     {
	  cpu_info_t cpu_info;
	  mosix_base_status |=
	       (MSX_STATUS_NO_PROVIDER | MSX_STATUS_CONF | MSX_STATUS_UP);
		    
	  get_cpu_info(&cpu_info);
	  myInfo.data.speed = 10 * cpu_info.mhz_speed;

	  myInfo.data.ncpus = cpu_info.ncpus;
	  debug_g("Detected -----------> %d cpus speed %d\n", 
		  cpu_info.ncpus, myInfo.data.speed);

	  periodic_admin_func = lp_periodic_admin;
	  lp_fast_update_info_func = lp_linux_fast_update_info;
	  lp_slow_update_info_func = lp_linux_slow_update_info;

	  // Adding info modules
	  mosix_pim = pim_init(pim_getDefaultPIMs(), infod_config_file);
	  if(!mosix_pim)
	       return 0;
     }
     break;
     case EXTERNAL_PROVIDER:
	  mosix_base_status |= MSX_STATUS_CONF;
	  periodic_admin_func = ep_periodic_admin;
	  break;
     default:
	  debug_lr(KCOMM_DEBUG, "Provider type %d is not supported",
		   mosix_provider_type);
     }

     // Checking out if we are inside VM
     if(running_on_vm())
	  mosix_base_status |= MSX_STATUS_VM;
     debug_lg(KCOMM_DEBUG, "Basic status %d\n", mosix_base_status);
	
     get_kernel_release(glob_kernel_release_buff);
     // Setting the real status
     myInfo.data.status = mosix_base_status;
	
     // Update extra information
     //slow_update_extra_info( &myInfo.extra );
     //fast_update_extra_info( &myInfo.extra );

     return 1;
}

/****************************************************************************
 * Reset the data
 ***************************************************************************/
int
mosix_sf_reset()
{
     debug_ly( KCOMM_DEBUG,"Kcomm: Reset\n" );
     bzero( &myInfo, MOSIX_IDATA_SZ );
     myInfo.data.status = mosix_base_status;
     return 1;
}

/****************************************************************************
 * Used by the infod, to tell the kcomm its local pe
 ***************************************************************************/
void
mosix_sf_set_pe( unsigned long pe ) {
	
     debug_ly( KCOMM_DEBUG,"Kcomm: Setting pe from infod (%d)\n", pe );
     //mosix_my_pe = pe;
     //myInfo.data.pe = pe;
}

/*************************************************************************
 * Setting the map change handler
 *************************************************************************/
void mosix_sf_set_map_change_hndl(map_change_hndl_t hndl)
{
     map_change_hndl = hndl;
}

/****************************************************************************
 * Parsing a msg buffer.
 * 1. Check the validy of the message on the buffer.
 * 2. If there are missing bytes for a valid message, report the amount of
 *    missing bytes (if it is possible)
 * 3. If there is additional bytes, then report about the extra data
 *    and the length of the message if we have a valid one.
 *
 * buff    : message buffer
 * bufflen : on input the length of the buffer
 * extra   : the size of extra bytes.
 *
 * Ret val: How much bytes are missing for a valid message
 *          -1 Error, 0 we have a valid message, >0 bytes are missing
 ****************************************************************************/
int
mosix_sf_parse_msg( void *buff, int bufflen, int *extra ) {

/*	int left = -1;
	unsigned long magic;
	int msg_size = 0;
	*extra = 0;
	
    	if( bufflen <= 0 )
	return -1;

	// The first part in any message is the magic. If we don't have
	// the magic, report the number of bytes still needed.
	if( bufflen < sizeof(unsigned long) )
	return (sizeof(unsigned long) - bufflen);

	// Using the magic, determine the erquired number of bytes.
	magic = *((unsigned long*)buff);

	switch( magic ) {
	case LOAD_MAGIC:
	msg_size = mosix_const_size; 	    
	if( bufflen >= msg_size ) {
	struct mosix_infomsg *msg;

	// Add the size of the topology
	msg = (struct mosix_infomsg*)buff;
	msg_size = msg_size +
	(msg->data.ntopology * ALL_TUNE_CONSTS *
	sizeof(int));
	}
	break;
		    
	case REQUEST_MAGIC:
	msg_size = 2 * sizeof(unsigned long);
	if( bufflen >= msg_size ) {
	unsigned long nnodes = ((unsigned long*)buff)[1];
	unsigned long nodes_len = nnodes *
	sizeof(unsigned long);
	msg_size = msg_size + nodes_len;
	}
	break;
		    
	case FREQUENCY_MAGIC:
	msg_size = sizeof(frequency_request_t);
	break;
		    
	case SPEEDCNG_MAGIC:
	msg_size = 2 * sizeof(unsigned long);
	break;
		    
	case  WHOTAKES_MAGIC:
	msg_size = sizeof(whotakes_request_t);
	break;
		    
	case CONSTCNG_MAGIC:
	msg_size = sizeof(struct costcng_msg);
	if( bufflen >= msg_size ) {
	struct costcng_msg *cost_msg =
	(struct costcng_msg*)buff;
	msg_size += ( cost_msg->ntopology *
	ALL_TUNE_CONSTS *
	sizeof(int));
	}
	break;
		    
	default:
	debug_lr( KCOMM_DEBUG, "KCOMM: unsupported magic %x\n",
	magic );
	return  -1;
	}

	// Calcilate the number of bytes required for a valid message. 
	left = (bufflen < msg_size)?(msg_size - bufflen):0;

	// Calculate the number of extra bytes, if the are additional ones.
	// One thing to remember is that the msg_size is without the magic
	// so we must take the size of the magic when calculating the extra
	if( left == 0 ) {
	if( bufflen > msg_size )
	*extra = bufflen - msg_size - sizeof(int);
	}

	return left;
*/
     return 0;
}

/*****************************************************************************
 * Printing a valid message
 ****************************************************************************/

static void
print_load_magic( void *msg ) {

     /*	debug_lb( KCOMM_DEBUG,
	struct mosix_infomsg *info =(struct mosix_infomsg*)msg;
	

	"Load message:\nPe %ld\nStatus %d\n", info->data.pe,
	info->data.status);
	debug_lb( KCOMM_DEBUG, "Speed %ld\nNtopology %d\nNcpus %d\nLoad %ld"
	"\nExport load %ld\nFrozen %h\nUtil %d\nFreepages %ld\n",
	info->data.speed, info->data.ntopology, info->data.ncpus,
	info->data.load, info->data.export_load, info->data.frozen,
	info->data.util,
	info->data.freepages );

     */
}

static void
print_request_magic( void *msg ) {
     unsigned long *ptr = (unsigned long*)msg;
     int node_num = ptr[1];
     int i;
	
     for( i = 0 ; i < node_num; i++ )
	  debug_lg( KCOMM_DEBUG, "Node %ld\n", ptr[2+i]);
}

static void
print_frequency_request( void *msg ) {
     struct frequency_request *fr = (struct frequency_request *)msg; 
     debug_lg( KCOMM_DEBUG, "Mili %d\nNinfo %d\nMax_age %d\n",
	       fr->milli, fr->ninfo, fr->max_age);
}

static void
print_costs_magic( void *msg ) {
     int i = 0, j = 0;
     struct costcng_msg *cost_msg = (struct costcng_msg *)msg;
	
     debug_lg( KCOMM_DEBUG, "Topology %ld\n", cost_msg->ntopology );
     for( i = 0 ; i < cost_msg->ntopology ; i++ ) {
	  for( j = 0 ; j < ALL_TUNE_CONSTS ; j++ )
	       debug_lg( KCOMM_DEBUG, " %ld ",
			 cost_msg->costs[i* ALL_TUNE_CONSTS +j]);
     }
     debug_lg( KCOMM_DEBUG, "\n" );
}

void
mosix_sf_print_msg( void *msg, int len ) {
     unsigned long magic = ((unsigned long*)msg)[0];
	
     switch( magic ) {
     case LOAD_MAGIC:
	  print_load_magic( msg );
	  break;
		    
     case REQUEST_MAGIC:
	  print_request_magic( msg );
	  break;

     case FREQUENCY_MAGIC:
	  print_frequency_request( msg ); 
	  break;

     case SPEEDCNG_MAGIC:
	  debug_lb( KCOMM_DEBUG, "New speed %ld\n",
		    ((unsigned long*)msg)[1]);
	  break;
		    
     case CONSTCNG_MAGIC:
	  print_costs_magic( msg );
	  break;
		    
     default:
	  debug_lr( KCOMM_DEBUG, "Unsupported magic\n");
     }
}

/****************************************************************************
 * Send node information to the kernel
 ***************************************************************************/
static int
mosix_send_info( ivec_entry_t* entry, node_t pe, unsigned long magic ) {
	
/*	struct mosix_infomsg msg;	
	struct mosix_infod_data *cur_infod_data = NULL;
	struct mosix_data *cur = NULL;
	int size = 0, send_len = 0;
	
	if( !entry ) {
	msg.data.status    = 0;
	msg.data.ntopology = 0;
	send_len = mosix_const_size;
	}

	else {
	cur_infod_data = (struct mosix_infod_data*)
	( entry->info->data );
		
	cur =  &(cur_infod_data->data);
	size = mosix_const_size + cur->ntopology *
	ALL_TUNE_CONSTS * sizeof(int);

	if( size >= MOSIX_DATA_SZ ) {
	debug_lr( KCOMM_DEBUG, "KCOMM: send size too big\n" );
	return 0;		
	}
	memcpy( &(msg.data), cur, size );
	}
	
	msg.magic   = magic;
	msg.data.pe = pe;
	send_len    = size + sizeof(msg.magic);
	
	if( !entry || !(entry->info->hdr.status & INFOD_ALIVE ) ||
	mosix_pe_diff_mode )
	msg.data.status = mosix_base_status;

	if( !kcomm_send((void*)&msg, send_len )) {
	debug_lr( KCOMM_DEBUG, "KCOMM: kcomm_send failed "
	"sending periodic\n");
	return 0;
	}
*/
     return 1;
}

/****************************************************************************
 * We assume that the periodic admin function is called from time to time
 * by infod. There are 2 time interval we do work in:
 * If in noprovider mode then:
 *    MOSIX_UPDATE_INFO_FAST  for updating regular information (load ...)
 *    MOSIX_UPDATE_INFO_SLOW  for updating less important information ()
 *
 * If in provider mode then since regular information is retrieved from
 * the provider the 2 time intervals are used to update the extra informaiton 
 *    MOSIX_UPDATE_INFO_FAST  for updating important extra
 *    MOSIX_UPDATE_INFO_SLOW  for updating less important extra information
 *
 *    Selected information is sent to the provider 
 ****************************************************************************/
static int lp_periodic_admin( ivec_t vec )
{
     static struct timeval last_called = { 0, 0 };
     static struct timeval last_slow_called = { 0, 0};
     struct timeval curr_time;
     //unsigned long up2age_time;
     unsigned long time_diff;

     gettimeofday( &curr_time, NULL );
	
     //debug_ly( KCOMM_DEBUG, "-----> Periodic Admin no provider\n");
     time_diff = milli_time_diff(&last_slow_called, &curr_time);
     if( time_diff >= MOSIX_UPDATE_INFO_SLOW) {
	  lp_slow_update_info_func(&myInfo, &curr_time);
	  last_slow_called = curr_time;
     }
        
     time_diff = milli_time_diff(&last_called, &curr_time);
     if( time_diff >= MOSIX_UPDATE_INFO_FAST) {
	  lp_fast_update_info_func(&myInfo, &curr_time);
	  last_called = curr_time;
     }
     return 1;
}

static int
ep_periodic_admin( ivec_t vec )
{
     static struct timeval last_called = { 0, 0 };
     static struct timeval last_extra_called = { 0, 0};
     struct timeval curr_time;
     //unsigned long up2age_time;
     unsigned long time_diff;
	
     int i = 0, how_many = 0, selected_size = 0;
     ivec_entry_t **vec_entries = NULL;

     // No frequency request arrived yet
     if( (mosix_periodic_data.milli <= 0) ||
//	    (mosix_my_pe == 0)               ||
//	    (mosix_pe_diff_mode)             || 
	 !vec )
	  return 1;

     gettimeofday( &curr_time, NULL );

     // Taking care of extra info updates
     time_diff = milli_time_diff(&last_extra_called, &curr_time);
     if( time_diff >= MOSIX_UPDATE_INFO_SLOW) {
	  ep_slow_update_extra_info(&myInfo.extra);
	  last_extra_called = curr_time;
     }
	
     // calculating the difference in milli
     time_diff = milli_time_diff(&last_called, &curr_time);
     if( time_diff < mosix_periodic_data.milli )
	  return 1;

     //debug_ly( KCOMM_DEBUG, "-----> Periodic Admin\nnow %ld : %ld\nlast %ld : %ld\n",
     //	  curr_time.tv_sec, curr_time.tv_usec/1000,
     //	  last_called.tv_sec, last_called.tv_usec/1000);
     //debug_ly( KCOMM_DEBUG, "-----> time diff %d\n",
     //	  time_diff);

     // Ok enough time has passed and we are ready to update fast info and send
     // information to mosixd
     ep_fast_update_extra_info(&myInfo.extra);

     //up2age_time = mosix_periodic_data.milli*1000;
     //vec_entries = ivec_ret_up2age( vec, up2age_time, &how_many );
     selected_size = SELECTED_MAX_SIZE;
	
     if( ( select_info_to_send( vec_entries, how_many,
				mosix_selected_nodes,
				&selected_size )) == 0 ){
	  free( vec_entries );
	  return 1;
     }
     for( i = 0 ; i < selected_size ; i++ ) {
	  int pos = mosix_selected_nodes[ i ];
	  if( vec_entries[ pos ]->isdead != 0 )
	       continue;

	  debug_lb( KCOMM_DEBUG, "KCOMM: Sending periodic \n" );
	  if( ( mosix_send_info( vec_entries[pos],
				 vec_entries[pos]->info->hdr.pe,
				 LOAD_MAGIC )) == 0 )
	       break;
     }
     free( vec_entries );
     last_called = curr_time;
     return 1;
}

int mosix_sf_periodic_admin( ivec_t vec ) {
     return (*periodic_admin_func)(vec);
}


/****************************************************************************
 * Compoutes the statistics of the infod
 ****************************************************************************/
int
mosix_sf_get_stats( ivec_t vec, infod_stats_t *stats ) {

     unsigned int i = 0, num = 0;
     ivec_entry_t** all = NULL;
	
     if( !vec || !stats ) {
	  debug_lr( VEC_DEBUG, "Error: args, stats\n" );
	  return 0;
     }

     // The fields the provider is responsible for
     stats->ncpus   = 0;
     stats->tmem    = 0;
     stats->avgload = 0;
     stats->sspeed  = 0;
	
     if( !( all = infoVecGetAllEntries( vec )))
	  return -1;

     for( i = 0; i < stats->total_num ; i++ )
	  if( all[ i ]->isdead == 0 ) {
	       node_info_t    *info = all[i]->info;
	       struct mosix_infod_data *cur =
		    (struct mosix_infod_data*)(info->data);
			
	       stats->tmem     += cur->extra.tmem;
	       stats->ncpus    += cur->data.ncpus;
	       stats->avgload  += (double)(cur->data.load);
	       if( cur->data.speed > stats->sspeed )
		    stats->sspeed = cur->data.speed;
	       num++;
	  }
	
     stats->avgload = stats->avgload/(double)(num);
     free(all);
     return 1;
}

/****************************************************************************
 *
 * Functions used to handle messages.
 *
 ***************************************************************************/
static int
handle_load_magic( char *req_buff ) {
	
/*	struct mosix_infomsg *msg = NULL;
	msg = (struct mosix_infomsg*)req_buff;
	if( msg->data.ntopology != mosix_ntopology ) {
	debug_lr( KCOMM_DEBUG, "KCOMM: mosix configured"
	"with different "
	"topology than kernel (%d) (%d)",
	mosix_ntopology, msg->data.ntopology);
	return 0;
	}

	// We keep local information
	memcpy( &(myInfo.data), &(msg->data), mosix_info_size );

	// Getting our pe. This will also enable priodic admin.
	if( myInfo.data.pe != mosix_my_pe ) {
	debug_lr( KCOMM_DEBUG, "ERROR pe of kernel is "
	"different (%d) (%d)\n",
	myInfo.data.pe ,mosix_my_pe );
	mosix_pe_diff_mode = 1;
	}
	else
	mosix_pe_diff_mode = 0;

*/
     return 1;
}

static int
handle_requet_magic( char* req_buff, ivec_t vec ) {

     unsigned long *data = (unsigned long*)req_buff;
     int nnodes = data[1];
     int i = 0;
     ivec_entry_t **vec_entries;

     debug_lg( KCOMM_DEBUG, "KCOMM: Got node request %d\n", nnodes );

     // data pointing to the pe list
     data += 2;

     // Asking the vector for the pes
     vec_entries = NULL; //ivec_ret_ni( vec, (node_t*)data, nnodes );
	
     if( !vec_entries ) {
	  debug_lr( KCOMM_DEBUG, "KCOMM: get information from vec\n");
	  return 0;
     }
	
     // Preapring every message
     for( i = 0 ; i < nnodes ; i++ ) {
	  debug_lb( KCOMM_DEBUG, "KCOMM: Sending periodic \n" );
	  if( ( mosix_send_info( vec_entries[i], data[ i ],
				 REQUEST_MAGIC )) == 0 )
	       break;
     }
	
     free( vec_entries );
     return 1;
}

static int
handle_frequency_magic( char *req_buff ) {
     frequency_request_t *fr = (frequency_request_t*)req_buff;
	
     if( (fr->milli < 0) || (fr->ninfo <= 0) || (fr->max_age <= 0)) {
	  bzero( &mosix_periodic_data, sizeof(frequency_request_t));
	  debug_lr( KCOMM_DEBUG, "KCOMM: invalid frequency message\n");
	  return 0;
     }
     else 
	  mosix_periodic_data = *fr;
     return 1;
}

/****************************************************************************
 ** Get a map change message from mosixd. and signal infod that the map
 ** has changed.
 **
 ****************************************************************************/
static int
handle_map_magic()
{
     if(map_change_hndl)
	  (*map_change_hndl)();
     return 1;
}

/****************************************************************************
 * Handling specific messages.
 * Sould get the information vector.
 * Sould we get access to other data structures???
 ***************************************************************************/
int
mosix_sf_handle_msg( unsigned long magic, char *req_buff, int req_len,
		     ivec_t vec) {

     switch( magic ) {
     case LOAD_MAGIC:
	  return handle_load_magic( req_buff );
		    
     case REQUEST_MAGIC:
	  return handle_requet_magic( req_buff, vec ) ; 
			    
     case FREQUENCY_MAGIC:
	  return handle_frequency_magic( req_buff );
		    
//	    case  WHOTAKES_MAGIC:
//		 return handle_whotakes_magic( req_buff, vec );
     case MAP_MAGIC:
	  return handle_map_magic();
		    
     default:
	  debug_lr( KCOMM_DEBUG, "KCOMM: unsupported magic\n");
     }
	
     return 0;
}

/****************************************************************************
 * Return the information description in the form of xml string
 *
 * The function allocate the memory for the description.
 * Return value : NULL if no memory or kcomm not active yet
 *                xml description if kcomm active and we got at least
 *                one load message.
 ***************************************************************************/

#define DESC_SZ      (16196)

char*
mosix_sf_get_infodesc() {

     static char desc[DESC_SZ];
	
     bzero( desc, DESC_SZ );
     strcpy( desc, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n" );
     strcat( desc, "<local_info>\n" );

     // The Extra info (not from mosixd) description
     strcat( desc, "\t<base name=\"tmem\"  type=\"unsigned long\" "
	     " unit=\"4KB\"/>\n" );
     strcat( desc, "\t<base name=\"tswap\" type=\"unsigned long\" "
	     " unit=\"4KB\"/>\n" );
     strcat( desc, "\t<base name=\"fswap\" type=\"unsigned long\" "
	     " unit=\"4KB\"/>\n" );
     strcat( desc, "\t<base name=\"uptime\" type=\"unsigned long\" "
	     "unit=\"Sec\" />\n" );

     strcat( desc, "\t<base name=\"disk_read_rate\" type=\"unsigned int\" "
	     "unit=\"KByte/Sec\" />\n" );
     strcat( desc, "\t<base name=\"disk_write_rate\" type=\"unsigned int\" "
	     "unit=\"KByte/Sec\" />\n" );
     strcat( desc, "\t<base name=\"net_rx_rate\" type=\"unsigned int\" "
	     "unit=\"KByte/Sec\" />\n" );
     strcat( desc, "\t<base name=\"net_tx_rate\" type=\"unsigned int\" "
	     "unit=\"KByte/Sec\" />\n" );
	
     strcat( desc, "\t<base name=\"tdisk\" type=\"unsigned long\" " 
	     "unit=\"4KB\" />\n" );
     strcat( desc, "\t<base name=\"fdisk\" type=\"unsigned long\" " 
	     "unit=\"4KB\" />\n" );
		
     strcat( desc, "\t<base name=\"locals\"    type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"guests\"    type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"maxguests\" type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"mosix_version\" type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"" ITEM_TOTAL_FREEZE_NAME "\" type=\"unsigned int\" />\n" );
     strcat( desc, "\t<base name=\"" ITEM_FREE_FREEZE_NAME  "\" type=\"unsigned int\" />\n" );

     strcat( desc, "\t<base name=\"arch_size\" type=\"unsigned char\" />\n" );
     strcat( desc, "\t<base name=\"" ITEM_IOWAIT_NAME "\" type=\"unsigned char\" />\n" );
     strcat( desc, "\t<base name=\"extra_unused_2\" type=\"unsigned short\" />\n" );

     strcat( desc, "\t<base name=\"extra_unused_3\" type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"extra_unused_4\" type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"extra_unused_5\" type=\"int\" />\n" );
	
     strcat( desc, "\t<base name=\"nfs_client_rpc_rate\" type=\"unsigned long\" " 
	     "unit=\"Ops/Sec\" />\n" );
	
     // The regular info (from mosixd) description
     strcat( desc, "\t<base name=\"freepages\" type=\"unsigned long\"/>\n" );
     strcat( desc, "\t<base name=\"totalpages\" type=\"unsigned long\"/>\n" );
     strcat( desc, "\t<base name=\"load\"  type=\"unsigned int\"/>\n" );
     strcat( desc, "\t<base name=\"speed\" type=\"unsigned int\"/>\n" );
        
     strcat( desc, "\t<base name=\"not_used_1\" type=\"int\"/>\n" );

     strcat( desc, "\t<base name=\"status\" type=\"unsigned short\"/>\n" );
     strcat( desc, "\t<base name=\"level\" type=\"unsigned short\"/>\n" );
     strcat( desc, "\t<base name=\"frozen\" type=\"unsigned short\"/>\n" );
     strcat( desc, "\t<base name=\"util\"   type=\"unsigned char\"/>\n" );
     strcat( desc, "\t<base name=\"ncpus\" type=\"unsigned char\"/>\n" );
     strcat( desc, "\t<base name=\"nproc\" type=\"unsigned short\"/>\n" );
     strcat( desc, "\t<base name=\"token_level\" type=\"unsigned short\"/>\n" );
     strcat( desc, "\t<base name=\"edgepages\" type=\"unsigned long\"/>\n" );
     strcat( desc, "\t<base name=\"not_used_2\" type=\"unsigned long\"/>\n" );
     strcat( desc, "\t<base name=\"baches\" type=\"unsigned short\" />\n" );
     strcat( desc, "\t<base name=\"pad0\" type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"pad1\" type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"pad2\" type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"pad3\" type=\"int\" />\n" );
     strcat( desc, "\t<base name=\"pad4\" type=\"int\" />\n" );
	
     // Adding the possible vlen info items
     pim_appendDescription(mosix_pim, desc);

     strcat( desc, "\t<vlen name=\"" ITEM_KERNEL_RELEASE_NAME "\" type=\"string\" unit=\"xml\" />\n" );
     strcat( desc, "\t<vlen name=\"" ITEM_PROVIDER_TYPE_NAME "\" type=\"string\" unit=\"xml\" />\n" );
     strcat( desc, "\t<vlen name=\"" ITEM_MOSIX_INFO_NAME "\" type=\"string\" unit=\"xml\" />\n" );
     strcat( desc, "\t<vlen name=\"" ITEM_EXTERNAL_NAME     "\" type=\"string\"/>\n" );
        
     strcat( desc, "</local_info>\n" );
     return strdup( desc );
}

/****************************************************************************
 * Return the size of the local iformation unit
 ***************************************************************************/
int
mosix_sf_get_infosize() {
     return mosix_infod_info_sz;
}

/****************************************************************************
 * return the new priority of the information for the infod usage in
 * constructing the window
 ***************************************************************************/
static int
mosix_get_priority() {
	
     return 0;
}

void addProviderTypeInfo(node_info_t *info) {
     char b[256];
     sprintf(b, "<%1$s>%2$s</%1$s>", ITEM_PROVIDER_TYPE_NAME,
	     providerNameList[mosix_provider_type]);
     add_vlen_info(info, ITEM_PROVIDER_TYPE_NAME, b, strlen(b)+1);
}

// Adding the kernel version into to info
void addKernelReleaseInfo(node_info_t *info) {
     char b[256];
     sprintf(b, "<%1$s>%2$s</%1$s>", ITEM_KERNEL_RELEASE_NAME, glob_kernel_release_buff);
     add_vlen_info(info, ITEM_KERNEL_RELEASE_NAME, b, strlen(b)+1);
}

void addMosixXMLInfo(node_info_t *info) {
     char b[256];

     if (mosix_info_xml[0]=='\0'){
	  sprintf(b, "<%1$s>%2$s</%1$s>", ITEM_MOSIX_INFO_NAME, "empty");
	  add_vlen_info(info, ITEM_MOSIX_INFO_NAME, b, strlen(b)+1);
     }
     else {
	  add_vlen_info(info, ITEM_MOSIX_INFO_NAME, mosix_info_xml, strlen(mosix_info_xml)+1);
     }
     
}


/****************************************************************************
 * Return the local information
 ***************************************************************************/
int mosix_sf_get_info( void *buff, int size ) {

     node_info_t *info = (node_info_t*)buff;

     if( size < mosix_infod_info_sz ) {
	  debug_lr(KCOMM_DEBUG, "Error: Got size smaller than info (%d %d)\n",
		   size, mosix_infod_info_sz);
	  return -1;
     }
     // The provider is not connected or not working ok
     if(!mosix_provider_status) { 
	  debug_lr(KCOMM_DEBUG, "Error: provider status = 0\n");
	  return -1;
     }
     bzero( info, mosix_infod_info_sz );
     
     //info->hdr.pe     = myInfo.data.pe;
     
     
     // Inf this version we dont have a partial size
     info->hdr.psize  = mosix_infod_info_sz;
     info->hdr.fsize  = mosix_infod_info_sz;
     
     info->hdr.status = INFOD_ALIVE;
     gettimeofday( &(info->hdr.time), NULL );
     
     // Adding flags such as the RUNNING on VM to the status
     myInfo.data.status |= mosix_base_status;
     debug_lb(KCOMM_DEBUG, "Status %d %d\n",
	      info->hdr.status, myInfo.data.status);
     // FIXME modifing the secret_load to be export load
     //myInfo.data.secret_load = myInfo.data.export_load;
     memcpy( info->data, &(myInfo), mosix_fixed_info_size );
     

     pim_packInfo(mosix_pim, info);

     addKernelReleaseInfo(info);
     addProviderTypeInfo(info);
     addMosixXMLInfo(info);
     
     
     
     // Handling external information (and external status)     
     debug_lg(KCOMM_DEBUG, "------>External Status %d size %d\n",
	      mosix_external_status, mosix_external_info_size);
     if(mosix_external_status == EXTERNAL_STAT_INFO_OK)
     {
	  info->hdr.external_status =
	       info->hdr.time.tv_sec - mosix_external_mtime;
	  add_vlen_info(info, ITEM_EXTERNAL_NAME, mosix_external_buff,
			mosix_external_info_size);
     }
     else
	  info->hdr.external_status = mosix_external_status;
        
     debug_lb(KCOMM_DEBUG, "Get info fsize %d psize %d bufferSize = %d LOAD %d\n ",
	      info->hdr.fsize, info->hdr.psize, size, myInfo.data.load);
     
     return mosix_get_priority();
}

/****************************************************************************
 * The default structure for mosix 
 ***************************************************************************/

kcomm_module_func_t mosix_provider_funcs = {
init           : mosix_sf_init,
reset          : mosix_sf_reset,
print_msg      : mosix_sf_print_msg,
parse_msg      : mosix_sf_parse_msg,
handle_msg     : mosix_sf_handle_msg,
periodic_admin : mosix_sf_periodic_admin,
get_stats      : mosix_sf_get_stats,
get_infodesc   : mosix_sf_get_infodesc,
get_infosize   : mosix_sf_get_infosize,
get_info       : mosix_sf_get_info,
set_pe         : mosix_sf_set_pe
};

/****************************************************************************
 *                              E O F 
 ***************************************************************************/
