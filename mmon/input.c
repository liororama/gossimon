/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include "mmon.h"
#include "DisplayModules.h"
#include "display.h"

char esc_no_wait;
int un_char, las_char;

void displayMsg(mmon_display_t *display, char *msg);

void ch_alarm() {
    signal(SIGALRM, (sig_t) ch_alarm);
}

void not_yet_ch(int ch) {
    if (ch)
        un_char = ch;
    else if (las_char && las_char != ERR)
        un_char = las_char;
}

int readc_half_second()
//This function sets the maximal waiting time
//for the user input
{
    char r = ERR;
    int a;
    struct itimerval t, tt;

    if (ioctl(0, FIONREAD, &a) >= 0 && a > 0)
        read(0, &r, 1);
    else if (esc_no_wait)
        return (ERR);
    else {
        signal(SIGALRM, (sig_t) ch_alarm);
        t.it_interval.tv_sec = 0;
        t.it_interval.tv_usec = 500000;
        t.it_value = t.it_interval;
        setitimer(ITIMER_REAL, &t, &tt);
        read(0, &r, 1);
        t.it_interval.tv_usec = t.it_interval.tv_sec = 0;
        t.it_value = t.it_interval;
        setitimer(ITIMER_REAL, &t, &tt);
    }
    return (r);
}

int my_getch() {
    char r = ERR;

    if (un_char && un_char != ERR) {
        las_char = un_char;
        un_char = 0;
        return (las_char);
    }

    read(0, &r, 1);
    if (r == '\33') {
        r = readc_half_second();
        if (r == ERR)
            return (las_char = '\33');
        if (r == '[' || r == 'O') {
            switch (r = readc_half_second()) {
                case 'A':
                    return (las_char = KEY_UP);
                case 'B':
                    return (las_char = KEY_DOWN);
                case 'C':
                    return (las_char = KEY_RIGHT);
                case 'D':
                    return (las_char = KEY_LEFT);
                case 'M':
                    return (las_char = KEY_ENTER);
                case 'q':
                case 'F':
                    return (las_char = KEY_C1);
                case 'r':
                    return (las_char = KEY_DOWN);
                case 's':
                case 'G':
                    return (las_char = KEY_C3);
                case 't':
                    return (las_char = KEY_LEFT);
                case 'v':
                    return (las_char = KEY_RIGHT);
                case 'w':
                case 'H':
                    return (las_char = KEY_A1);
                case 'x':
                    return (las_char = KEY_UP);
                case 'y':
                case 'I':
                    return (las_char = KEY_A3);
                case '5':
                case '6':
                    if (readc_half_second() == '~')
                        return (las_char = (r == '5' ? KEY_A3 : KEY_C3));
                    /* fall thru */
                default:
                    return (las_char = r);
            }
        } else
            return (las_char = r);
    } else
        return (las_char = r);
}

int is_input() {
    int r;

    return ((un_char && un_char != ERR) || (ioctl(0, FIONREAD, &r) >= 0 && r > 0));
}

