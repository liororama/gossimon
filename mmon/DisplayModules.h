/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __INFO_DISPLAY_MODULE
#define __INFO_DISPLAY_MODULE

#include "mmon.h"

#include <info.h>
#include <infolib.h>
#include <info_reader.h>
#include <info_iter.h>


//#define PARAM 36
//Number of the last item in disp_item_t //(considering first = -1...).


typedef struct settings
//Since the design is based on the fact all the display-type functions follow
//a predefined template, it is practical to add specific function parameters
//where it's needed. this is why we need some different way of getting fresh 
//data, only relevent for a specific function, in time for it's creation.
//This data structure is responsible for delivering data to the calculations
//of any display type.
{
  float* yardstick; //this points to the current yardstick speed
  int clsinit; //1 iff the cluster marks table should be inited
  unsigned short uid; //contains user id for load iff option activated 
} settings_t;

extern settings_t current_set; //current settings fed to the new data.


//The Module functions prototype
typedef int     (*init_func_t)         ();

typedef int     (*length_func_t)       ();

typedef char**  (*help_func_t)         ();


typedef void    (*new_item_func_t)     (mmon_info_mapping_t* /*Data pool*/, 
                                        const void* /*Data source*/, 
                                        void* /*Data destination*/,
                                        settings_t* /*Additinal info*/);

typedef void    (*del_item_func_t)     (void* /*Data source*/);

typedef double  (*scalar_func_t)       (const void* /*Data source*/, 
                                        double /*factor*/);

typedef void    (*display_bar_func_t)  (WINDOW*       /*Target graph*/, 
                                        Configurator* /*Color pallets*/, 
                                        const void*   /*Data source*/, 
                                        int           /*Base row index*/, 
                                        int           /*Min row index*/, 
                                        int           /*Colomn index*/, 
                                        const double  /*Maximal value*/,
                                        int           /*Width mode*/);

typedef int     (*text_info_func_t)    (void *,   // data
                                        char *,  // buffer to print info to 
                                        int *,   // size of buffer
                                        int     // width of print area 
                                        );

typedef void    (*update_info_desc_func_t)     (void*,
                                                variable_map_t *info_mapping
                                                );

typedef struct mon_disp
{
  int                md_item;        // Type of item
  char              *md_name;        // Name of display (short name)
  char              *md_info_item;   // The name of the informaton item required by this module
  int                md_iscalar;      // Is value a scalar (comparable)
  int                md_canrev;      // Is it allowd to use reverse mode here
  char              *md_title;       // Title of display
  char              *md_shortTitle;  // Short title for the side window
  int                md_titlength;   // Length of title string
  char              *md_format;      // Format of value displa
  int                md_key;         // key binding for item
  init_func_t        md_init_func;   // Optional initialization function of module
  length_func_t      md_length_func; // Length function
  scalar_func_t      md_scalar_func; // returns a scalar form if possible
  new_item_func_t    md_new_func;    // Item creation function
  del_item_func_t    md_del_func;    // Item disposal function
  display_bar_func_t md_disp_func;   // Bar drawing function
  help_func_t        md_help_func;   // Help discription function
  text_info_func_t   md_text_info_func; // Textual info of node (instead of bar)
  update_info_desc_func_t md_update_info_desc_func; 
  var_t             *md_info_var_map;     // Var Mapper of the info;

  int                md_side_window_display; // 1 or 0 if the module has a side window display
  
  int               *md_debug;       // Address of debug flag
  FILE              *md_debugFp;     // File pointer to send debug output to
  int               _not_used[8];    // For future uses of plugins
} mon_display_module_t;


#define MAX_INFO_DISPLAY_MODULES     (100)
extern int infoDisplayModuleNum;
extern mon_display_module_t mon_displays[];


int initInfoDisplay();
int registerInfoDisplayModule(mon_display_module_t *module);
mon_display_module_t *getInfoDisplayByName(char *name);
mon_display_module_t *getInfoDisplayById(int id);
int dm_getIdByName(char *name);

void listInfoDisplays(FILE *fp);
int displayModule_init();

int displayModule_updateInfoDescription();
int displayModule_initModules();
int displayModule_getModulesWithTextInfo(int *idArr, int *size);

int displayModule_registerStandardModules();

int displayModule_detectExternalPlugins(PluginConfig_t *p);
int displayModule_registerExternalPlugins(PluginConfig_t *p);

typedef mon_display_module_t *mdPtr;
int getMDWithSideWindow(mdPtr *modulePtrArr);

extern mon_display_module_t name_mod_info;
extern mon_display_module_t num_mod_info;
extern mon_display_module_t status_mod_info;
extern mon_display_module_t infod_status_mod_info;
extern mon_display_module_t version_mod_info;
extern mon_display_module_t kernel_version_mod_info;
extern mon_display_module_t uptime_mod_info;
extern mon_display_module_t load_mod_info;
extern mon_display_module_t load_val_mod_info;
extern mon_display_module_t frozen_mod_info;
extern mon_display_module_t load2_mod_info;
extern mon_display_module_t mem_mod_info;
extern mon_display_module_t swap_mod_info;
extern mon_display_module_t disk_mod_info;
extern mon_display_module_t freeze_space_mod_info;
extern mon_display_module_t freeze_space_val_mod_info;
extern mon_display_module_t grid_mod_info;
extern mon_display_module_t token_mod_info;
extern mon_display_module_t speed_mod_info;
extern mon_display_module_t util_mod_info;
extern mon_display_module_t iowait_mod_info;

extern mon_display_module_t rio_mod_info;
extern mon_display_module_t wio_mod_info;
extern mon_display_module_t tio_mod_info;
extern mon_display_module_t net_in_mod_info;
extern mon_display_module_t net_out_mod_info;
extern mon_display_module_t net_total_mod_info;
extern mon_display_module_t nfs_rpc_mod_info;
extern mon_display_module_t using_clusters_mod_info;
extern mon_display_module_t using_users_mod_info;
extern mon_display_module_t infod_debug_mod_info;
extern mon_display_module_t cluster_id_mod_info;
extern mon_display_module_t ncpus_mod_info;


// Economy Info
extern mon_display_module_t eco_info_mod_info;
extern mon_display_module_t eco2_info_mod_info;
extern mon_display_module_t eco3_info_mod_info;
extern mon_display_module_t eco4_info_mod_info;
extern mon_display_module_t eco5_info_mod_info;

extern mon_display_module_t seperator_mod_info;

extern mon_display_module_t space_mod_info;


#endif
