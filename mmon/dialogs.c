#include <curses.h>
#include "mmon.h"
#include "drawHelper.h"

// Choose a new selected node
static char lable[] = "\n  Choose a node to view: ";
void selected_node_dialog(mon_disp_prop_t* display, Configurator *config) {
  
     if(display->side_win_type != SIDE_WIN_NODE_INFO)
          return;

     int selectedNode = 0; 
     
     display->dest = newwin(3, 
                            display->max_col - display->min_col - display->left_spacing - 4,
                            display->min_row + (display->max_row - display->min_row) / 2,
                            display->min_col + display->left_spacing + 2);
     wc_on(display->dest, &(config->Colors._chartColor));
     
     wprintw(display->dest, lable);
     wborder(display->dest, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
             ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
     wrefresh(display->dest);

     // Read selected node from user
     // TODO : check that the value is leagal and there is such a node
    do {
        echo();
        mvwscanw(display->dest, 1, strlen(lable) + 3, "%d", &selectedNode);
        noecho();
        if (dbg_flg) fprintf(dbg_fp, "NEW: %d\n", selectedNode);
        if(selectedNode < 1) selectedNode = 1;

    } while (0);
    
    display->selected_node = selectedNode;
    delwin(display->dest); //delete the window

}
