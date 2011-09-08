/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/



#ifndef __GENERAL_DISPLAY_MODULES
#define __GENERAL_DISPLAY_MODULES

//DISP_NAME
int name_get_length (); 
char** name_display_help (); 
void name_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void name_del_item (void* addess);
void name_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double name_scalar_div_x (const void* item, double x);

//DISP_NUM
int num_get_length ();
char** num_display_help ();  
void num_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void num_del_item (void* addess);
void num_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double num_scalar_div_x (const void* item, double x);

//INFOD_STATUS
int infod_status_get_length ();
char** infod_status_display_help ();  
void infod_status_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void infod_status_del_item (void* addess);
void infod_status_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double infod_status_scalar_div_x (const void* item, double x);

//DISP_LOAD
int load_get_length ();
char** load_display_help ();  
void load_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void load_del_item (void* addess);
void load_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double load_scalar_div_x (const void* item, double x);

//DISP_LOAD_VAL
int load_val_get_length ();
char** load_val_display_help ();  
void load_val_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void load_val_del_item (void* addess);
void load_val_display_item (WINDOW* graph, Configurator* pConfigurator, 
                            const void* source, int base_row, int min_row, int col, const double max, int width);
double load_val_scalar_div_x (const void* item, double x);

//DISP_LOAD2
int load2_get_length ();
char** load2_display_help ();  
void load2_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void load2_del_item (void* addess);
void load2_display_item (WINDOW* graph, Configurator* pConfigurator, 
                         const void* source, int base_row, int min_row, int col, const double max, int width);
double load2_scalar_div_x (const void* item, double x);

//DISP_MEM
int mem_get_length (); 
char** mem_display_help (); 
void mem_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void mem_del_item (void* addess);
void mem_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double mem_scalar_div_x (const void* item, double x);

//DISP_SWAP
int swap_get_length (); 
char** swap_display_help (); 
void swap_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void swap_del_item (void* addess);
void swap_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double swap_scalar_div_x (const void* item, double x);

//DISP_DISK
int disk_get_length (); 
char** disk_display_help (); 
void disk_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void disk_del_item (void* addess);
void disk_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double disk_scalar_div_x (const void* item, double x);

//DISP_FREEZE_SPACE
int freeze_space_get_length (); 
char** freeze_space_display_help (); 
void freeze_space_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void freeze_space_del_item (void* addess);
void freeze_space_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double freeze_space_scalar_div_x (const void* item, double x);

//DISP_FREEZE_SPACE_VAL
int freeze_space_val_get_length (); 
char** freeze_space_val_display_help (); 
void freeze_space_val_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void freeze_space_val_del_item (void* addess);
void freeze_space_val_display_item (WINDOW* graph, Configurator* pConfigurator, 
                                    const void* source, int base_row, int min_row, int col, const double max, int width);
double freeze_space_val_scalar_div_x (const void* item, double x);

//DISP_STATUS
int status_get_length ();
char** status_display_help ();  
void status_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void status_del_item (void* addess);
void status_display_item (WINDOW* graph, Configurator* pConfigurator, 
                          const void* source, int base_row, int min_row, int col, const double max, int width);

//DISP_FROZEN
int frozen_get_length ();
char** frozen_display_help ();  
void frozen_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void frozen_del_item (void* addess);
void frozen_display_item (WINDOW* graph, Configurator* pConfigurator, 
                          const void* source, int base_row, int min_row, int col, const double max, int width);
double frozen_scalar_div_x (const void* item, double x);

//DISP_VERSION
int version_get_length ();
char** version_display_help ();  
void version_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void version_del_item (void* addess);
void version_display_item (WINDOW* graph, Configurator* pConfigurator, 
                           const void* source, int base_row, int min_row, int col, const double max, int width);
double version_scalar_div_x (const void* item, double x);

//DISP_UPTIME
int uptime_get_length ();
char** uptime_display_help ();  
void uptime_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void uptime_del_item (void* addess);
void uptime_display_item (WINDOW* graph, Configurator* pConfigurator, 
                           const void* source, int base_row, int min_row, int col, const double max, int width);
double uptime_scalar_div_x (const void* item, double x);

//DISP_GRID
int grid_get_length ();
char** grid_display_help ();  
void grid_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void grid_del_item (void* addess);
void grid_display_item (WINDOW* graph, Configurator* pConfigurator, 
                          const void* source, int base_row, int min_row, int col, const double max, int width);

//DISP_TOKEN
int token_get_length ();
char** token_display_help ();  
void token_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void token_del_item (void* addess);
void token_display_item (WINDOW* graph, Configurator* pConfigurator, 
                          const void* source, int base_row, int min_row, int col, const double max, int width);

//DISP_SPEED
int speed_get_length ();
char** speed_display_help ();  
void speed_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void speed_del_item (void* addess);
void speed_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double speed_scalar_div_x (const void* item, double x);

//DISP_UTIL
int util_get_length ();
char** util_display_help ();  
void util_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void util_del_item (void* addess);
void util_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double util_scalar_div_x (const void* item, double x);

