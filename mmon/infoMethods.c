/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2011 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

/******************************************************************************
 *
 * Author(s): Alexander Margolin, Amar Lior
 *
 * File was extracted from mmon.c - and reorganaized by Lior Amar
 ******************************************************************************/


#include "mmon.h"


void set_settings(mon_disp_prop_t* display, settings_t* setup)
//This function fills the relevant settings in the setup struct
//(For more information see setup struct documentation)
{
    setup->yardstick = &(display->sspeed);
    setup->clsinit = true;
}

int get_infod_description(mon_disp_prop_t* display, int forceReload)
//Getting infod description and parsing it creating the mapping structure
{
    forceReload = forceReload || display->recount;
    if (!forceReload && glob_info_desc && glob_info_var_mapping)
        return 1;

    if (dbg_flg) fprintf(dbg_fp, "getting infod description...(%s : %i) \n",
            display->mosix_host, glob_host_port);

    if (forceReload) {
        if (glob_info_desc) {
            free(glob_info_desc);
            glob_info_desc = NULL;
        }
        if (glob_info_var_mapping) {
            destroy_info_mapping(glob_info_var_mapping);
            glob_info_var_mapping = NULL;
        }
    }


    if (!glob_info_desc) {
        glob_info_desc = infolib_info_description(display->mosix_host, glob_host_port);
        if (!glob_info_desc) {
            if (dbg_flg) fprintf(dbg_fp, "Failed getting description\n");
            return 1;
        }

        if (!(glob_info_var_mapping = create_info_mapping(glob_info_desc))) {
            if (dbg_flg) fprintf(dbg_fp, "Failed creating info mapping\n");
            free(glob_info_desc);
            glob_info_desc = NULL;
            glob_info_var_mapping = NULL;
            return 0;
        }
        if (dbg_flg) fprintf(dbg_fp, "description \n%s\n", glob_info_desc);
    }

    if (dbg_flg) fprintf(dbg_fp, "got description.\n");


    // Regular MOSIX node information
    glob_info_map.tmem = get_var_desc(glob_info_var_mapping, ITEM_TMEM_NAME);
    glob_info_map.tswap = get_var_desc(glob_info_var_mapping, ITEM_TSWAP_NAME);
    glob_info_map.fswap = get_var_desc(glob_info_var_mapping, ITEM_FSWAP_NAME);
    glob_info_map.tdisk = get_var_desc(glob_info_var_mapping, ITEM_TDISK_NAME);
    glob_info_map.fdisk = get_var_desc(glob_info_var_mapping, ITEM_FDISK_NAME);
    glob_info_map.uptime = get_var_desc(glob_info_var_mapping, ITEM_UPTIME_NAME);
    glob_info_map.disk_read_rate =
            get_var_desc(glob_info_var_mapping, ITEM_DISK_READ_RATE);
    glob_info_map.disk_write_rate =
            get_var_desc(glob_info_var_mapping, ITEM_DISK_WRITE_RATE);
    glob_info_map.net_rx_rate =
            get_var_desc(glob_info_var_mapping, ITEM_NET_RX_RATE);
    glob_info_map.net_tx_rate =
            get_var_desc(glob_info_var_mapping, ITEM_NET_TX_RATE);
    glob_info_map.nfs_client_rpc_rate =
            get_var_desc(glob_info_var_mapping, ITEM_NFS_CLIENT_RPC_RATE);


    glob_info_map.speed = get_var_desc(glob_info_var_mapping, ITEM_SPEED_NAME);
    glob_info_map.iospeed = get_var_desc(glob_info_var_mapping, ITEM_IOSPEED_NAME);
    glob_info_map.ntopology = get_var_desc(glob_info_var_mapping, ITEM_NTOPOLOGY_NAME);
    glob_info_map.ncpus = get_var_desc(glob_info_var_mapping, ITEM_NCPUS_NAME);
    glob_info_map.frozen = get_var_desc(glob_info_var_mapping, ITEM_FROZEN_NAME);
    glob_info_map.priority = get_var_desc(glob_info_var_mapping, ITEM_PRIO_NAME);
    glob_info_map.load = get_var_desc(glob_info_var_mapping, ITEM_LOAD_NAME);
    glob_info_map.export_load = get_var_desc(glob_info_var_mapping, ITEM_ELOAD_NAME);
    glob_info_map.util = get_var_desc(glob_info_var_mapping, ITEM_UTIL_NAME);
    glob_info_map.iowait = get_var_desc(glob_info_var_mapping, ITEM_IOWAIT_NAME);

    glob_info_map.status = get_var_desc(glob_info_var_mapping, ITEM_STATUS_NAME);
    glob_info_map.freepages = get_var_desc(glob_info_var_mapping, ITEM_FREEPAGES_NAME);
    glob_info_map.token_level = get_var_desc(glob_info_var_mapping, ITEM_TOKEN_LEVEL_NAME);

    // Grid + Reserve information
    glob_info_map.locals = get_var_desc(glob_info_var_mapping, ITEM_LOCALS_PROC_NAME);
    glob_info_map.guests = get_var_desc(glob_info_var_mapping, ITEM_GUESTS_PROC_NAME);
    glob_info_map.maxguests = get_var_desc(glob_info_var_mapping, ITEM_MAXGUESTS_NAME);
    glob_info_map.mosix_version =
            get_var_desc(glob_info_var_mapping, ITEM_MOSIX_VERSION_NAME);
    glob_info_map.min_guest_dist =
            get_var_desc(glob_info_var_mapping, ITEM_MIN_GUEST_DIST_NAME);
    glob_info_map.max_guest_dist =
            get_var_desc(glob_info_var_mapping, ITEM_MAX_GUEST_DIST_NAME);
    glob_info_map.max_mig_dist =
            get_var_desc(glob_info_var_mapping, ITEM_MAX_MIG_DIST_NAME);
    glob_info_map.min_run_dist =
            get_var_desc(glob_info_var_mapping, ITEM_MIN_RUN_DIST_NAME);
    glob_info_map.mos_procs =
            get_var_desc(glob_info_var_mapping, ITEM_MOS_PROCS_NAME);
    glob_info_map.ownerd_status =
            get_var_desc(glob_info_var_mapping, ITEM_OWNERD_STATUS_NAME);

    // vlen String
    glob_info_map.usedby = get_var_desc(glob_info_var_mapping, ITEM_USEDBY_NAME);
    glob_info_map.freeze_info = get_var_desc(glob_info_var_mapping, ITEM_FREEZE_INFO_NAME);
    glob_info_map.proc_watch = get_var_desc(glob_info_var_mapping, ITEM_PROC_WATCH_NAME);
    glob_info_map.cluster_id = get_var_desc(glob_info_var_mapping, ITEM_CID_CRC_NAME);
    glob_info_map.infod_debug = get_var_desc(glob_info_var_mapping, ITEM_INFOD_DEBUG_NAME);
    glob_info_map.eco_info = get_var_desc(glob_info_var_mapping, ITEM_ECONOMY_STAT_NAME);
    glob_info_map.jmig = get_var_desc(glob_info_var_mapping, ITEM_JMIGRATE_NAME);

    displayModule_updateInfoDescription(glob_info_var_mapping);

    return 1;
}

