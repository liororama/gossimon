/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


// The display structure management

#include <string.h>

#include "parse_helper.h"
#include "mmon.h"
#include "DisplayModules.h"
#include "display.h"
#include "drawHelper.h"

//SCREEN INIT
void displayInit (mmon_display_t* display)  
{
  //format init (empty)
  (display->legend).head = NULL;
  (display->legend).legend_count = 0;

  //data init is done in "update_data", so:
  display->raw_data = NULL;

  //init current_set settings
  current_set.yardstick = NULL;

  //init main display position:
  display->max_row      = LINES; //last row allocated to display
  display->max_col      = COLS;  //last col allocated to display
  display->min_row      = 1;     //first col allocated to display
  display->min_col      = 0;     //first col allocated to display

  display->display_data = NULL;  //displayed data
  display->raw_data     = NULL;  //all available data for display
  display->life_arr     = NULL;  //index of availability of nodes
  display->mosix_host   = NULL;  //host address char array
  display->last_host    = NULL;  //last successful host string

  display->graph        = NULL;  // The territory of the graph
  display->wlegend      = NULL;  // Displays side window (legend,sideinfo,stats)
  display->wlegendBorder= NULL;  // Border for the side window
  display->last_cols    = COLS;  //initial display width
  display->last_lines   = LINES; //initial display hight

  display->selected_node = -1;  // No selected node
  display->side_window_width_factor = 4;
  display->lgdw         = COLS/ display->side_window_width_factor;//width of left legend window
 
  display->show_dead    = 1;     //1 iff dead displayed
  display->show_help    = 0;     //1 if help shown, otherwise the graph is displayed
  display->show_side_win = 0;    // show or not show the side window
  display->side_win_type  = SIDE_WIN_NONE; // The side window type
  display->side_win_md_index = -1; // Start by showing the first module with side window 
  display->side_win_md_ptr = NULL;
  display->show_cluster = 0;     //1 iff cluster displayed
  display->show_status  = 1;     //1 iff status line displaye
  display->data_count   = 0;     //holds the current node count - none.
  display->wmode        = 2;     //to toggle width in basic mode

  display->recount      = 1;     //to init the displayed count
  display->need_dest    = 1;     //to config new destination

  display->NAcounter    = 0;     //inits the NA time to 0
  display->max_counter  = 0;     //counts cycles of unchanged max
  display->last_max     = -1;    //holds the previous max value 
  display->help_start   = 0;     //notes where to start the help string

  display->sspeed       = MOSIX_STD_SPEED;

  display->left_spacing = 
    LSPACE + (display->wlegend != NULL) * (display->lgdw);  
  display->bottom_spacing = 
    display->show_cluster + display->show_status + 2; 
  
  //cluster list init
  display->clist = (clusters_list_t*)malloc(sizeof(clusters_list_t));
  cl_init(display->clist);

  //if the window is not the first, it takes the destination
  //configuration from the current display 
  if ((curr_display) && ((*curr_display)->last_host)) {
       displaySetHost(display, (*curr_display)->last_host);
       display->show_dead = (*curr_display)->show_dead;
       display->wmode = (*curr_display)->wmode;
  }
//    new_host(display, strdup((*curr_display)->last_host));
  else
       init_clusters_list(display);
  
  //if display is empty - start default view
  if ((display->legend).legend_count == 0) {
       mlog_bn_dg("disp", "Setting display to load\n");
       toggle_disp_type(display, dm_getIdByName("load"), 1); //default display
  }
  if (curr_display)
       size_recalculate(1); //recalculate sizes
  
  if (dbg_flg) fprintf(dbg_fp, "Screen init complete. \n");
}


// Deletes the legend (includig malloced memory)
void displayLegendFree (mmon_display_t* display)
{
  legend_node_t *free_lgd;
  if ((display->legend).head != NULL)
    {
      if (dbg_flg) fprintf(dbg_fp, "Legend freed.\n");
      free_lgd = (display->legend).head;
      (display->legend).head = ((display->legend).head)->next;
      free(free_lgd);
      displayLegendFree(display);
    }
}

void displayFree(mmon_display_t *display) {

  if (display->raw_data) {
       displayFreeData(display);
  }  
  
  if (display->clist) {
       cl_free(display->clist);
       free(display->clist);
  }
  
  if (display->last_host)
       free(display->last_host);  
  
  if (display->graph)
       delwin(display->graph);
  
  if (display->dest)
       delwin(display->dest);
  
  if (display->wlegend)
       delwin(display->wlegend);
  
  if(display->wlegendBorder)
       delwin(display->wlegendBorder);
  
  if (display->display_data)
       free(display->display_data);
  displayLegendFree(display);
  
  free(display);
}


//here is the cleanup of the memory potentially taken by 
//the raw data section. every display type might be holding
//pointers leading outside the raw_data array, and so we use each 
//"del_item" function to avoid memory leaks.
void displayFreeData(mmon_display_t* display)
{
     int i; //a var for indexing the display array
     
     for(i = 0 ; i < display->data_count ; i++ ) {
          for(int id=0 ; id < infoDisplayModuleNum ; id++)
          {
               del_item(id, 
                        (void*)((long)display->raw_data + display->block_length * i + 
                                get_pos(id)));
          }
     }  
     if (dbg_flg) fprintf(dbg_fp, "Data deleted.\n");

     if(display->raw_data) {
          free(display->raw_data);
          display->raw_data = NULL;
     }
     if (display->life_arr)
     {    
          free(display->life_arr);
          display->life_arr = NULL;
     }
     
}

