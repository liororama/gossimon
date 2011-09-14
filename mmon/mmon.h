/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __MMON_H
#define __MMON_H

#include <stdio.h>
#include <stdlib.h>
#include <termio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <curses.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/types.h>
#include <pwd.h>


#include <info.h>
#include <infolib.h>
#include <info_reader.h>
#include <info_iter.h>


#include <msx_common.h>
#include <pe.h>
#include <distance_graph.h>

#include "gossimon_config.h"
#include "parse_helper.h"
#include "host_list.h"
#include "ColorPrint.h"
#include "ModuleLogger.h"
#include "pluginUtil.h"

#include "host_list.h"
#include "cluster_list.h"
#include "ConfigurationManager.h"
#include "UsedByInfo.h"
#include "ProcessWatchInfo.h"
#include "FreezeInfo.h"
#include "InfodDebugInfo.h"

//#include "infoDisplayModule.h"

#define KB1                     (1024)
#define MG1                     (KB1*KB1)
#define MAX_REPEATED_ERRORS     (5)
#define MAX_CONF_FILE_LEN       1024
#define CONNECT_TIMEOUT         5

#define units_per_mb            MG1/MEM_UNIT_SIZE


#define MIN_LINES             (20) // minimal number of lines for mmon to work
#define MIN_COLS              (30) // minimal number of lines for mmon to work
#define LSPACE                (7)  // space on the left of the chart (for maximal values)
#define MAX_SPLIT_SCREENS     (5)  // maximal amount of permitted screens
#define MAX_HOST_NAME         (100)// longest possible host name from input
#define mmon_connect_timeout  (5)  // timeout untill node deemed "dead"
#define max_val_reset_timeout (3)  // timeout to change of max value in display
#define EXIT_TIMEOUT          (3)  // the time you have to confirm an exit command


#define MMON_DEF_CONFIG_FILE2           "mmon.conf"
#define MMON_DEF_CONFIG_FILE            ".mmon.conf"
#define MMON_DEF_WINDOW_SAVE_FILE       ".mmon.winsave"

/****************************************************************************
 *
 *                     DATA RETRIEVAL
 *
 * A structure that holds all the mappings that are used to access
 * the retrieved information. The structure is initialized once the
 * the description is obtained from the infod
 ***************************************************************************/

// A structure that holds all the mappings that are used to access
// the retrieved information (The structure is initialized once the
// the description is obtained from the infod):
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

#define NEXT_CLUSTER   (1)
#define PREV_CLUSTER   (2)

typedef enum {
	PE_DISP_REG,
	PE_DISP_GRID_MODE,
} pe_disp_t;



//Declarations for Vlen items
void getVlenItem(node_info_t *ninfo, var_t *infoVar, char **vlenItem);



//DRAWING FUNCTIONS:
//These functions are used by the module functions 
double scalar_div_x(int item, const void* address, double x);

//to display the output to the graph.
void print_str(WINDOW* target, ColorDescriptor* pDesc,
               int row, int col, char *str, int len, int rev);
     //outputs a string of given length in (row, col)

void display_str(WINDOW* target, ColorDescriptor* pDesc,
                 int base_row, int col, char *str, int len, int rev);
     //outputs a vertical form of the given string.

void display_rtr(WINDOW* target, ColorDescriptor* pDesc,
                 int base_row, int col, char *str, int len, int rev);
     //outputs a vertical form of the given string - reversed.

void display_bar (WINDOW* target, int base_row, int col, int total,
                  char usedChar, ColorDescriptor *pDesc, int rev);
     //draws a "total"-sized bar of "usedChar"s in target window. 

/* void display_part_bar (WINDOW* target, int base_row, int col,  */
/*                        int total, char freeChar,  */
/*                        ColorDescriptor *freeColor, int revfree, */
/*                        int used, char usedChar,  */
/*                        ColorDescriptor *usedColor, int revused, */
/*                        ColorDescriptor *errorColor); */
/*      //draws 2 bars - both max and actual values. */