/******************************************************************************
 * A helper function that save information given in the data pointer into the
 * target location.
 *****************************************************************************/
int save_vlen_data(char *data, int size, char **target)
{
    int sizeToAllocate;

    if (!data || size < 0)
        return 0;

    // Allocate the external string if necessary
    if (!(*target)) {
        sizeToAllocate = size > 512 ? size : 512;
        *target = (char*) realloc(*target, sizeToAllocate + sizeof (char));
        if (!(*target))
            return 0;
    } else {
        if (size > 512) {
            //free(*target);
            *target = realloc(*target, size + sizeof (char));
            if (!(*target))
                return 0;
        }
    }

    data[size - 1] = '\0';
    memcpy(*target, data, size);
    (*target)[size] = '\0';
    return 1;
}

void getVlenItem(node_info_t *ninfo, var_t *infoVar, char **vlenItem)
{
    char *vlenData;
    int vlenSize;

    if (!infoVar)
        return;

    //if(dbg_flg) fprintf(dbg_fp, "Adding vlen %s\n", infoVar->name);
    vlenData = get_vlen_info(ninfo, infoVar->name, &vlenSize);
    if (!vlenData)
        *vlenItem = NULL;
    else
        if (!save_vlen_data(vlenData, vlenSize, vlenItem))
        msx_critical_error("Error allocating memory for vlen data\n");
}