int displaySetHost(mmon_display_t *display, char *host) {
     cl_free(display->clist);
     if(display->last_host)
          free(display->last_host);

     display->last_host = strdup(host);
     display->need_dest = 1 - 
          cl_set_single_cluster(display->clist, host);

     // FIXME this name should be changed to displayXXXXX
     init_clusters_list(display);
     return 1;
}

int displayClearLegend(mmon_display_t *display) {
     legend_node_t* lgd_ptr; //to iterate the legend
       
     while ((display->legend).legend_count > 0) //display types to erase
     {
          lgd_ptr = (display->legend).head;
          while (lgd_ptr->next != NULL)
               lgd_ptr = lgd_ptr->next;
          toggle_disp_type(display, lgd_ptr->data_type, 0);
     }
     return 1;
}

int displaySetLegendFromStr(mmon_display_t *display, char *legendStr) 
{
     int    size = 50;
     char  *args[50];
     int    items;
     items = split_str(legendStr, args, size, ","); 
     printf("Items: %d\n", items);

     // Clearing the legend

     for(int i=0 ; i<items ; i++) {
          printf("Setting display %s\n", args[i]);
          mon_display_module_t *infoDisp = getInfoDisplayByName(args[i]);
          if(!infoDisp) {
               fprintf(stderr, "Error: there is no display called: %s\n", args[i]);
               return 0;
          }
          toggle_disp_type(display, infoDisp->md_item, 1);
     }
     return 1;
}

int displaySetModeFromStr(mmon_display_t *display, char *modeStr) {
     int    size = 50;
     char  *args[50];
     int    items;
     items = split_str(modeStr, args, size, ","); 
     printf("Items: %d\n", items);
     
     for(int i=0 ; i<items ; i++) {
          printf("Setting display Mode: %s\n", args[i]);
          

          if(strcmp(args[i], "d") == 0 ||
             strcmp(args[i], "dead") == 0) {
               display->show_dead = 1;
               
          }
          else if(strcmp(args[i], "sv") == 0 ||
                  strcmp(args[i], "super-vertial") == 0) {
               display->wmode = 1;
          }
          else if(strcmp(args[i], "v") == 0 ||
                  strcmp(args[i], "vertial") == 0) {
               display->wmode = 2;
          }
          else if(strcmp(args[i], "w") == 0 ||
                  strcmp(args[i], "wide") == 0) {
               display->wmode = 3;
          }

    }
     return 1;
}


// "--startwin  win1:host=sorma-base win2:m host=sorma-base2:"
// "--startwin  mode=w,d disp=mem,load,space,f+l host=sorma-base2
int displayInitFromStr(mmon_display_t *display, char *initStr) {
  
     int    size = 50;
     char  *args[50];
     int    items;
     items = split_str(initStr, args, size, " \t"); 
     printf("Items: %d\n", items);
     
     for(int i=0 ; i < items ; i++) {
          printf("Item: %s\n", args[i]);
          
          if(strncmp(args[i], "host=", 5) == 0) {

               char *host = args[i]+5;
               printf("Found host item: %s\n", host);
               if(!displaySetHost(display, host))
                    return 0;
          } 
          else if(strncmp(args[i], "disp=", 5) == 0) {
               printf("Found host disp\n");
               displayClearLegend(display);
               if(!displaySetLegendFromStr(display, args[i] + 5))
                    return 0;
          }
          else if(strncmp(args[i], "mode=", 5) == 0) {
               printf("Found host mode\n");
               if(!displaySetModeFromStr(display, args[i] + 5))
                    return 0;
          }
     }
     
     return 1;
}

int displayGetHostStr(mmon_display_t *display, char *str) 
{
     char tmp[512];
     if(dbg_flg) fprintf(dbg_fp, "======================> Host: %s\n", display->last_host);
     sprintf(tmp, "host=%s ", display->last_host);
     strcat(str, tmp);
     return 1;
}


int displayGetLegendStr(mmon_display_t *display, char *str) 
{
     legend_node_t *legendNode = display->legend.head;
     char tmp[512];
     char tmp2[512];
     
     sprintf(tmp, "disp=");
     
     int i =0;
     while(legendNode) {
          //fprintf(dbg_fp, "Legend %d\n", legendNode->data_type);
          mon_display_module_t *infoDisp = getInfoDisplayById(legendNode->data_type);
          if(!infoDisp) {
               fprintf(stderr, "Error: there is no display with ID: %d\n", legendNode->data_type );
               return 0;
          }
          
          if(strlen(infoDisp->md_name) > 0 ) {
               sprintf(tmp2, i == 0 ? "%s" : ",%s", infoDisp->md_name);
               strcat(tmp, tmp2);
          }
          i++;
          legendNode = legendNode->next;
     }
     strcat(tmp, " ");
     strcat(str, tmp);
     
     return 1;
}

int displayGetModeStr(mmon_display_t *display, char *str) 
{
     char tmp[512];

     sprintf(tmp, "mode=");
     int i=0;

     if(display->show_dead) {
          strcat(tmp, i == 0 ? "dead" : ",dead");
          i++;
     }
     
     switch (display->wmode) {
     case 1: strcat(tmp, i == 0 ? "sv" : ",sv"); break; 
     case 2: strcat(tmp, i == 0 ? "v" : ",v"); break; 
     case 3: strcat(tmp, i == 0 ? "w" : ",w"); break; 
     }
     i++;

     strcat(tmp, " ");
     strcat(str, tmp);
     return 1;  
}