//get_max is declared here, to be use to determine highest speed displayed.
int isScalar(int item);
void display_item(int item, WINDOW* graph, void* source,
        int base_row, int min_row, int col, const double max, int width);

extern Configurator* pConfigurator;


//The Map array is designed to map the mon_displays array, for gaining fast
//access to it's members. there is no need to go through ALL the cells to find
//a certain display mode - the map provides access by using the enums like
//"map[SPACE]" to obtain a pointer to the mon_disp_t-type member.
//The map is constructed in init, so that the order of the mon_displays array
//in this .h file will not matter (in case it's changed).
//mon_disp_t **map;



#include "display.h"
double get_max(mon_disp_prop_t* display, int item);
double avg_by_item(mon_disp_prop_t* display, int item);
void show_manual(mon_disp_prop_t* display);

void init_clusters_list(mon_disp_prop_t* display);
void toggle_disp_type(mon_disp_prop_t* display, int item, int action);
void size_recalculate ();
void del_item (int item, void* address);
int get_pos (int item);
int get_text_info_byName(char *name, void *nodeDataPtr, char *buff, int *len, int width);

void terminate ();
//terminates the current display


void displayRedrawGraph (mon_disp_prop_t* display);           
//clears and draws the graph all over again

void displayRedraw (mon_disp_prop_t* display);           
//clears and draws a single display screen 
int get_nodes_to_display (mon_disp_prop_t* display);


//recalculates positions of active displays.

extern mon_disp_prop_t** curr_display; //pointer to selected screen.
extern mon_disp_prop_t** glob_displaysArr; //array of screens on display.


extern int    dbg_flg;
extern int    dbg_flg2;
extern FILE  *dbg_fp;

extern int    exiting;
extern int    max_key; 
extern int  **key_map; 

extern mon_display_module_t** infoModulesArr; 

#define MAX_START_WIN      (10)
#define MAX_CONF_FILE_LEN 1024 //maximal length


typedef struct mmon_data {
  
  mmon_display_t **displayArr;
  int              colorMode;
  
  int              startWinFromCmdline;
  int              skeepWinfile;
  char            *winSaveFile;
  char            *startWinStrArr[MAX_START_WIN];
  char             confFileName[MAX_CONF_FILE_LEN + 1]; //string of configuration file name
  char            *nodesArgStr;
  
  mon_hosts_t      hostList;

  
} mmon_data_t;

int saveCurrentWindows(mmon_data_t *md);
void mmon_exit(int status);
void new_yardstick(mon_disp_prop_t* display);

// From displaySaveFile
int displayFileSave(char *name, char **displayStrArr, int num);
int displayFileLoad(char *name, char **displayStrArr, int *num);


// From input.c

int is_input();
void ch_alarm();
void not_yet_ch(int ch);
int readc_half_second();
int my_getch();
void parse_cmd (mmon_data_t *md, int pressed);        
void onio();
void sleep_or_input(int secs);




// From helpStr.c
extern char *head_str[];
extern char *help_str[];
void printUsage(FILE *fp);
void printCopyright(FILE *fp);



int get_str_length (char** man_str);
void move_left (mon_disp_prop_t* display);
void move_right (mon_disp_prop_t* display);
void new_host(mon_disp_prop_t* display, char* request);
void new_user(mon_disp_prop_t* display);
int get_raw_pos (mon_disp_prop_t* display, int index);

char** display_help (int item);

// From dialogs
void selected_node_dialog(mon_disp_prop_t* display, Configurator *config);

// From NodeInfoWin
void toggleSideWindowMD(mon_disp_prop_t *display);
void show_node_info_side_win(mon_disp_prop_t* display);

// From mmonCommandLine
int parseCommandLine(mmon_data_t *md, int argc, char **argv, mon_disp_prop_t *prop);

#endif 

