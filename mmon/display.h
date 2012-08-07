/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __MMON_DISPLAY
#define __MMON_DISPLAY

#include <glib-2.0/glib.h>

#ifdef  __cplusplus
extern "C" {
#endif

#include "DisplayModules.h"

    //LEGEND

    typedef struct legend_node {
        int data_type; //displayed type
        struct legend_node* next; //pointer to next
    } legend_node_t;

    typedef struct legend_list {
        struct legend_node* head; //pointer to first
        struct legend_node* curr_ptr; //pointer to current type (NULL if at end)
        int legend_size; //amount of types in legend
    } legend_list_t;

    typedef enum SideWindowType {
        SIDE_WIN_NONE = 0,
        SIDE_WIN_NODE_INFO,
        SIDE_WIN_LEGEND,
        SIDE_WIN_STATS,
        SIDE_WIN_MAX,
    } SideWindowType_T;

    typedef enum StdSpeedMode {
        SSPEED_FIXED_VALUE = 0, // A fixed value used for std speed
        SSPEED_FASTEST_NODE,    // Find fastest node and use its speed as the std speed
        SSPEED_ALL_EQUAL,       // ALL nodes are equal        
    } StdSpeedMode_T;

    //DISPLAY

    typedef struct mon_disp_prop
    //main display management structure (representing a single graph) 
    {
        //POSITION:
        int min_row;
        int max_row;
        int min_col;
        int max_col;

        //DATA ARRAY:
        //This is a continuous array of data, in which each station (computer providing
        //data) has a fixed memory space, divided into slots. The slots' content is
        //managed by the display module (like the load, or utilization module).

        void *raw_data;
        int nodes_count; //number of monitored machines
        int block_length; //memory alocated for every single block (a block per
        //machine)
        int *alive_arr; //1 states corresponding machine is alive (0 for dead).

        //A pointer to a cluster type, stating which one is in the data array.
        cluster_entry_t* cluster;

        //A cluster list, for multicluster mode (NULL, otherwise)
        clusters_list_t* clist;

        //The host for the remote monitoring
        char *info_src_host;

        //The last (successful) host string
        char* last_host;

        //DISPLY ARRAY:
        //This complex actually consists of data structures: 
        //The Pointer array is an array of pointers, in which each used colomn on the
        //screen has a pointer to data in the data array.
        //NULL pointer states an empty colomn, like the ones used for spacing.
        //"displayed" indicates the amount of machines on display
        //"legend" is a linked list, which stores the format of display for each
        //machine, where every node in the list holds a display type, representing
        //a singlee colomn in the graph.
        void **displayed_bars_data;
        int displayed_bar_num;
        int selected_node; // The selected node number
        legend_list_t legend;

        //THE WINDOWS  
        WINDOW *graph; //The territory of the graph (axis included)
        WINDOW *dest; //destination input dialog
        WINDOW *wlegend; //displays useful statistics/legend
        WINDOW *wlegendBorder; //The window to draw the border of the legend window

        int lgdw; //width of left legend/stat. window
        int side_window_width_factor;
        int show_dead; //1 iff dead displayed
        int show_help; //1 if help shown, otherwise the graph shown
        SideWindowType_T side_win_type; //1 if legend displayed, otherwise statistics are, or node info
        mdPtr side_win_md_ptr; // Pointer to the mmon display which is currently used in the side window
        int side_win_md_index; // An index to the Display Module to use in case of SIDE_WIN_NODE_INFO
        int show_side_win;
        int show_cluster; //1 iff cluster displayed
        int show_status; //1 iff status line displayed
        int wmode; //to toggle width in basic mode:
        //1 - "Super Vertical"
        //2 - "Vertical"
        //3 - "wide"

        int last_lines; //to detect screen resizing
        int last_cols; //to detect screen resizing

        int left_spacing; //for graph position
        int bottom_spacing; //for graph position

        int recount; //wheather there is a need to recount the displayed or not
        int need_dest; //wheather there is a need to find a new destination
        int help_start; //notes where to start the help str printout

        StdSpeedMode_T sspeed_mode;  // Stdspeed mode to use
        float sspeed;

        int NAcounter; //counts cycles of N\A data
        int max_counter; //counts cycles of unchanged max
        double last_max; //holds the previous max value (for max value delay)
        int last_max_type; //holds the type of the last max value

        int filter_nodes_by_name;
        GHashTable *nodes_to_display_hash;


    } mon_disp_prop_t;



    typedef mon_disp_prop_t mmon_display_t;

    extern mon_disp_prop_t** glob_curr_display; //pointer to selected screen.

    void displayInit(mon_disp_prop_t* display);
    void displayFree(mon_disp_prop_t *display);
    void displayFreeData(mon_disp_prop_t* display);
    int displaySetHost(mon_disp_prop_t *display, char *host);
    int displaySetNodesToDisplay(mon_disp_prop_t *display, mon_hosts_t *hostList);
    int displayInitFromStr(mon_disp_prop_t *display, char *initStr);
    int displaySaveToStr(mmon_display_t *display, char *buff, int size);
    void displayShowMsg(mmon_display_t *display, char *msg);
    void displayEraseMsg(mmon_display_t *display);

    void displayNewSideWin(mmon_display_t *display);
    void displayDelSideWin(mmon_display_t *display);

    void displayShowTitle(mmon_display_t *display);

    int displayGetSelecteNodeDataIndex(mon_disp_prop_t* display);
    int displayGetFirstNodeInViewDataIndex(mon_disp_prop_t *display);
    int displayGetFirstNodeInViewNumber(mon_disp_prop_t *display);
    void displayIncSelectedNode(mmon_display_t *display, int step);
    void displayIncSideWindowWidth(mmon_display_t *display, int extraWidth);
    void displayDecSideWindowWidth(mmon_display_t *display, int extraWidth);
#ifdef  __cplusplus
}
#endif

#endif 