int displaySaveToStr(mmon_display_t *display, char *buff, int size) 
{
     
     char saveStr[1024];
     char tmp[1024];

     saveStr[0] = '\0';
     
     tmp[0] = '\0';
     displayGetHostStr(display, tmp);
     strcat(saveStr, tmp);

     tmp[0] = '\0';
     displayGetLegendStr(display, tmp);
     strcat(saveStr, tmp);

     tmp[0] = '\0';
     displayGetModeStr(display, tmp);
     strcat(saveStr, tmp);
     
     if(strlen(saveStr) < size) 
          strcpy(buff, saveStr);
     else 
          return 0;

     return 1;
}


void displayPrint(mmon_display_t *display, FILE *fp) {
     
     mmon_display_t *d = display;
     fprintf(fp, "Saving display\n\n\n");
     fprintf(fp,
             "Display:\n"
             "==========================\n"
             "min_raw:     %d \n"
             "max_raw:     %d \n"
             "min_col:     %d \n"
             "max_col:     %d \n"
             "mosix_host:  %s \n"
             "last_host:   %s \n"
             "lgdw:        %d \n"
             "show_dead:   %d \n"
             "show_help:   %d \n"
             "show side:   %d \n"
             "side window: %d \n"
             "show_cluster:%d \n"
             "show_status: %d \n"
             "wmode:       %d \n"
             "last_lines:  %d \n"
             "last_cols:   %d \n"
             "left_spacing %d \n"
             "botm_spacing %d \n"
             "recount:     %d \n"
             "need_dest:   %d \n"
             "help_start:  %d \n"
             "sspeed:      %f \n"
             "NAcounter:   %d \n"
             "max_counter: %d \n"
             "last_max:    %f \n"
             "last_max_type %d\n",
             d->min_row, d->max_row, d->min_col, d->max_col,
             d->mosix_host, d->last_host,
             d->lgdw, d->show_dead, d->show_help, 
             d->show_side_win,
             d->side_win_type, 
             d->show_cluster, 
             d->show_status, 
             d->wmode,
             d->last_lines, 
             d->last_cols,
             d->left_spacing, 
             d->bottom_spacing,
             d->recount, d->need_dest, d->help_start, d->sspeed,
             d->NAcounter, d->max_counter,
             d->last_max, d->last_max_type);
     
     legend_node_t *legendNode = display->legend.head;
     fprintf(dbg_fp, "WLEGEND %d\n", display->wlegend ? 1 : 0);
     fprintf(dbg_fp, "Legend Count %d\n", display->legend.legend_count);
     while(legendNode) {
          fprintf(dbg_fp, "Legend %d\n", legendNode->data_type);
          legendNode = legendNode->next;
     }
     
}

