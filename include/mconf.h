/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/



/** @defgroup mconf The mosix status/configuration module
    Acessing the configurations information of umosix.
    All the non kernel configuration statuses....

    For example:  manual_gblock = 1;
                  guest_definition = gird_is_guest;

	   And maybe the whole map file should come here.
	   
    

*/

#ifndef __UMOSIX_MCONF__
#define __UMOSIX_MCONF__

#include <msx_common.h>
#include <mapper.h>

typedef struct mconf_struct  *mconf_t;

int start_mconf(char *fname);
int stop_mconf(char *fname);
	
mconf_t mconf_init(char *fname);
void mconf_done(mconf_t mconf);
int mconf_get_off_mode(mconf_t mconf, int *off_mode);

/* Printing functions */
void mconf_file_sprintf(char *buff, char *fname);
void mconf_file_fprintf(FILE *stream, char *fname);
void mconf_file_printf(char *fname);
void mconf_sprintf(char *buff, mconf_t mconf);
void mconf_fprintf(FILE *stream, mconf_t mconf);
void mconf_printf(mconf_t mconf);

// Only for debugging
//int mconf_set_guest_type(mconf_t mconf, guest_t guest_type);
//int mconf_get_guest_type(mconf_t mconf, guest_t *guest_type);

// Setting/Getting the maxguest value, this value is the one when the node open
// itself for guests.
int mconf_set_maxguests(mconf_t mconf, int maxguests);
int mconf_get_maxguests(mconf_t mconf, int *maxguests);

// Storing/Getting the actuall used map
int mconf_get_map_size(mconf_t mconf);
int mconf_set_map(mconf_t mconf, char *map, int size);
int mconf_get_map(mconf_t mconf, char *map, int size);

mapper_t mconf_get_mapper(mconf_t mconf);
// Manuall presedence mode manipulations. This mode is when we want to control
// the way this node is opened for grid/cluster nodes manually without
// the ownerd doing anything
int mconf_set_manual_presedence_mode(mconf_t mconf);
int mconf_clear_manual_presedence_mode(mconf_t mconf);
int mconf_is_manual_presedence_mode(mconf_t mconf, int *is_mpm);

int mconf_set_reserve_dist(mconf_t mconf, int reserve_dist);
int mconf_get_reserve_dist(mconf_t mconf, int *reserve_dist);
int mconf_calc_reserve_dist(int min_guest_dist, int max_guest_dist,
			    int maxguests);

int mconf_set_min_run_dist(mconf_t mconf, int min_run_dist);
int mconf_get_min_run_dist(mconf_t mconf, int *min_run_dist);

int mconf_set_mos_procs(mconf_t mconf, int mos_procs);
int mconf_get_mos_procs(mconf_t mconf, int *mos_procs);

int mconf_pblock(mconf_t mconf);
int mconf_gblock(mconf_t mconf);
int mconf_gopen(mconf_t mconf);

// Letting the ownerd report about its status so we can present it in mmon
int mconf_set_ownerd_status(mconf_t, int status);
int mconf_get_ownerd_status(mconf_t, int *status);

// Letting/unletting local cluster/partition processes use grid nodes.
// This way local processes can be prevented from going outisde and also
// are brought back into the local cluster/partition
int mconf_grid_use(mconf_t mconf);
int mconf_grid_unuse(mconf_t mconf);

// Connecting/Dissconnecting from the grid, The grid is defined to be the layer
// above the local cluster. So we can decide if we want to connect/disscon
// In a dissconn the kernel configuration is changes so all the nodes which
// our distance to them is > then the distance to another cluster nodes
// are removed from the map. In connect we add them back to the map.
int mconf_grid_connect(mconf_t mconf);
int mconf_grid_dissconnect(mconf_t mconf);

// Getting the minimal distance to a node that is defined as guest
// This value is manipulated by the above pblock, glbock, gopen
// routines.
int mconf_get_min_guest_dist(mconf_t mconf, int *dist);

// As above getting the maximal distance to a known node. A node with distance
// larger then this is not included in the configuration. So there will be no
// process migration to that node, or information desimination This value is
// manipulated by the grid_connect, grid_disconnect routines
int mconf_get_max_guest_dist(mconf_t mconf, int *dist);

// Getting the maximal distance a local process can migrate to
int mconf_get_max_mig_dist(mconf_t mconf, int *dist);
#endif /* __UMOSIX_MCONF__ */