void sig_alarm_hndl(int sig)
{
}

//In case we get sig alarm in the middle of the infolib library


int compare_num(const void* item1, const void* item2)
//compares the number of two objects (the pointers point to instances in raw_data)
{
    return (int) scalar_div_x(dm_getIdByName("num"), (void*) ((long) item1 + get_pos(dm_getIdByName("num"))), 0) -
            (int) scalar_div_x(dm_getIdByName("num"), (void*) ((long) item2 + get_pos(dm_getIdByName("num"))), 0);
}


int get_data_from_infod(mon_disp_prop_t* display, idata_t **infod_data_ptr)
{
    idata_t *infod_data = NULL;
    struct sigaction act;
    struct itimerval timeout;

    /* setting the signal handler of SIGALRM */
    act.sa_handler = sig_alarm_hndl;
    sigemptyset(&(act.sa_mask));
    sigaddset(&(act.sa_mask), SIGINT);
    sigaddset(&(act.sa_mask), SIGTERM);
    act.sa_flags = 0; //SA_RESTART ;

    if (sigaction(SIGALRM, &act, NULL) < -1) {
        if (dbg_flg) fprintf(dbg_fp, "Error: sigaction on SIGALRM\n");
        terminate();
    }

    timeout.it_value.tv_sec = mmon_connect_timeout;
    timeout.it_value.tv_usec = 0;
    timeout.it_interval.tv_sec = mmon_connect_timeout;
    timeout.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timeout, NULL);

    // First getting the description if we dont have it already
    if (!get_infod_description(display, 0)) {
        if (dbg_flg) fprintf(dbg_fp, "Error getting description\n");
        // Disable the sigalarm
        setitimer(ITIMER_VIRTUAL, NULL, NULL);
        return 0;
    }

    // Getting the information itself
    infod_data = infolib_all(display->mosix_host, glob_host_port);
    if (!infod_data) {
        if (dbg_flg) fprintf(dbg_fp, "Error getting info\n");
        // Disable the sigalarm
        setitimer(ITIMER_VIRTUAL, NULL, NULL);
        return 0;
    }

    // Disable the sigalarm
    setitimer(ITIMER_REAL, NULL, NULL); // questionable
    setitimer(ITIMER_VIRTUAL, NULL, NULL);

    *infod_data_ptr = infod_data;
    return 1;
}

int allocate_display_raw_data_mem(mmon_display_t *display, int size) {

    display->block_length = 0;

    // Iterating over the display modules and summing their memory length
    for (int index = 0; index < infoDisplayModuleNum; index++)
        display->block_length += infoModulesArr[index]->md_length_func();

    //allocating data array memory
    if (size > 0) {
        display->raw_data = calloc(size, display->block_length);
        display->life_arr = (int*) malloc(sizeof (int) * size);
    } else {
        display->raw_data = NULL;
        display->life_arr = NULL;
    }
    return 1;
}