void parse_cmd(mmon_data_t *md, int pressed)
//The command parser, responsible for handling the user input
//There are two editing modes, effecting the way input is handled.
//
//The basic mode - every display type activated is the only one displayed
//on the axis, besides SPACEs - depending on the view mode:
// - "wide" : minimal amount of spacing to allow horizontal number display
// - "vertical" : one space between each two bars
// - "super vertical" : no spacing at all.
//
//The expert mode - the display legend is customized manually.
//
//The expert mode is on iff legend side-panel appears on the screen. 
{
    int index; //an integer for loops...
    int offset; //for handling the dead nodes (and other uses)
    mon_disp_prop_t* display = *curr_display;
    //int pressed; //save the pressed key for reference in key_map
    legend_node_t* lgd_ptr; //to iterate the legend

    switch (pressed) {
        case 13: //Enter - redraws the screen and sets next_key to 1
            display->recount = 1;

            //erase all screen content
            clear();
            refresh();

            displayRedraw(display);
            break;

        case KEY_UP: //moves to previous cluster or the help string 
            if (display->wlegend) {
                if (((display->legend).curr_ptr !=
                        (display->legend).head) &&
                        ((display->legend).head != NULL)) {
                    lgd_ptr = (display->legend).head;
                    while (lgd_ptr->next != (display->legend).curr_ptr)
                        lgd_ptr = lgd_ptr->next;
                    (display->legend).curr_ptr = lgd_ptr;
                }
            } else {
                if (display->show_help) {
                    if (display->help_start > 0)
                        display->help_start--;
                    displayRedraw(display);
                }
                else {
                    //use new host:
                    free(display->displayed_bars_data);
                    display->displayed_bars_data = NULL;
                    display->displayed_bar_num = 0;

                    if (display->nodes_count > 0) {
                        displayFreeData(display);
                        free(display->alive_arr);
                    }

                    display->cluster = cl_prev(display->clist);
                    init_clusters_list(display);

                    display->recount = 1; //changes to apply
                    get_nodes_to_display(display);
                }
            }
            break;

        case KEY_DOWN: //moves to next cluster or the help string
            if (display->wlegend) {
                if ((display->legend).curr_ptr != NULL)
                    (display->legend).curr_ptr =
                        (display->legend).curr_ptr->next;
            } else {
                if (display->show_help) {
                    //length check
                    offset = get_str_length(head_str) + get_str_length(help_str) -
                            display->max_row + display->min_row + display->bottom_spacing;
                    index = 0; //starting to go through display types
                    for (index = 0; index < infoDisplayModuleNum; index++)
                        offset += get_str_length(display_help(index));
                    if (offset > display->help_start) {
                        display->help_start++;
                        displayRedraw(display);
                    }
                }
                else {
                    //use new host:
                    free(display->displayed_bars_data);
                    display->displayed_bars_data = NULL;
                    display->displayed_bar_num = 0;

                    if (display->nodes_count > 0) {
                        displayFreeData(display);
                        free(display->alive_arr);
                    }

                    display->cluster = cl_next(display->clist);
                    init_clusters_list(display);

                    display->recount = 1; //changes to apply
                    get_nodes_to_display(display);
                }
            }
            break;

        case KEY_LEFT: //move left
            if (display->displayed_bar_num == 0)
                break;
            move_left(display);
            break;

        case KEY_RIGHT: //move right
            if (display->displayed_bar_num == 0)
                break;
            move_right(display);
            break;

        case 349: //PageUp - moves for a whole display
            if (display->show_help) //in help menu
            {
                display->help_start -= display->max_row
                        - display->min_row
                        - display->bottom_spacing;
                displayRedraw(display);
            }
            else //in graph display
            {
                for (index = 0;
                        index < display->displayed_bar_num / (display->legend).legend_size;
                        index++)
                    move_left(display);
            }
            break;

        case 352: //PageDown - moves for a whole display
            if (display->show_help) //in help menu
            {
                display->help_start += display->max_row
                        - display->min_row
                        - display->bottom_spacing;
                displayRedraw(display);
            }
            else //in graph display
            {
                for (index = 0;
                        index < display->displayed_bar_num / (display->legend).legend_size;
                        index++)
                    move_right(display);
            }
            break;

        case 127: //backspace - removes last display type
            if (display->wlegend) //if expert mode on
            {
                lgd_ptr = (display->legend).head;
                if (lgd_ptr != NULL) {
                    while (lgd_ptr->next != NULL)
                        lgd_ptr = lgd_ptr->next;
                    toggle_disp_type(display, lgd_ptr->data_type, 0);
                    get_nodes_to_display(display);
                }
            }
            break;

        // Toggle text display in the side window. Toggle all modules which has text info 
        // method.
        case 'S':
            saveCurrentWindows(md);
            break;

        case 'h':
            //toggle help panel
            if ((display->NAcounter == 0) || (!display->show_help))
                display->show_help = 1 - display->show_help;
            displayRedraw(display);
            break;

        case 'n':
            new_host(display, NULL); //opens a new host dialogue
            break;

        case 'v':
            //toggle width mode
            display->wmode++; //toggle
            if (display->wmode == 4)
                display->wmode = 1;
            display->recount = 1;
            displayRedraw(display);
            break;

            
        case 'a':
            toggleSideWindowMD(display);
            break;
            
        case 'z':
        {
            // toggle side window
            SideWindowType_T prevType = display->side_win_type;
            display->side_win_type = (display->side_win_type + 1) % SIDE_WIN_MAX;
            if (display->side_win_type == SIDE_WIN_NONE) {
                displayDelSideWin(display);
            } else if (prevType == SIDE_WIN_NONE) {
                displayNewSideWin(display);
            }
            
            // In case we do not have a MD to display in the side window we look for onw
            if (display->side_win_type == SIDE_WIN_NODE_INFO &&
                display->side_win_md_index == -1)
                toggleSideWindowMD(display);
            
            display->recount = 1; //so that the displayed will be recalculate            
            displayRedraw(display);
        }
        break;

        case 'x':
            display->side_win_type = SIDE_WIN_NONE;
            displayDelSideWin(display);
            display->recount = 1; //so that the displayed will be recalculate            
            displayRedraw(display);
            break;

            // Decreasing the selected node number
        case '[':
            displayIncSelectedNode(display, -1);
            break;
            // Increasing the selected node number
        case ']':
            displayIncSelectedNode(display, 1);
            break;

        case 'Z':
            displayDecSideWindowWidth(display, 1);
            break;
        case 'X':
            displayIncSideWindowWidth(display, 1);
            break;
        case 'w':
            //w - add a new screen
            //first - check if addition is available
            index = 0;
            while ((index < MAX_SPLIT_SCREENS) &&
                    (glob_displaysArr[index] != NULL))
                index++;
            if (index == MAX_SPLIT_SCREENS)
                break;

            glob_displaysArr[index] =
                    (mon_disp_prop_t*) malloc(sizeof (mon_disp_prop_t));
            displayInit(glob_displaysArr[index]);
            curr_display = &(glob_displaysArr[index]);
            break;

        case 'e':
            //e - delete the current screen
            if (glob_displaysArr[1] != NULL)
                terminate();
            break;

        case 9: //TAB - cycle through screens
            if (curr_display == &(glob_displaysArr[MAX_SPLIT_SCREENS - 1]))
                curr_display = &(glob_displaysArr[0]);
            else
                curr_display++;
            if (*curr_display == NULL)
                curr_display = &(glob_displaysArr[0]);
            break;

        case 't': //toggle bottom status bar
            display->show_status = 1 - display->show_status;
            if (dbg_flg) fprintf(dbg_fp, "SHOW STATUS : %i\n", display->show_status);
            displayRedraw(display);
            break;

        case 'd': //toggle dead 
            display->show_dead = 1 - display->show_dead;
            display->recount = 1; //so that the display will recalculate
            if (dbg_flg) fprintf(dbg_fp, "SHOW DEAD : %i\n", display->show_dead);
            displayRedraw(display);
            break;

        case 'c': //toggle cluster id line 
            display->show_cluster = 1 - display->show_cluster;
            display->recount = 1; //so that the display will recalculate
            if (dbg_flg) fprintf(dbg_fp, "SHOW CLUSTER ID : %i\n", display->show_dead);
            displayRedraw(display);
            break;

        case 'y':
            new_yardstick(display); //opens a new yardstick dialogue
            break;

        case 'U':
            new_user(display); //opens a new username dialogue
            break;

        case 'C':
            selected_node_dialog(display, pConfigurator);
            break;
        case 'Q': // Exit mmon without delay
            mmon_exit(0);
            break;

        case 'q': // Display a confirmation message before exiting 
            if (glob_exiting > 0)
                mmon_exit(0);
            // Setting the timeout for the exit message
            glob_exiting = EXIT_TIMEOUT; //setting exit monitor "off"
            break;

        case 32:
            if (!display->wlegend) //if NOT in expert mode
                break;

            //DISPLAY TYPES:
            //if the input isn't one of the mmon control keys - 
            //we search the key_map for a binding for the pressed key.
        default:
            if (dbg_flg) fprintf(dbg_fp, "Pressed key : %i\n", pressed);
            if ((pressed <= max_key) && //in array range
                    (pressed >= 0) &&
                    (*(key_map[pressed]) != -1)) {
                index = 0;
                if (!display->wlegend) {//if in basic mode
                    while ((display->legend).legend_size > 0) //display types to erase
                    {
                        lgd_ptr = (display->legend).head;
                        while (lgd_ptr->next != NULL)
                            lgd_ptr = lgd_ptr->next;
                        index = index || (lgd_ptr->data_type == *(key_map[pressed]));
                        toggle_disp_type(display, lgd_ptr->data_type, 0);
                        //remove last item
                    }
                }
                if ((index) || (display->wlegend)) {
                    //if the chosen (by the new key) item is one of the erased,
                    //or expert mode is engaged

                    //we cycle the int array of the current key binding (1,2,3->2,3,1)
                    offset = *(key_map[pressed]); //remember last display type
                    index = 0;
                    while (*(key_map[pressed] + index + 1) != -1) {
                        *(key_map[pressed] + index) =
                                *(key_map[pressed] + index + 1);
                        index++;
                    }
                    *(key_map[pressed] + index) = offset;
                }

                //add desired item
                if ((display->legend).curr_ptr != NULL)
                    (display->legend).curr_ptr->data_type = *(key_map[pressed]);
                else
                    toggle_disp_type(display, *(key_map[pressed]), 1);

                index = get_raw_pos(display, 0);
                get_nodes_to_display(display);

                //move to previous position:
                for (; index > 0; index--)
                    move_right(display);
            }
    }
    return;
}

struct sigaction act;

void onio() {
    act.sa_handler = (sig_t) onio;
    sigaction(SIGALRM, &act, 0);
    alarm(1);
}

void sleep_or_input(int secs) {
    if (is_input())
        return;
    act.sa_handler = (sig_t) onio;
    sigaction(SIGALRM, &act, 0);
    alarm(secs);
    if (my_getch() != ERR)
        not_yet_ch(0);
    alarm(0);
}