// IO Wait information 
int iowait_get_length ();
char** iowait_display_help ();  
void iowait_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void iowait_del_item (void* addess);
void iowait_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double iowait_scalar_div_x (const void* item, double x);


//DISP_NFS_RPC
int nfs_rpc_get_length ();
char** nfs_rpc_display_help ();  
void nfs_rpc_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void nfs_rpc_del_item (void* addess);
void nfs_rpc_display_item (WINDOW* graph, Configurator* pConfigurator, 
                           const void* source, int base_row, int min_row, int col, const double max, int width);
double nfs_rpc_scalar_div_x (const void* item, double x);

//DISP_RIO
int rio_get_length ();
char** rio_display_help ();  
void rio_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void rio_del_item (void* addess);
void rio_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double rio_scalar_div_x (const void* item, double x);

//DISP_WIO
int wio_get_length ();
char** wio_display_help ();  
void wio_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void wio_del_item (void* addess);
void wio_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double wio_scalar_div_x (const void* item, double x);

//DISP_TIO
int tio_get_length ();
char** tio_display_help ();  
void tio_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void tio_del_item (void* addess);
void tio_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double tio_scalar_div_x (const void* item, double x);

//DISP_NET_IN
int net_in_get_length ();
char** net_in_display_help ();  
void net_in_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void net_in_del_item (void* addess);
void net_in_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double net_in_scalar_div_x (const void* item, double x);

//DISP_NET_OUT
int net_out_get_length ();
char** net_out_display_help ();  
void net_out_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void net_out_del_item (void* addess);
void net_out_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double net_out_scalar_div_x (const void* item, double x);

//DISP_NET_TOTAL
int net_total_get_length ();
char** net_total_display_help ();  
void net_total_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void net_total_del_item (void* addess);
void net_total_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);
double net_total_scalar_div_x (const void* item, double x);

//DISP_USING_CLUSTERS
int using_clusters_get_length ();
char** using_clusters_display_help ();  
void using_clusters_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void using_clusters_del_item (void* addess);
void using_clusters_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);

//DISP_USING_USERS
int using_users_get_length ();
char** using_users_display_help ();  
void using_users_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void using_users_del_item (void* addess);
void using_users_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col, const double max, int width);

//INFOD_DEBUG
int infod_debug_get_length ();
char** infod_debug_display_help ();  
void infod_debug_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void infod_debug_del_item (void* addess);
void infod_debug_display_item (WINDOW* graph, Configurator* pConfigurator, 
                       const void* source, int base_row, int min_row, int col, const double max, int width);



//The Function declerations, for each dispaly mode:
//The implementations reside in "modules.c"
// ECO INFO
int eco_info_get_length ();
char** eco_info_display_help ();  
void eco_info_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void eco_info_del_item (void* addess);
void eco_info_display_item (WINDOW* graph, Configurator* pConfigurator, 
                      const void* source, int base_row, int min_row, int col, const double max, int width);
double eco_info_scalar_div_x (const void* item, double x);

// ECO2 INFO
int eco2_info_get_length ();
char** eco2_info_display_help ();  
void eco2_info_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void eco2_info_del_item (void* addess);
void eco2_info_display_item (WINDOW* graph, Configurator* pConfigurator, 
                      const void* source, int base_row, int min_row, int col, const double max, int width);


// ECO3 INFO
int eco3_info_get_length ();
char** eco3_info_display_help ();  
void eco3_info_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void eco3_info_del_item (void* addess);
void eco3_info_display_item (WINDOW* graph, Configurator* pConfigurator, 
                      const void* source, int base_row, int min_row, int col, const double max, int width);
double eco3_info_scalar_div_x (const void* item, double x);

// ECO4 INFO
int eco4_info_get_length ();
char** eco4_info_display_help ();  
void eco4_info_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void eco4_info_del_item (void* addess);
void eco4_info_display_item (WINDOW* graph, Configurator* pConfigurator, 
                      const void* source, int base_row, int min_row, int col, const double max, int width);
double eco4_info_scalar_div_x (const void* item, double x);

// JMIG 
int jmig_get_length ();
char** jmig_display_help ();  
void jmig_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void jmig_del_item (void* addess);
void jmig_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, int min_row, int col,
                        const double max, int width);
double jmig_scalar_div_x (const void* item, double x);


//CLUSTER_ID
int cluster_id_get_length ();
char** cluster_id_display_help ();  
void cluster_id_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void cluster_id_del_item (void* addess);
void cluster_id_display_item (WINDOW* graph, Configurator* pConfigurator, 
                         const void* source, int base_row, int min_row, int col, const double max, int width);
double cluster_id_scalar_div_x (const void* item, double x);

//SPACE
int space_get_length ();
char** space_display_help ();  
void space_new_item (mmon_info_mapping_t* iMap, const void* source, void* dest, settings_t* setup);
void space_del_item (void* addess);
void space_display_item (WINDOW* graph, Configurator* pConfigurator, 
                         const void* source, int base_row, int min_row, int col, const double max, int width);


#endif
