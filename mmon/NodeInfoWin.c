/*
 * Displaying a side window on the currently selected node
 */

#include "mmon.h"

void toggleSideWindowMD(mon_disp_prop_t *display) {

    if (display->side_win_type != SIDE_WIN_NODE_INFO) return;

    mdPtr arr[100];
    int sideMD = getMDWithSideWindow(arr);
    if (sideMD <= 0) {
        mlog_bn_dy("side", "No MD with side windows\n", sideMD);
        display->side_win_md_ptr = NULL;
        display->side_win_md_index = -1;
        return;
    }
    mlog_bn_dy("side", "SideMD num: %d\n", sideMD);
    if(display->side_win_md_index > -1) {
        display->side_win_md_index = (display->side_win_md_index + 1) % sideMD; 
    }
    else {
        display->side_win_md_index = 0;
    }
    display->side_win_md_ptr = arr[display->side_win_md_index];
    mlog_bn_dy("side", "Selected SideMD [%s]\n", display->side_win_md_ptr->md_name);
}

void printSideWindowInfo(mon_disp_prop_t* display, int index);


void show_node_info_side_win(mon_disp_prop_t* display)
//draws the statistics on the left (in wlegend)

{
     int index;
     double avg;
     legend_node_t* lgd_ptr;

     if (display->side_win_type != SIDE_WIN_NODE_INFO) 
          //not to be displayed
          return;

     mlog_bn_info("side", "displaying side window [w=%d]\n", display->lgdw);
          
     if (display->wlegend != NULL) {
          delwin(display->wlegend);
     }
     if(display->wlegendBorder != NULL) {
	  delwin(display->wlegendBorder);
     }
     
     // Border window
     display->wlegendBorder = newwin(display->max_row - display->min_row - 1,
				     display->lgdw, 
				     display->min_row,
				     display->min_col);
     // Window
     display->wlegend = newwin(display->max_row - display->min_row - 1 - 2,
                               display->lgdw-2, 
			       display->min_row+1,
                               display->min_col+1);
     
     //printing the contents:
     mvwprintw(display->wlegend, 0, 0, "Node [%d] Info:", display->selected_node);

     
     // Looking for the selected node pe in our data.
     // This is done here since the position in data might change
     if(display->selected_node != -1)
        index = displayGetSelecteNodeDataIndex(display);
     else {
         index = displayGetFirstNodeInViewDataIndex(display);
         display->selected_node = displayGetFirstNodeInViewNumber(display);
     }
     
     // We have a valid index we can print data
     if(index != -1) {
          mlog_bn_db("side", "Index = %d\n", index);
          printSideWindowInfo(display, index);
     }

     wborder(display->wlegendBorder, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
             ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
     mvwprintw(display->wlegendBorder,
               display->max_row - display->min_row - 2, 1,
               " z - toggle, x - close");
     
     wrefresh(display->wlegendBorder);
     wrefresh(display->wlegend);

}


static char glob_buff[8192];
void printSideWindowInfo(mon_disp_prop_t* display, int index) {

     int  size;
     int line = 1;

     void *nodeDataPtr = display->raw_data + display->block_length * index;
     int pos = get_pos(dm_getIdByName("name"));
     mlog_bn_db("side", "Pos: [%d]\n", pos);

     if(!display->life_arr[index]) {
          mvwprintw(display->wlegend, line++, 0, "Node is inactive ");
          return;
     }
   

     char *nodeName = ((char*) ((long) nodeDataPtr + pos));
     mlog_bn_db("side", "Name: [%s]\n", nodeName);
     
     mvwprintw(display->wlegend, line++, 0, "Name  : %s", nodeName);
     
     size = 64;
     get_text_info_byName("uptime", nodeDataPtr, glob_buff, &size, 10);
     mvwprintw(display->wlegend, line++, 0, "Uptime: %s", glob_buff);

     size = 64;
     get_text_info_byName("speed", nodeDataPtr, glob_buff, &size, 10);
     mvwprintw(display->wlegend, line++, 0, "Speed : %s", glob_buff);

     size = 64;
     get_text_info_byName("ncpus", nodeDataPtr, glob_buff, &size, 10);
     mvwprintw(display->wlegend, line++, 0, "CPU's : %s", glob_buff);

     size = 64;
     get_text_info_byName("mem", nodeDataPtr, glob_buff, &size, 10);
     mvwprintw(display->wlegend, line++, 0, "Mem   : %s", glob_buff);

     size = 64;
     get_text_info_byName("swap", nodeDataPtr, glob_buff, &size, 10);
     mvwprintw(display->wlegend, line++, 0, "Swap  : %s", glob_buff);

     size = 64;
     get_text_info_byName("load", nodeDataPtr, glob_buff, &size, 10);
     mvwprintw(display->wlegend, line++, 0, "Load  : %s", glob_buff);

     size = 64;
     get_text_info_byName("iowait", nodeDataPtr, glob_buff, &size, 10);
     mvwprintw(display->wlegend, line++, 0, "IOWait: %s", glob_buff);

     
     size = 64;
     line++;
     
     // We take the selected module with side window
     if (display->side_win_md_ptr) {
        if (get_text_info_byName(display->side_win_md_ptr->md_name, nodeDataPtr, glob_buff, &size, 10) >= 0) {
            mlog_bn_info("side", "%s buff[%s]\n", display->side_win_md_ptr->md_name, glob_buff);
            mvwprintw(display->wlegend, line++, 0, "%s:\n%s", display->side_win_md_ptr->md_title, glob_buff);
        }
     }
}