void displayShowMsg(mmon_display_t *display, char *msg) 
{
     //open the dialogue window:
     display->dest = 
          newwin(3, 
                 display->max_col - display->min_col - display->left_spacing - 4, 
                 display->min_row + (display->max_row - display->min_row) / 2,
                 display->min_col + display->left_spacing + 2);
     
     //present demand for new destination:
     //wc_on(display->dest, &(pConfigurator->Colors._chartColor));
     wborder(display->dest, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
             ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
     wmove(display->dest,1,2);    
     
     wprintw(display->dest, msg);
     wrefresh(display->dest);
}

void displayEraseMsg(mmon_display_t *display) 
{
     
     //wc_off(display->dest, &(pConfigurator->Colors._chartColor));
     werase(display->dest);
}

void displayDelSideWin(mmon_display_t *display) {

     if (display && display->wlegend) {
	  if(display->wlegendBorder) {
	       delwin(display->wlegendBorder);
	       display->wlegendBorder = NULL;
	  }
	  delwin(display->wlegend);
	  display->wlegend = NULL;
	  move(display->min_row, display->min_col); 
	  clrtobot(); //erase
     }
}

void displayNewSideWin(mmon_display_t *display) {
  move(display->min_row, display->min_col); 
  clrtobot(); //erase
  display->wlegend = newwin(1,1,0,0);
}


int displayGetSelecteNodeDataIndex(mon_disp_prop_t* display) {
     int index;
     void* temp_p;
     temp_p = display->raw_data;
     for (index = 0; index < display->data_count; index++, temp_p += display->block_length ) {
          //points to the currently displayed node
          int nodeNum = *((int*) ((long) temp_p + get_pos(dm_getIdByName("num"))));
          mlog_bn_dy("disp", "Index %d Number %d (selected %d)\n", index, nodeNum, display->selected_node);
          if(nodeNum == display->selected_node) 
               return index;
     }
     mlog_bn_dr("disp", "Index -1\n");
     return -1;
}

int displayGetFirstNodeInViewNumber(mon_disp_prop_t *display) {
    void *firstDisplayedNodeData;
    //points to the fist displayed node
    firstDisplayedNodeData = display->display_data[0];

    int firstInViewNodeNum = *((int*) ((long) firstDisplayedNodeData + get_pos(dm_getIdByName("num"))));
    mlog_bn_db("disp", "First in view node number [%d]\n", firstInViewNodeNum);

    return firstInViewNodeNum;
}

int displayGetFirstNodeInViewDataIndex(mon_disp_prop_t *display)
{
    int firstInViewNodeNum = displayGetFirstNodeInViewNumber(display);
    
    void *temp_p = display->raw_data;
    for (int index = 0; index < display->data_count; index++, temp_p += display->block_length) {
        //points to the currently displayed node
        int nodeNum = *((int*) ((long) temp_p + get_pos(dm_getIdByName("num"))));
        if (nodeNum == firstInViewNodeNum) {
            mlog_bn_dy("disp", "Index %d = Number %d\n", index, nodeNum);
            return index;
        }
    }
    return -1;
}


void displayIncSelectedNode(mmon_display_t *display, int step) {

  if(step == 0)
    return;

  if(display->side_win_type != SIDE_WIN_NODE_INFO)
    return;

  int index = displayGetSelecteNodeDataIndex(display);
  if(index == -1) {
    index = 0;
  }

  if(step > 0) {
    index += 1;
    if(index >= display->data_count)
      index--;

  } else if(step < 0) {
        index -= 1;
        if(index < 0)
          index = 0;
  }

  void *nodeDataPtr = display->raw_data + display->block_length * index;
  int nodeNum = *((int*) ((long) nodeDataPtr + get_pos(dm_getIdByName("num"))));
  display->selected_node = nodeNum;
  mlog_bn_db("disp", "New selected node [%d]\n", nodeNum);
  
}

void displayIncSideWindowWidth(mmon_display_t *display, int extraWidth) {
  display->side_window_width_factor--;
  if(display->side_window_width_factor < 2)
    display->side_window_width_factor = 2;
  
  display->lgdw = COLS / display->side_window_width_factor;
  display->recount = 1;
}
void displayDecSideWindowWidth(mmon_display_t *display, int extraWidth) {
  display->side_window_width_factor++;
  if(display->side_window_width_factor > 5)
    display->side_window_width_factor = 5;
  display->lgdw = COLS / display->side_window_width_factor;

  display->recount = 1;
}


void displayShowTitle(mmon_display_t *display) {
  
     char titleBuff[128];
     int titleSize;
     int middleCol = (display->max_col - display->min_col - display->left_spacing) / 2; 
     
     //cluster name
     if (*curr_display != display)
          //only the chosen graph has colored axis
          wc_on(display->graph, &(pConfigurator->Colors._chartColor));
     else
          wc_on(display->graph, &(pConfigurator->Colors._multicluster));
     

     if (current_set.uid != 65534) {
          titleSize = sprintf(titleBuff, "  [Cluster: %s  UID: %i]  ", display->cluster->name,
                              current_set.uid);
          mvwprintw(display->graph, 0, middleCol - titleSize/2, titleBuff);
     }
     else {
          titleSize = sprintf(titleBuff, "[Cluster: %s %d ]", 
                              display->cluster->name,
                              current_set.uid);
          
          mvwprintw(display->graph, 0, middleCol - titleSize/2, titleBuff);
     }
     
     if (*curr_display != display)
          //only the chosen graph has colored axis
          wc_off(display->graph, &(pConfigurator->Colors._chartColor));
     else
          wc_off(display->graph, &(pConfigurator->Colors._multicluster));
}

void display_cluster_id(mon_disp_prop_t* display, int draw)
//Displays a cluster id line, in which each cluster has it's own 
//latin capital letter (A, B, C...). 
//iff draw is 1 the line is (re)printed. otherwise only erased.
{
    int index, offset; //temp var
    void* temp_p; //temp pointer

    //delete a single line of numbering
    move(display->max_row - display->bottom_spacing + 1,
            display->min_col + display->left_spacing - 1);
    for (offset = display->min_col + display->left_spacing;
            offset <= display->max_col; offset++)
        printw(" ");

    if ((display->displayed > 0) && (display->data_count > 0) && (draw)) {
        //move to position
        move(display->max_row - display->bottom_spacing + 1,
                display->min_col + display->left_spacing + 1);

        offset = 0;
        c_on(&(pConfigurator->Colors._horizNodeName));
        for (index = 0; index < display->displayed / (display->legend).legend_count; index++) {
            //points to the currently displayed node
            temp_p = display->display_data[index * (display->legend).legend_count];

            //left space
            for (offset = 0;
                    offset < (display->legend).legend_count - 1 -
                    ((display->legend).legend_count - 1) / 2;
                    offset++)
                printw(" ");

            //the cluster id (should be A-Z or 0)
            offset++;
            printw("%c", (char)
                    scalar_div_x(dm_getIdByName("cluster-id"),
                    (void*) ((long) temp_p + get_pos(dm_getIdByName("cluster-id"))), 1));

            //right space
            for (; offset < (display->legend).legend_count; offset++)
                printw(" ");
        }
        c_off(&(pConfigurator->Colors._horizNodeName));
    }
}

void show_legend_side_win(mon_disp_prop_t* display)
//draws the legend on the left (in wlegend)
//note that the legend also asserts expert editing mode
{
    int index;
    legend_node_t* lgd_ptr;

    if (display->side_win_type != SIDE_WIN_LEGEND)
        //not to be displayed
        return;

    if (dbg_flg) fprintf(dbg_fp, "LEGEND.\n");

    if (display->wlegend != NULL)
        delwin(display->wlegend);

    //creating the window:
    display->wlegend = newwin(display->max_row - display->min_row - 1,
            display->lgdw, display->min_row,
            display->min_col);

    //printing the contents:
    mvwprintw(display->wlegend, 1, 1, "Legend: ");
    index = 1;
    lgd_ptr = (display->legend).head;
    while (lgd_ptr != NULL) {
        if (lgd_ptr == (display->legend).curr_ptr)
            wattron(display->wlegend, A_REVERSE);
        mvwprintw(display->wlegend, index + 1, 1, "%i)", index);
        if (lgd_ptr == (display->legend).curr_ptr)
            wattroff(display->wlegend, A_REVERSE);
        print_str(display->wlegend, &(pConfigurator->Colors._statusNodeName),
                index + 1, 4, infoModulesArr[lgd_ptr->data_type]->md_title,
                infoModulesArr[lgd_ptr->data_type]->md_titlength, 0);
        index++;
        lgd_ptr = lgd_ptr->next;
    }

    if (lgd_ptr == (display->legend).curr_ptr) {
        wattron(display->wlegend, A_REVERSE);
        mvwprintw(display->wlegend, index + 1, 1, "%i)", index);
        wattroff(display->wlegend, A_REVERSE);
    }

    wborder(display->wlegend, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);

    mvwprintw(display->wlegend,
            display->max_row - display->min_row - 2, 1,
            " z - toggle, x - close");

    wrefresh(display->wlegend);
}

void show_statistics_side_win(mon_disp_prop_t* display)
//draws the statistics on the left (in wlegend)
{
    int index;
    double avg;
    legend_node_t* lgd_ptr;

    if (display->side_win_type != SIDE_WIN_STATS)
        //not to be displayed
        return;

    if (dbg_flg) fprintf(dbg_fp, "STATS.\n");

    if (display->wlegend != NULL)
        delwin(display->wlegend);

    //creating window:
    display->wlegend = newwin(display->max_row - display->min_row - 1,
            display->lgdw, display->min_row,
            display->min_col);

    //printing the contents:
    mvwprintw(display->wlegend, 1, 1, "Statistics: ");
    index = 1;
    lgd_ptr = (display->legend).head;
    while (lgd_ptr != NULL) {
        avg = avg_by_item(display, lgd_ptr->data_type);
        if (avg >= 0) {
            mvwprintw(display->wlegend, index + 1, 1,
                    "%i) ", index);
            print_str(display->wlegend, &(pConfigurator->Colors._statusNodeName),
                    index + 1, 4, infoModulesArr[lgd_ptr->data_type]->md_title,
                    infoModulesArr[lgd_ptr->data_type]->md_titlength, 0);
            wprintw(display->wlegend, " AVG: %.2f", avg);

            index++;
        }
        lgd_ptr = lgd_ptr->next;
    }

    wborder(display->wlegend, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    mvwprintw(display->wlegend,
            display->max_row - display->min_row - 2, 1,
            " z - toggle, x - close");

    wrefresh(display->wlegend);
}


void displayDrawNumbering(mon_disp_prop_t* display, int draw)
//This function displayes bottom numbering for the graph, iff the draw
//int is 1 - otherwise only erases them
{
    int index, index2, offset;
    double temp_max; //holds a temporary maximum
    void* temp_p;
    char* title;

    //delete a single line of numbering
    move(display->max_row - display->bottom_spacing
            + display->show_cluster + 1,
            display->min_col + display->left_spacing - 1);
    for (offset = display->min_col + display->left_spacing;
            offset <= display->max_col; offset++)
        printw(" ");

    if ((display->displayed > 0) && (display->data_count > 0)) {
        //find the widest number in the list
        temp_max = get_max(display, dm_getIdByName("num"));

        //remove other line if in vertical mode
        if ((display->legend).legend_count < (int) temp_max + 1) {
            index2 = 1;
            while (index2 < (int) temp_max) {
                move(display->max_row - display->bottom_spacing
                        + display->show_cluster + 1 + index2,
                        display->min_col + display->left_spacing - 1);
                for (offset = display->min_col + display->left_spacing;
                        offset <= display->max_col; offset++)
                    printw(" ");
                index2++;
            }
        }


        if (draw) //if there is data displayed...
        {
            //move to position
            move(display->max_row - display->bottom_spacing
                    + display->show_cluster + 1,
                    display->min_col + display->left_spacing + 1);

            //find the widest number in the list
            temp_max = get_max(display, dm_getIdByName("num"));

            if ((display->legend).legend_count >= (int) temp_max + 1)
                //if all the numbers fit in horizontally...
            {
                offset = 0;
                c_on(&(pConfigurator->Colors._horizNodeName));
                for (index = 0; index < display->displayed / (display->legend).legend_count; index++) {
                    //points to the currently displayed node
                    temp_p = display->display_data[index * (display->legend).legend_count];

                    //store the length of the number
                    index2 = scalar_div_x(dm_getIdByName("num"), (void*) ((long) temp_p + get_pos(dm_getIdByName("num"))), 1);

                    //left space
                    for (offset = 0;
                            offset < (display->legend).legend_count - index2 -
                            ((display->legend).legend_count - index2) / 2;
                            offset++)
                        printw(" ");

                    offset += index2;

                    //the number itself
                    int nodeNum = *((int*) ((long) temp_p + get_pos(dm_getIdByName("num"))));
                    if (display->side_win_type == SIDE_WIN_NODE_INFO &&
                            nodeNum == display->selected_node)
                        attron(A_UNDERLINE);

                    printw("%i", nodeNum);

                    if (display->side_win_type == SIDE_WIN_NODE_INFO &&
                            nodeNum == display->selected_node)
                        attroff(A_UNDERLINE);

                    //right space
                    for (; offset < (display->legend).legend_count; offset++)
                        printw(" ");
                }
                c_off(&(pConfigurator->Colors._horizNodeName));
            } else
                //go to vertical mode!
            {
                //store the length of the number
                index2 = (int) temp_max;

                title = (char*) malloc((index2 + 1) * sizeof (char));

                for (index = 0; index < display->displayed / (display->legend).legend_count; index++) {
                    //points to the currently displayed node
                    temp_p = display->display_data[index * (display->legend).legend_count];

                    //left space
                    offset = (display->legend).legend_count - 1 -
                            ((display->legend).legend_count - 1) / 2;

                    int nodeNum = *((int*) ((long) temp_p + get_pos(dm_getIdByName("num"))));
                    sprintf(title, "%i", nodeNum);

                    display_rtr(stdscr, &(pConfigurator->Colors._vertNodeName),
                            display->max_row - display->bottom_spacing
                            + index2 + display->show_cluster, //row
                            index * (display->legend).legend_count + offset
                            + display->left_spacing + 1, //col
                            title, strlen(title), 0);
                }

                free(title);
            }
        }
    }
}

void displayDrawVerticalTitle(mon_disp_prop_t *display)
{
    legend_node_t* lgd_ptr; //for iterating the legend list
    lgd_ptr = (display->legend).head;
    
    char *title = (char*) (infoModulesArr[lgd_ptr->data_type]->md_title);
    display_rtr(stdscr, &(pConfigurator->Colors._chartColor),
            display->min_row + (display->max_row - display->min_row
            - display->bottom_spacing
            + infoModulesArr[lgd_ptr->data_type]->md_titlength) / 2,
            display->min_col + display->left_spacing - LSPACE,
            (char*) (infoModulesArr[lgd_ptr->data_type]->md_title),
            infoModulesArr[lgd_ptr->data_type]->md_titlength, 0);
}


void displayManageMax(mon_disp_prop_t *display)
{
    double max; //holds the max value
    int max_type; //holds type of max
    legend_node_t *lgd_ptr = (display->legend).head;
  
    // Manage max =====================================
    max = get_max(display, lgd_ptr->data_type);
    max_type = lgd_ptr->data_type;

    if ((max < 1.0) && ((max_type == dm_getIdByName("load")) || (max_type == dm_getIdByName("f+l"))))
        max = 1.0;

    // The time to change the max has come!
    if ((display->max_counter >= max_val_reset_timeout) ||
            (max > display->last_max) ||
            (display->last_max == -1) ||
            (display->last_max_type != max_type)) {
        display->last_max = max;
        display->last_max_type = max_type;
        display->max_counter = 0;
    } else {
        // Increment waiting time
        display->max_counter++;
    }
}


void displayDrawYAxisValues(mmon_display_t *display) 
{
    legend_node_t *lgd_ptr = (display->legend).head;

    // Displays Y axis valus (if scalar)
    if ((isScalar(lgd_ptr->data_type)) &&
            !(display->show_help)) {
        int width = display->max_row - display->min_row - display->bottom_spacing;
        c_on(&(pConfigurator->Colors._stringMB));
        mvprintw(display->min_row,
                display->left_spacing - LSPACE + 1,
                infoModulesArr[lgd_ptr->data_type]->md_format,
                display->last_max);
        mvprintw(display->min_row + width / 2,
                display->left_spacing - LSPACE + 1,
                infoModulesArr[lgd_ptr->data_type]->md_format,
                display->last_max / 2);
        mvprintw(display->min_row + width / 4,
                display->left_spacing - LSPACE + 1,
                infoModulesArr[lgd_ptr->data_type]->md_format,
                display->last_max * 3 / 4);
        mvprintw(display->min_row + (width * 3) / 4,
                display->left_spacing - LSPACE + 1,
                infoModulesArr[lgd_ptr->data_type]->md_format,
                display->last_max / 4);
        c_off(&(pConfigurator->Colors._stringMB));

        mlog_bn_db("disp", "MAX %f\n", display->last_max);
    }

}

void displayDrawBorder(mon_disp_prop_t *display)
{
    // If the first displayed item belongs to first machine in data
    if ((display->displayed == 0) ||
            ((long) (display->display_data[0]) - (long) display->raw_data < display->block_length)) {
        // If the last displayed item belongs to the last machine in data
        if ((display->displayed == 0) ||
                ((long) (display->display_data[display->displayed - 1]) - (long) display->raw_data
                > (display->data_count - 2) * display->block_length)) {
            wborder(display->graph, ACS_VLINE, ' ', ' ', ACS_HLINE, ACS_UARROW, ' ', ACS_LLCORNER, ' ');
        } else {
            wborder(display->graph, ACS_VLINE, ' ', ' ', ACS_HLINE, ACS_UARROW, ' ', ACS_LLCORNER, ACS_RARROW);
        }
    } else if ((long) (display->display_data[display->displayed - 1]) - (long) display->raw_data
            > (display->data_count - 2) * display->block_length)
        wborder(display->graph, ACS_VLINE, ' ', ' ', ACS_HLINE, ACS_UARROW, ' ', ACS_LARROW, ' ');
    else
        wborder(display->graph, ACS_VLINE, ' ', ' ', ACS_HLINE, ACS_UARROW, ' ', ACS_LARROW, ACS_RARROW);

}

// Redraws only the graph and max values
void displayRedrawGraph(mon_disp_prop_t* display)
{
    int index; //temp var for loops
    legend_node_t* lgd_ptr; //for iterating the legend list
    int max_type; //holds type of max


    if (dbg_flg) fprintf(dbg_fp, "Graph redrawn.\n");

    werase(display->graph);

    // Find the first non-space item in the legend (NULL if non-existant)
    lgd_ptr = (display->legend).head;
    while ((lgd_ptr != NULL) && (lgd_ptr->data_type == dm_getIdByName("space")))
        lgd_ptr = lgd_ptr->next;

    // Display bottom numbering - if needed
    displayDrawNumbering(display, (lgd_ptr != NULL));
    
    // Vertical Title
    if ((lgd_ptr != NULL) && (!(display->show_help))) {
        displayDrawVerticalTitle(display);
    }

    // Print the new (if needed):
    if ((lgd_ptr != NULL) && (display->displayed > 0) && (display->raw_data != NULL)) {
        
        displayManageMax(display);
        max_type = lgd_ptr->data_type;

        displayDrawYAxisValues(display);
        
        //=========================================================
        // ACTUAL GRPAHS: Displaying the bars on the graph by using the module functions
        // COULD BE IMPROVED (FINDING MAX)
        lgd_ptr = (display->legend).head;
        //if (dbg_flg) fprintf(dbg_fp, "Printing...\n");
        int width = display->max_row - display->min_row - display->bottom_spacing;
        for (index = 0; index < display->displayed; index++) {

            //if corresponding cell is alive...
            if (display->life_arr[get_raw_pos(display, index)]) {

                if (max_type == lgd_ptr->data_type)
                    display_item(lgd_ptr->data_type, //data type
                        display->graph, //target window
                        (void*) ((long) display->display_data[index] + get_pos(lgd_ptr->data_type)),
                        width - 1, //base row
                        0, index + 1, //col index
                        display->last_max, //max value
                        display->wmode); //width mode
                else
                    display_item(lgd_ptr->data_type, //data type
                        display->graph, //target window
                        (void*) ((long) display->display_data[index] + get_pos(lgd_ptr->data_type)),
                        width - 1, //base row
                        0, index + 1, //col index
                        get_max(display, lgd_ptr->data_type), //max value
                        display->wmode); //width mode
            }
                // The corisponding cell is dead
            else {
                if (lgd_ptr->data_type != dm_getIdByName("space") &&
                        lgd_ptr->data_type != dm_getIdByName("seperator")) {
                    display_dead(display->graph,
                            (void*) ((long) display->display_data[index] + get_pos(dm_getIdByName("infod-status"))),
                            width - 1,
                            0, index + 1);

                } else {
                    // The following line display the space or seperator even if the node is dead!!!
                    // This should be change to something more generic.
                    display_item(lgd_ptr->data_type, //data type 
                            display->graph, //target window
                            NULL,
                            width - 1, //base row
                            0, index + 1, //col index
                            get_max(display, lgd_ptr->data_type), //max value
                            display->wmode); //width mode

                }
            }
            lgd_ptr = lgd_ptr->next;
            if (lgd_ptr == NULL)
                lgd_ptr = (display->legend).head;
        }
    }

    //================================= Displaying the AXIS ============================
    // Displaying  graph axis
    if (*curr_display == display) {
        //only the chosen graph has colored axis
        wc_on(display->graph, &(pConfigurator->Colors._chartColor));
    }

    displayDrawBorder(display);
    
    
    // Only the chosen graph has colored axis
    if (*curr_display == display)
        wc_off(display->graph, &(pConfigurator->Colors._chartColor));

    //============================= From here below it is ok ============================
    
    // Show the cluster id 
    if (display->show_cluster)
        display_cluster_id(display, 1);

    // Show the dead node indicator
    if (display->show_dead)
        mvprintw(display->max_row - display->bottom_spacing + 1,
            display->min_col + display->left_spacing,
            "D");

}

void displayShowHelp(mon_disp_prop_t *display)
{
    //erase previous graph content
    werase(display->graph);
    //remove bottom numbering
    displayDrawNumbering(display, 0);
    //remove cluster line
    display_cluster_id(display, 0);
    show_manual(display);
}

void displayRedraw(mon_disp_prop_t* display)
//clears the screen and draws it all again
{
    int index, index2; //temp vars
    int dead_count, offset; //for handling the dead nodes
    legend_node_t* lgd_ptr; //for manipulating the legend list
    double temp;

    display->left_spacing = LSPACE + (display->wlegend != NULL) * (display->lgdw);
    display->bottom_spacing = display->show_cluster + display->show_status + 2;

    //check for existance of non-SPACE display type
    lgd_ptr = (display->legend).head;
    while ((lgd_ptr != NULL) && (lgd_ptr->data_type == dm_getIdByName("space")))
        lgd_ptr = lgd_ptr->next;

    //insert SPACEs to the legend in case of basic mode, and not yet padded
    if ((!display->wlegend) &&
            (display->recount) &&
            (lgd_ptr)) {
        //first we remove previous padding:
        while (((display->legend).head != NULL) &&
                ((display->legend).head->data_type == dm_getIdByName("space"))) {
            //remove first space in the legend
            lgd_ptr = (display->legend).head;
            (display->legend).head = (display->legend).head->next;
            free(lgd_ptr);
            (display->legend).legend_count--;
        }
        index = 0;
        while ((display->legend).legend_count > index) {
            //remember the count to track changes
            index = (display->legend).legend_count;
            //remove space from the end
            toggle_disp_type(display, dm_getIdByName("space"), 0);
        }

        //now we add current padding
        index2 = 0; //initially - no spaces required
        if (display->wmode > 1)
            index2 = 1;
        if (display->wmode == 3)
            index2 = get_max(display, dm_getIdByName("num"));
        for (index = 0; index < index2; index++) {
            if (index % 2 == 0) {
                //insert space
                lgd_ptr = (legend_node_t*) malloc(sizeof (legend_node_t));
                lgd_ptr->data_type = dm_getIdByName("space");
                lgd_ptr->next = (display->legend).head;
                (display->legend).head = lgd_ptr;
                (display->legend).legend_count++;
            } else
                //toggle space
                toggle_disp_type(display, dm_getIdByName("space"), 1);
        }
    }

    //redrawing the graph in case the screen size changed
    if ((display->last_cols != COLS) ||
            (display->last_lines != LINES) ||
            (display->recount)) {
        //updata screen size
        display->last_cols = COLS;
        display->last_lines = LINES;

        //if(!display->recount)
        display->lgdw = COLS / display->side_window_width_factor; //width of left legend window

        if (dbg_flg) fprintf(dbg_fp, "RESIZE! <%i, %i>\n", LINES, COLS);
        size_recalculate(1 - display->recount); //redraw only if the change is the size

        display->recount = 0; //reset need to recount the displayed

        //count the dead:
        dead_count = 0;
        for (index = 0; index < display->data_count; index++)
            dead_count += 1 - display->life_arr[index];

        //now, to init the display array:
        //(if the dead are hidden, less display space might be needed)

        //calculating display size : "displayed" amount...
        display->displayed = display->max_col - display->min_col - display->left_spacing - 2;
        if (display->displayed > (display->data_count - dead_count * (1 - display->show_dead))
                * (display->legend).legend_count)
            display->displayed = (display->data_count - dead_count * (1 - display->show_dead))
            * (display->legend).legend_count;

        if ((display->legend).legend_count > 0)
            display->displayed -= display->displayed % (display->legend).legend_count;

        //memory allocation:
        if (display->display_data != NULL)
            free(display->display_data);
        if (display->displayed > 0)
            display->display_data = malloc(display->displayed * sizeof (void*));
        else
            display->display_data = NULL;

        //fill the display array from the data array:
        index2 = 0;
        offset = 0;
        //index indicates the dispayed nodes, index2 indicates display types
        for (index = 0;
                index * (display->legend).legend_count < display->displayed;
                index++) {
            while ((index + offset < display->data_count) &&
                    (!((display->life_arr[index + offset]) || (display->show_dead))))
                offset++;

            if (index + offset < display->data_count)
                for (index2 = 0; index2 < (display->legend).legend_count; index2++)
                    //no fear of segmentation fault, because displayed is
                    //never larger then "data_count" of the data array.
                    display->display_data[index * (display->legend).legend_count + index2] =
                        (void*) ((long) display->raw_data + (index + offset) * display->block_length);
        }
    }

    if ((COLS < MIN_COLS) || (LINES < MIN_LINES))
        //if screen too small
    {
        erase(); //clear the screen
        mvprintw(0, 0, "The screen size is too small - please resize.");
        return;
    }

    //if Inforamtion isn't available, and it's not the help screen we are after - we stop.
    if ((display->NAcounter > 0) && (!display->show_help))
        return;

    //make space for bottom numbering - if insufficient
    if ((display->displayed > 0) && (display->data_count > 0)) {
        //find the widest number in the list
        temp = get_max(display, dm_getIdByName("num"));

        if ((display->legend).legend_count < (int) temp + 1)
            //if NOT all the numbers fit in horizontally:
            //*( -1 is for the line already accounted for...)
            display->bottom_spacing += (int) temp - 1;
    }

    //create the graph window
    if (display->graph) {
        werase(display->graph);
        delwin(display->graph);
    }
    display->graph =
            newwin(display->max_row - display->bottom_spacing - display->min_row + 1,
            display->max_col - display->left_spacing - display->min_col,
            display->min_row,
            display->min_col + display->left_spacing);

    //delete "leftover"s of left side values
    for (index = display->min_row;
            index < display->max_row - display->bottom_spacing;
            index++) {
        move(index, display->min_col + display->left_spacing - LSPACE);
        for (index2 = 0; index2 < LSPACE; index2++)
            printw(" ");
    }

    //now, if the "show_help" flag is 1, we display the help in the graph window.
    //otherwise - we display the graph as usual.
    if (display->show_help) 
        displayShowHelp(display);        
    else
        displayRedrawGraph(display); //everything related to the graph


    displayShowTitle(display);

    //redraw tooltips (check inside)
    show_legend_side_win(display);
    show_statistics_side_win(display);
    show_node_info_side_win(display);

    refresh();
    wrefresh(display->graph);
}