int set_raw_data_item(mmon_display_t *display, int index, idata_entry_t *curInfoEntry) {

    // Checking if the node is valid:
    display->life_arr[index] = ((curInfoEntry->valid != 0) &&
            (curInfoEntry->data->hdr.status & INFOD_ALIVE));

    void *node_raw_ptr = (void *)((long)display->raw_data + display->block_length * index);
    // Filling the data into the apropriate entry. This should be the only
    // place where specific data is accessed
    if (display->life_arr[index]) {
        for (int dispModule = 0; dispModule < infoDisplayModuleNum; dispModule++) {
            new_item(dispModule, &glob_info_map, curInfoEntry,
                    (void*) ((long) node_raw_ptr + get_pos(dispModule)),
                    &current_set);
        }
    } else {
        // Extracting the status for a dead node
        if (dbg_flg) fprintf(dbg_fp, "#%i is DEAD!\n", index);

        // Adding the num info so the number of the nodes will be displayed
        new_item(dm_getIdByName("num"), &glob_info_map, curInfoEntry,
                (void*) ((long) node_raw_ptr + get_pos(dm_getIdByName("num"))),
                &current_set);

        // Adding the infod-status so we can see why it is dead
        new_item(dm_getIdByName("infod-status"), &glob_info_map, curInfoEntry,
                (void*) ((long) node_raw_ptr + get_pos(dm_getIdByName("infod-status"))),
                &current_set);


    }
    return 1;
}


int get_nodes_to_display(mon_disp_prop_t* display)
//This is The heart of the mmon:
//Here, the data is obtained from the host(s).
//If the "get_all" is 1, then all avalable data (and not just what is
//currently displayed) is loaded to the raw_data array.

{
    idata_t *infod_data = NULL;
    idata_entry_t *curInfoEntry = NULL;
    idata_iter_t *iter = NULL;
    //void* temp; //temp place-holder for numbers

    int i, j; //temp vars
    struct sigaction act;
    struct itimerval timeout;

    int index; //temp var for loops

    if (display == NULL)
        return 1;

    if (display->need_dest) {
        new_host(display, NULL);
        return 1;
    }

    // TODO : move this line out of here
    //bottom status line
    display_totals(display, display->show_status);

    if(!get_data_from_infod(display, &infod_data))
        return 0;

    display->last_host = display->mosix_host;

    if (!(iter = idata_iter_init(infod_data))) {
        free(infod_data);
        return 0;
    }

    // Calculating the memory size for each node and allocating memory
    if (display->raw_data == NULL) {
        allocate_display_raw_data_mem(display, infod_data->num);
    }

    //set needed info in the current_set
    set_settings(display, &current_set);

    for (i = 0; i < infod_data->num; i++) {
        //first loop only to insert MACHINE NUMBERS
        if (!(curInfoEntry = idata_iter_next(iter)))
            break;


        set_raw_data_item(display, i, curInfoEntry);
        //new_item(dm_getIdByName("num"), &glob_info_map, curInfoEntry,
        //        (void*) ((long) display->raw_data + display->block_length * i +
        //        get_pos(dm_getIdByName("num"))), &current_set);
    }


    display->nodes_count = infod_data->num;
    mlog_bn_db("info", "Data count : %d\n", display->nodes_count);

    idata_iter_done(iter);

    // Sort the raw_data array by number
    qsort(display->raw_data,
            display->nodes_count,
            display->block_length,
            &compare_num);

    //temp = (char*) malloc(display->block_length);
    // temp will use now as a place-holder for numbers.
    // each new number will be put in temp,
    // and then compared with others to find the correct index.

    //if (!(iter = idata_iter_init(infod_data))) {
    //    free(infod_data);
    //    return 0;
    //}
    // Second loop, to put values in correct order
    //for (i = 0 ; i < infod_data->num ; i++) {
    //    if (!(curInfoEntry = idata_iter_next(iter)))
    //        break;

    //    set_raw_data_item(display, curInfoEntry);
    //}

    free(infod_data);
    //free(temp);
    if (dbg_flg) fprintf(dbg_fp, "Data retrieved.\n");

    return 1;
}

