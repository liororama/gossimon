/* 
 * File:   mmon_info_map.h
 * Author: lior
 *
 * Created on April 18, 2012, 2:31 PM
 */

#ifndef MMON_INFO_MAP_H
#define	MMON_INFO_MAP_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <info.h>
#include <infolib.h>
#include <info_reader.h>
#include <info_iter.h>

typedef struct mmon_info_mapping
{
	var_t  *tmem;
	var_t  *tswap;
	var_t  *fswap;
	var_t  *tdisk;
	var_t  *fdisk;
	var_t  *uptime;

	var_t  *disk_read_rate;
	var_t  *disk_write_rate;
	var_t  *net_rx_rate;
	var_t  *net_tx_rate;
	var_t  *nfs_client_rpc_rate;
	
	var_t  *speed;
	var_t  *iospeed;
	var_t  *ntopology;
	var_t  *ncpus;
	var_t  *load;
	var_t  *export_load;
	var_t  *frozen;
	var_t  *priority;
	var_t  *util;
        var_t  *iowait;
        var_t  *status;
	var_t  *freepages;
      
	var_t  *guests;
	var_t  *locals;
	var_t  *maxguests;
	var_t  *mosix_version;
	var_t  *min_guest_dist;
	var_t  *max_guest_dist;
	var_t  *max_mig_dist;

	var_t  *token_level;
	
	var_t *min_run_dist;
	var_t *mos_procs;
	var_t *ownerd_status;

        var_t *usedby;
        var_t *freeze_info;
        var_t *proc_watch;
	var_t *cluster_id;
        var_t *infod_debug;
        var_t *eco_info;
        var_t *jmig;
} mmon_info_mapping_t;



#ifdef	__cplusplus
}
#endif

#endif	/* MMON_INFO_MAP_H */

