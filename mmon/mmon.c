/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

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
 *****************************************************************************/


static const char* MMON_VERSION = "2a"; //The current version

#include "mmon.h"
#include "drawHelper.h"
#include "DisplayModules.h"

// Color Configuration file settings
static  const char *defConfFileName = MMON_DEF_CONFIG_FILE; //default file name
static char *homeDir = NULL; //optional string for working directory
static char* const defConfDir = GOSSIMON_CONFIG_DIR; //default directory
Configurator* pConfigurator = NULL; //for color configuration structure



// Exit monitor:
// the q button doesn't exit the program right away, but rather alows the user
// to abort the exit request. To confirm the exit you must push q twice.
// This int is a counter for the exit message to last on the status line.
int glob_exiting = 0;

// The mapping structure. It is initialized when description is read from infod
// for the first time
mmon_info_mapping_t glob_info_map;

// Mapping variables
char *glob_info_desc = NULL;
variable_map_t *glob_info_var_mapping = NULL;

//Quick reference maps:
 mon_display_module_t** infoModulesArr; 
 

int* pos_map; //holds the relative position of the item in a block.
int** key_map; //holds key bindings of all display types: each cell is an array
//of all the display types binded to the key of the value of that cells
//address. every such array is "truncated" by -1.
int max_key; //holds the maximal key allocated (in key_map) 

settings_t current_set; //current settings fed to the new data.

// Debugging mon, since we use the screen we print debug info to another file
// Which of course can be /dev/pts/xx so we can print to another terminal
int dbg_flg;
int dbg_flg2;
FILE *dbg_fp;

//The "mon_displays" is the database containing every relevant attribute of the
//display types, where every such display type has an entry. even though most
//of the runtime actions are done via other structures - this is the source of
//all reference to display types, throughout this file.
//
//Each entry has the following format (see struct mon_display_module_t for more info):
// { <display type name - from enumerator in mmon.h>,
//   <scalar boolean - 1 iff type should be treated as scalar>,
//   <reverse boolean - relevant to reverse algorithem>,
//   <name string>,  
//   <name string length>, 
//   <format for display of typical values (souldn't be longer then LSPACE)>, 
//   <key bindings>*,
//   <function returning an objects required length (in bytes)>, 
//   <function returning a double value of an addressed object>**,
//   <function creating a new object using data from addressed source>,
//   <function deleting an adressed instance>,
//   <function displaying an addressed object in the target screen>***,
//   <name string>  
// }
//  
// * Some keys are reserved for mmon control, and for these keys - the display
//   types bound to them will be ignored. 
//   Such are : 'w', 'e', 'n', 't', 'd', 'z', 'x', 'y', 'q', 'Q',
//   and also the space bar, tab, backspace, PageUp, PageDown and enter.
// ** Will only be used if scalar boolean is asserted.
// *** The display function can use some predefined functions, to make it
//     simpler. here is a partial list (can be edited in the future):
//
//   print_str - outputs a string of given length in (row, col)
//
//   display_str - outputs a vertical form of the given string.
//
//   display_rtr - outputs a vertical form of the given string - reversed.
//
//   display_bar - draws a "total"-sized bar of "usedChar"s in target window. 
//
//   display_part_bar - draws 2 bars - both max and actual values.
//
// - for more information, see each function's description.
//


//Help string display:
//The mmon discription is expected to exeed the length allocated to display it.
//This is why, upon demand, the help str is displayed, starting from the line
//specified in the help_start int, inited to 0 at the start of the program.
//The structure of the help string is such that the head comes first,
//then a compiled list of module descriptions, and last is the help str itself.
//Note: The first string and the first char of each string are empty, to make
//      space for the border of the window. 

//DISPLAYS (for split screen mode)
mon_disp_prop_t** glob_displaysArr; //array of screens on display.
mon_disp_prop_t** glob_curr_display = NULL; //pointer to selected screen.


void init(mmon_data_t *md, int argc, char** argv);
//init mmon and create the first active window 



int get_infod_description(mon_disp_prop_t* display, int forceReload);
//Getting infod description and parsing it creating the mapping structure

void display_totals(mon_disp_prop_t* display, int draw);
//displays the bottom status line

//updates all on display:
//retrieves new data and posts it on the display screen

//Color attribute related:
// Free mmon global memory
void mmon_free();
// Exit mmon
int mmon_redraw(mmon_display_t *display);


int setStartWindows(mmon_data_t *md);
int loadCurrentWindows(mmon_data_t *md);


/****************************************************************************
 *                         
 *                        CLUSTER LIST MANEGEMENT
 *
 ***************************************************************************/

//DECLERATIONS:

//Environment variables to take the clusters from. If we are in multi cluster
//mode and we didnt get a clusters list from cmdline we check the envs
//First the clusters_list than the clusters file
char *clusters_list_env = "MOSIX_CLUSTERS_LIST";
char *clusters_file_env = "MOSIX_CLUSTERS_FILE";

//Current mosix cluster
char *mosix_cluster = NULL;

char localhost_str[] = "localhost"; //localhost string to use as a source
mon_hosts_t *mon_hosts; //a structure to hold single or multiple hosts 
int attempt = 0; //1 iff access to host attempted.
unsigned short glob_host_port = MSX_INFOD_DEF_PORT; //defines connection port

//FUNCTIONS

void init_clusters_list(mon_disp_prop_t* display)
//here, after the optional host list is compiled, 
//it's put to the test of a connection.
//if there is no compiled list by the time this function is invoked
//(might happen in case of invalid parameters) - we try to use
//the localhost (our machine) to compile an alternative.
{
    cluster_entry_t *ce;
    attempt = 1; //attempt made

    // We got the -h option so we use it as a single cluster
    // or if there was no -h and there was no multy cluster option -c -f
    if (!(display->mosix_host) &&
            (cl_size(display->clist) == 0))
        //if no valid compiled list available:
    {
        display->mosix_host = localhost_str;
        display->need_dest =
                !(cl_set_single_cluster(display->clist, display->mosix_host));
        display->mosix_host = NULL;
    }

    if ((!display->need_dest) &&
            (cl_size(display->clist) > 0))
        //if there is now an available host list:
    {
        attempt = 0; //successful attempt
        ce = cl_curr(display->clist);
        mosix_cluster = ce->name;
        mon_hosts = &(ce->host_list);
        display->mosix_host = mh_current(mon_hosts);

        if (dbg_flg)
            fprintf(dbg_fp, "Host = %s \n", display->mosix_host);

        display->need_dest = 0; //mark host as aquired
        display->cluster = cl_curr(display->clist);

        get_infod_description(display, 1); //retrieve the data description
    }
}

void new_host(mon_disp_prop_t* display, char* request)
//Opens a new cluster list dialogue
//If a request was forwarded - it's processed as the aquired host.
{
    int found = 0; //to determine when to accept input as correct
    //(while the inmput is incorrect the request the data over and over again)

    //a string for the input of the new destination host:
    char* new_host_str;
    if (request)
        new_host_str = request;
    else {
        new_host_str = (char*) calloc(MAX_HOST_NAME, sizeof (char));
        new_host_str[MAX_HOST_NAME - 1] = '\0';
    }

    cl_init(display->clist); //init the future host cluster list

    while (!(found))
        //iterations occur when incorrect input is presented
    {
        found = 1; //hope that input is correct...

        if (!request) {
            //open the dialogue window:
            display->dest =
                    newwin(4,
                    display->max_col - display->min_col - display->left_spacing - 4,
                    display->min_row + (display->max_row - display->min_row) / 2,
                    display->min_col + display->left_spacing + 2);

            //present demand for new destination:
            wc_on(display->dest, &(pConfigurator->Colors._chartColor));
            wborder(display->dest, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
                    ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
            wmove(display->dest, 1, 2);
            if (attempt)
                wprintw(display->dest, "No valid host found. ");
            if (display->last_host)
                wprintw(display->dest, "Last Host : %s", display->last_host);
            else
                wprintw(display->dest, "Last Host : none");
            mvwprintw(display->dest, 2, 2, "State new host list: ");
            wrefresh(display->dest);

            //reading the new destination cluster
            echo();
            wscanw(display->dest, "%s", new_host_str);
            noecho();

            if (dbg_flg) fprintf(dbg_fp, "New hosts: %s \n", new_host_str);
            //if the input is empty - use the last known good host
            if ((strlen(new_host_str) == 0) && (display->last_host))
                strncpy(new_host_str, display->last_host,
                    MAX_HOST_NAME * sizeof (char));

            wc_off(display->dest, &(pConfigurator->Colors._chartColor));
            werase(display->dest);
        }

        if ((new_host_str) && //string exists
                (!(cl_set_from_str(display->clist, new_host_str))) && //not a list
                (!(cl_set_from_file(display->clist, new_host_str))) && //not a file

                ((getenv(clusters_list_env) == 0) ||
                (new_host_str = getenv(clusters_list_env)) ||
                (!(cl_set_from_str(display->clist, new_host_str)))) &&

                ((getenv(clusters_file_env) == 0) ||
                (new_host_str = getenv(clusters_file_env)) ||
                (!(cl_set_from_file(display->clist, new_host_str))))) {
            if (dbg_flg)
                fprintf(dbg_fp, "Error in cluster string\n");
            found = 0;
        }

        if (cl_size(display->clist) == 0) {
            if (dbg_flg)
                fprintf(dbg_fp, "Invalid (empty) cluster list\n");
            found = 0;
        }

        delwin(display->dest);
        attempt = 1;
    }

    if (display->last_host)
        free(display->last_host);
    display->last_host = new_host_str;
    delwin(display->dest);

    //now, to free all alocated memory for the new data:
    if (display->displayed_bars_data) {
        free(display->displayed_bars_data);
        display->displayed_bars_data = NULL;
    }
    display->displayed_bar_num = 0;

    if (display->raw_data) {
        if (display->nodes_count > 0)
            displayFreeData(display);
    }


    display->nodes_count = 0;
    display->need_dest = 0;

    init_clusters_list(display); //set new hosts

    //to apply the changes:
    display->recount = 1;
    mmon_redraw(display);
}

void new_user(mon_disp_prop_t* display)
//Opens a new user dialog
{
    struct passwd *pw;

    //a string for the input of the new destination host:
    char* new_user_str = (char*) calloc(MAX_HOST_NAME, sizeof (char));
    new_user_str[MAX_HOST_NAME - 1] = '\0';

    //init the string to the last username in use
    pw = getpwuid(current_set.uid);
    if (pw)
        new_user_str = strcpy(new_user_str, pw->pw_name);

    //open the dialogue window:
    display->dest =
            newwin(4,
            display->max_col - display->min_col - display->left_spacing - 4,
            display->min_row + (display->max_row - display->min_row) / 2,
            display->min_col + display->left_spacing + 2);

    //present demand for new destination:
    wc_on(display->dest, &(pConfigurator->Colors._chartColor));
    wborder(display->dest, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    wmove(display->dest, 1, 2);
    if (new_user_str)
        wprintw(display->dest, "Last username : %s", new_user_str);
    else
        wprintw(display->dest, "Last username : Unknown");
    mvwprintw(display->dest, 2, 2, "State new username (Use \"nobody\" to cancel option): ");
    wrefresh(display->dest);

    //reading the new username
    echo();
    wscanw(display->dest, "%s", new_user_str);
    noecho();

    if (dbg_flg) fprintf(dbg_fp, "New username: %s \n", new_user_str);
    //if the input isn't empty - process...
    if (strlen(new_user_str) > 0) {
        pw = getpwnam(new_user_str);
        if (pw)
            current_set.uid = pw->pw_uid;
    }

    //close window
    wc_off(display->dest, &(pConfigurator->Colors._chartColor));
    werase(display->dest);
    delwin(display->dest);
    display->dest = NULL;

    //free string
    if (new_user_str)
        free(new_user_str);

    //to apply the changes:
    display->recount = 1;
    mmon_redraw(display);
}


void new_yardstick(mon_disp_prop_t* display)
//Opens a new yardstick dialogue:
//First you choose one of the three options (1-3):
//1. new value to be set as the yardstick speed
//2. use the fastest computer as the yardsick
//3. to set it to the default value.
{
    int new_speed = 0; //an int to hold the choice of options

    //presenting the window:
    display->dest =
            newwin(7, display->max_col - display->min_col - display->left_spacing - 4,
            display->min_row + (display->max_row - display->min_row) / 2,
            display->min_col + display->left_spacing + 2);
    wc_on(display->dest, &(pConfigurator->Colors._chartColor));
    wprintw(display->dest, "\n Choose how to set the new yardstick speed : \n");
    wprintw(display->dest, " Current yardstick speed is %.2f.\n", display->sspeed);
    wprintw(display->dest, " 1. Input new value \n");
    wprintw(display->dest, " 2. Match fastest machine in cluster \n");
    wprintw(display->dest, " 3. Set to mosix standard speed (default)");
    wborder(display->dest, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    wrefresh(display->dest);

    do {
        //request choice of options:
        echo();
        mvwscanw(display->dest, 1, 45, "%d", &new_speed);
        noecho();
        if (dbg_flg) fprintf(dbg_fp, "NEW: %d\n", new_speed);

        switch (new_speed)
            //act according to choice:
        {
        case 1:
            mvwprintw(display->dest, 3, 21, ": ");
            echo();
            wscanw(display->dest, "%f", &(display->sspeed));
            noecho();
            break;
        case 2:
            display->sspeed = get_max(display, dm_getIdByName("speed"));
            break;
        case 3:
            display->sspeed = MOSIX_STD_SPEED;
            break;
        }
    } while ((1 > new_speed) || (new_speed > 3));
    //iterated while choice input is invalid (not 1-3)

    delwin(display->dest); //delete the window
    mmon_redraw(display); //apply changes
}

/****************************************************************************
 *                         
 *                        DATA ARRAY MANAGEMENT
 * 
 ***************************************************************************/

int get_pos(int item)
//Returns the relative position in the data block of the array,
//using the pos_map array, holding the values for quick reference.
{
    return pos_map[item];
}

int get_raw_pos(mon_disp_prop_t* display, int index)
//Returns the relative position of the indexed
//item of the display array, in the raw data array
{
    return (((long) (display->displayed_bars_data[index]) - (long) display->raw_data)
            / display->block_length);
}

int canRev(int item)
//The function returns 1 iff the reverse boolean is asserted
{
    return infoModulesArr[item]->md_canrev;
}

int isScalar(int item)
//The function returns 1 iff item is scalar (scalar bool. asserted)
{
    return infoModulesArr[item]->md_iscalar;
}

int get_length(int item)
//Returns the required length for a parameter
{
    return infoModulesArr[item]->md_length_func();
}

void new_item(int item, mmon_info_mapping_t* iMap,
        void* source, void* dest, settings_t* setup)
//The function recieves it's raw data from the retrieved vector,
//translates it to the requested (presentable) data,
//and stores it in the dest address
{
    infoModulesArr[item]->md_new_func(iMap, source, dest, setup);
}

void del_item(int item, void* address)
//The function deletes it's item from the address given address.
//This is important in case an object holds a pointer to malloced
//memory space - in which case to free the raw_data array would
//cause the loss of the pointer and thus a memory leak.
{
    infoModulesArr[item]->md_del_func(address);
}

void display_item(int item, WINDOW* graph, void* source,
        int base_row, int min_row, int col, const double max, int width)
//This function draws the source item on the graph, considering:
//The bar starts at "base_row", and may go as high as "min_row",
//The bar belongs to colomn "col".
//The maximal value is the one pointed by max (obtained by "get_max").
{
    infoModulesArr[item]->md_disp_func(graph, pConfigurator, source,
            base_row, min_row, col, max, width);
}

char** display_help(int item)
//The function returns a char**, similar to help_str,
//describing the display type of the invoked function.
{
    return infoModulesArr[item]->md_help_func();
}

int get_text_info_byName(char *name, void *nodeDataPtr, char *buff, int *len, int width)
{
    int item = dm_getIdByName(name);
    int pos = get_pos(item);

    buff[0] = '\0';
    if (item < 0)
        return -1;

    if (infoModulesArr[item]->md_text_info_func) {
        int size = infoModulesArr[item]->md_text_info_func(nodeDataPtr + pos, buff, len, width);
        return size;
    }
    return 0;
}

double scalar_div_x(int item, const void* address, double x)
//The function returns a double value representing the data
//*The (x == 0) isn't a real possibility, and thus 
//the check is preformed. future implementations may use the 
// (x==0) case to contain more options.
{
    if (isScalar(item))
        return infoModulesArr[item]->md_scalar_func(address, x);
    return 1.0; //errorous
}

double get_max(mon_disp_prop_t* display, int item)
//returns value in a display for a specific type
{
    double temp; //current maximum
    legend_node_t* lgd_ptr = (display->legend).head;
    //we get to this point only if the legend isn't empty...
    //so lgd_ptr isn't NULL. and displayed != 0.

    int itemNumId = dm_getIdByName("num");
    int itemPos = get_pos(item);

    //if(item = itemNumId)
    //    mlog_bn_dg("mmon", "---- get max ---- item %d  numId %d\n", item, itemNumId);

    // Item is != num and there is no max... (irrelevant)
    if (!(item == itemNumId) && (!(isScalar(item)) || (display->displayed_bar_num <= 0)))
        return 0;

    // Find first node alive (or not) to compare to
    int index = -1; //index over the display data array

    if(item == itemNumId) {
        temp = scalar_div_x(item, (void*) ((long) display->displayed_bars_data[0] + itemPos), 1);
    }
    else {
        do {
            index++;
            temp = scalar_div_x
                    (item,
                    (void*) ((long) display->displayed_bars_data[index] + itemPos), 1);
        } while ((!(display->alive_arr[get_raw_pos(display, index)])) &&
                (index < display->displayed_bar_num - 1));

    
        if ((index == display->displayed_bar_num - 1) &&
            (!(display->alive_arr[get_raw_pos(display, index)]))) {
            //no living nodes found
            if (dbg_flg) fprintf(dbg_fp, "All nodes are down.");
            return 0;
        }
    }
    
    //if(item == itemNumId)
    //    mlog_bn_dg("mmon", "Temp %d \n", item, itemNumId);

    for (index = 0; index < display->displayed_bar_num;
            index++, lgd_ptr = lgd_ptr->next)
        //going over the display data array and the legend at the same time
    {
        if (lgd_ptr == NULL) //reset lgd_ptr...
            lgd_ptr = (display->legend).head;

        
        if (item != itemNumId && lgd_ptr->data_type != item)  continue;  // Same item
        if (!display->alive_arr[get_raw_pos(display, index)] && !(item == dm_getIdByName("num"))) continue;

        double val = scalar_div_x(item,(void*) ((long) display->displayed_bars_data[index] + get_pos(item)), 1);
        //mlog_bn_dg("mmon", "Val for max %.3f\n", val);
        if (val > temp) temp = val;
        //replace max
    }
    return temp;
}



double avg_by_item(mon_disp_prop_t* display, int item)
//This function calculates the averege value of the display
{
    double sum = 0; //current sum of elements
    int count = 0; //current amount of elements
    int index; //index over the display data array
    if (dbg_flg) fprintf(dbg_fp, "Averege by : %i\n", item);
    if (!(isScalar(item)))
        return -1; //order irrelevant

    for (index = 0; index < display->nodes_count; index++)
        if (display->alive_arr[index]) {
            sum +=
                    scalar_div_x(item,
                    (void*) ((long) (display->raw_data)
                    + display->block_length * index
                    + get_pos(item)),
                    1);
            count++;
        }

    return sum / count;
}


//LEGEND MANAGMENT
//The legend (wheather shown or hidden) is stored in the form of a linked
//list, in which the representation of each machine on the graph is stored
//using nodes, containing the data_type (of type disp_type_t) of the colomn.

void toggle_disp_type(mon_disp_prop_t* display, int item, int action)
//This function has control over the legend - upon invokation with certain
//display type, the function looks for it at the end of the legend. if
//found and action is 0 - removes it. otherwise - adds it to the end.
{
    legend_node_t* lgd_ptr = (display->legend).head;
    legend_node_t* free_ptr; //temp pointer to help in deleting items

    if (dbg_flg) fprintf(dbg_fp, "toggle %i (%i)\n", item, action);

    display->recount = 1; //so that "redraw" will detect change.

    if (lgd_ptr == NULL)
        //if the legend list is empty
    {
        lgd_ptr = (legend_node_t*) malloc(sizeof (legend_node_t));
        lgd_ptr->data_type = item;
        lgd_ptr->next = NULL;
        (display->legend).head = lgd_ptr;
        (display->legend).legend_size = 1;
        (display->legend).curr_ptr = lgd_ptr;
        if (dbg_flg) fprintf(dbg_fp, "Added as single\n");
        return;
    }

    //going to the end of the legend:
    while (lgd_ptr->next != NULL)
        lgd_ptr = lgd_ptr->next;

    if ((lgd_ptr->data_type == item) && (action == 0)) //if removal...
    {
        if ((display->legend).head == lgd_ptr)
            //remove head
        {
            free_ptr = (display->legend).head;
            (display->legend).head = (display->legend).head->next;
            free(free_ptr);
            (display->legend).legend_size--;
            (display->legend).curr_ptr = NULL;
            if (dbg_flg) fprintf(dbg_fp, "Removed from head\n");
        } else
            //remove end
        {
            free_ptr = (display->legend).head;
            while (free_ptr->next != lgd_ptr)
                free_ptr = free_ptr->next;
            free_ptr->next = NULL;
            free(lgd_ptr);
            (display->legend).legend_size--;
            (display->legend).curr_ptr = NULL;
            if (dbg_flg) fprintf(dbg_fp, "Popped from end\n");
        }
    } else //addition
    {
        if ((item == dm_getIdByName("space")) && (action == 0)) return;
        lgd_ptr->next = (legend_node_t*) malloc(sizeof (legend_node_t));
        (lgd_ptr->next)->data_type = item;
        (lgd_ptr->next)->next = NULL;
        (display->legend).legend_size++;
        (display->legend).curr_ptr = lgd_ptr->next;
        if (dbg_flg) fprintf(dbg_fp, "Added at end\n");
    }
}



int get_str_length(char** man_str)
//this function recieves a null truncated array of strings,
//and returns it's length
{
    int length = 0;
    while (man_str[length])
        length++;
    return length;
}

void show_manual(mon_disp_prop_t* display)
//Displays the help string (containing the user manual)
//The manual isn't just displayed from the top of the window,
//but is scrollable, and can be moved up and down using
//the arrow keys, when the manual is open.
//The manual also adds the help discription of all the display types.
{

    int index; //for indexing the manual strings array
    int long_index = 0; //for keeping track of total length of displayed manual
    int type_index; //for indexing display type arrays
    char** man_str; //manual string

    //display check
    if (!(display->graph))
        return;

    //if in the selected screen - change color.
    if (*glob_curr_display == display)
        wc_on(display->graph, &(pConfigurator->Colors._chartColor));

    //limits check
    if (display->help_start < 0)
        display->help_start = 0;

    index = display->help_start;
    wprintw(display->graph, "\n"); //cover for ncurses frame

    //printing part of the head_str of mmon, if in needed
    if (index < get_str_length(head_str)) {
        while (head_str[index]) {
            wprintw(display->graph, "%s\n", head_str[index]);
            index++;
            long_index++;
        }
        index = 0;
    } else
        index -= get_str_length(head_str);

    type_index = 0; //starting to go through display types
    while ((index >= 0) &&
            (long_index < display->max_row
            - display->min_row - display->bottom_spacing) &&
            (type_index < infoDisplayModuleNum))
        //the loop works like this:
        //if the index is larger then the amount of rows in the array,
        //we reduce the number of lines in tha array from the index and
        //continue. otherwise, we start from the indexed position and
        //print the array (or part of it), and end by setting index to 0
        //so that the next arrays will be printed in full.
    {
        man_str = display_help(type_index);

        if (index >= get_str_length(man_str))
            index -= get_str_length(man_str); //skip this one
        else {
            while ((man_str[index]) &&
                    (long_index < display->max_row
                    - display->min_row - display->bottom_spacing)) {
                wprintw(display->graph, " %c - %s\n", infoModulesArr[type_index]->md_key, man_str[index]);
                index++; //next line
                long_index++; //increment total count
            }
            index = 0; //next description - start from the begining
        }
        type_index++; //move to next type
    }

    //printing the last part of the help list
    if (index >= get_str_length(help_str))
        index -= get_str_length(help_str); //skip this one
    else {
        while ((help_str[index]) &&
                (long_index < display->max_row - display->min_row)) {
            wprintw(display->graph, "%s\n", help_str[index]);
            index++;
            long_index++;
        }
        index = 0;
    }

    //if the last line of the manual isn't printed,
    //the next start value is set to be the line after the last line -
    //to avoid scrolling away...
    if (long_index < display->max_row
            - display->min_row - display->bottom_spacing)
        display->help_start -=
            display->max_row - display->min_row
            - display->bottom_spacing - long_index;

    wborder(display->graph, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    mvwprintw(display->graph,
            display->max_row - display->min_row - display->bottom_spacing, 2,
            "Press 'h' to return");

    if (*glob_curr_display == display)
        wc_off(display->graph, &(pConfigurator->Colors._chartColor));
}

void display_totals(mon_disp_prop_t* display, int draw)
//Displays status bar at the bottom of the display
//in case of a connection failure, a timer is presented,
//counting the time since last valid data recieved.
//iff draw is 0 - only deletes the line - without redrawing.
{
    int index; //temp var
    infod_stats_t stats;
    char* totals_str = (char*) malloc(100 * sizeof (char));

    if (draw == 0) {
        move(display->max_row - 1,
                display->min_col);
        index = display->min_col;
        while (index < display->max_col) {
            printw(" ");
            index++;
        }
        free(totals_str);
        return;
    }

    if ((infolib_get_stats(display->mosix_host,
            glob_host_port, &stats)) == 1)
        //if data is available:
    {
        display->NAcounter = 0; //to reset the N\A-time counter

        move(display->max_row - 1,
                display->min_col);
        index = display->min_col;
        while (index < display->max_col) {
            printw(" ");
            index++;
        }

        if (glob_exiting > 0)
            sprintf(totals_str,
                "    Exit mmon2? (press 'q' to confirm)");
        else {
            sprintf(totals_str,
                    "[Nodes: %d][Live: %d][Host: %s][Info age: %.1f][Max age: %.1f]",
                    stats.total_num, stats.num_alive,
                    display->mosix_host,
                    stats.avgage,
                    stats.maxage);

            if (strlen(totals_str) >
                    display->max_col - display->min_col)
                //swich to "economic" mode:
                sprintf(totals_str,
                    "[Nodes: %d/%d] [host: %s]",
                    stats.num_alive, stats.total_num, display->mosix_host);
        }

        c_on(&(pConfigurator->Colors._bottomStatusbar));
        mvprintw(display->max_row - 1,
                display->min_col,
                totals_str);
        c_off(&(pConfigurator->Colors._bottomStatusbar));
    } else {
        if ((display->show_help) || (display->NAcounter > 1)) {
            move(display->max_row - 1, display->min_col);
            clrtoeol(); //clears only the status line
        } else {
            move(display->min_row, display->min_col);
            clrtobot(); //clears the display
        }

        c_on(&(pConfigurator->Colors._bottomStatusbar));
        move(display->max_row - 1, display->min_col);
        printw("[Information is not available %d]",
                display->NAcounter++);
        c_off(&(pConfigurator->Colors._bottomStatusbar));

        if (display->show_help)
            show_manual(display);
        refresh();
    }
    free(totals_str);
}




/****************************************************************************
 *                         
 *                        INPUT PROCCESSING
 *
 ***************************************************************************/

void move_left(mon_disp_prop_t* display)
//moves given display to the left
{
    int index, offset = 1;
    //counting the offset: how far we jump (1 if dead are shown)
    while ((1 - display->show_dead) &&
            (get_raw_pos(display, 0) - offset >= 0) &&
            (1 - display->alive_arr[get_raw_pos(display, 0) - offset]))
        offset++; //offset is anyway >=1, or else we don't move

    if (dbg_flg) fprintf(dbg_fp, "Right shift. offset = %i\n", offset);

    if ((get_raw_pos(display, 0) - offset >= 0) &&
            ((display->alive_arr[get_raw_pos(display, 0) - offset]) ||
            (display->show_dead)))
        //if we have more room to move left...
    {
        for (index = display->displayed_bar_num - 1; index >= (display->legend).legend_size; index--) {
            display->displayed_bars_data[index] =
                    display->displayed_bars_data[index - (display->legend).legend_size]; //offset*
        }

        for (index = 0; index < (display->legend).legend_size; index++)
            display->displayed_bars_data[index] =
                (void*) ((long) display->displayed_bars_data[index] - offset * display->block_length);

        displayRedrawGraph(display);
    }
}

void move_right(mon_disp_prop_t* display)
//moves given display to the right
{
    int index, offset = 1;
    while ((1 - display->show_dead) &&
            (get_raw_pos(display, display->displayed_bar_num - 1) + offset < display->nodes_count) &&
            (1 - display->alive_arr[get_raw_pos(display, display->displayed_bar_num - 1) + offset]))
        offset++; //offset is anyway >=1, or else we don't move

    if (dbg_flg) fprintf(dbg_fp, "Left shift. offset = %i\n", offset);

    if ((get_raw_pos(display, display->displayed_bar_num - 1) + offset < display->nodes_count) &&
            ((display->alive_arr[get_raw_pos(display, display->displayed_bar_num - 1) + offset]) ||
            (display->show_dead)))
        //if we have more room to move right...
    {
        for (index = 0; index < display->displayed_bar_num - (display->legend).legend_size; index++)
        {
            display->displayed_bars_data[index] =
                    display->displayed_bars_data[index + (display->legend).legend_size]; //offset*
        }

        for (; index < display->displayed_bar_num; index++)
            display->displayed_bars_data[index] =
                (void*) ((long) display->displayed_bars_data[index] +
                offset * display->block_length);

        displayRedrawGraph(display);
    }
}



/****************************************************************************
 *                         
 *                            INITIALIZATION
 *
 ***************************************************************************/

//loads color configuration file
//  1 - default colors
//  2 - black and white
//  3 - configuration file

void loadConfigFile(mmon_data_t *md)
{
    //if requested, load BW mode
    if (md->colorMode == 2) {
        if (dbg_flg)
            fprintf(dbg_fp, "Trying black and white configuation...\n");
        if (loadConfig(&pConfigurator, NULL, md->colorMode)) {
            return;
        } else {
            if (dbg_flg)
                fprintf(dbg_fp, "Loading default BW configuation - ERROR.\n");
            terminate();
        }
    }

    //if given, try loading configuration file
    if (md->colorMode == 3) {
        if (strlen(md->confFileName)) {
            if (dbg_flg2) fprintf(dbg_fp, "Trying parameter specified file: %s\n", md->confFileName);
            if (!loadConfig(&pConfigurator, md->confFileName, md->colorMode)) {
                printf("Specified configuration file %s does not exist.\n",
                        md->confFileName);
                terminate();
            } else {
                return;
            }
        }

        //try loading default file from home directory
        homeDir = getenv("HOME"); //find home directory for conf. file
        if (dbg_flg2)
            fprintf(dbg_fp, "Trying configuation from %s/%s\n",
                homeDir, defConfFileName);
        if (loadConfigEx(&pConfigurator, homeDir, defConfFileName)) {
            return;
        }

        //try loading default file from default directory
        if (dbg_flg2)
            fprintf(dbg_fp, "Trying configuration from %s/%s\n",
                defConfDir, &defConfFileName[1]);
        if (loadConfigEx(&pConfigurator, defConfDir, &defConfFileName[1])) {
            return;
        }
    }


    if (dbg_flg2)
        fprintf(dbg_fp, "Trying default color configuration...\n");
    if (loadConfig(&pConfigurator, NULL, 1)) //now color_mode has to be "1"
    {
        return;
    } else {
        if (dbg_flg2)
            fprintf(dbg_fp, "Loading default color configuration - ERROR.\n");
        terminate();
    }
    terminate();
}

//In the event of a change in the screen size or amount of displays,
//this function recalculates the positions of all displays.
//The new screens are redrawn only if redraw is asserted.
void size_recalculate(int redraw)
{
    int index; //temp var for loops

    int dispNum = 0;
    while ((dispNum < MAX_SPLIT_SCREENS) && (glob_displaysArr[dispNum]))
        dispNum++;
    //now total holds the amount of active displays

    //erase all screen content
    clear();
    refresh();

    // TODO move this copyright drawing from here to another method
    //COPYRIGHTS (string length is 40)
    c_on(&(pConfigurator->Colors._copyrightsCaption));
    mvprintw(0, (COLS - 64) / 2,
            "  Mmon - The MOSIX %c Monitor 2011 [%s] : Supported by Cluster Logic Ltd  ", 174, MMON_VERSION);
    c_off(&(pConfigurator->Colors._copyrightsCaption));

    for (index = 0; index < dispNum; index++) {
        //divides the screen equaly to all displays
        glob_displaysArr[index]->max_row = (index + 1) * (LINES - 1) / dispNum + 1;
        glob_displaysArr[index]->max_col = COLS;
        glob_displaysArr[index]->min_row = index * (LINES - 1) / dispNum + 1;
        glob_displaysArr[index]->min_col = 0;

        //document the new sizes
        if (dbg_flg)
            fprintf(dbg_fp, "Disp %i : <(%i,%i) , (%i,%i)> \n",
                index,
                glob_displaysArr[index]->min_row,
                glob_displaysArr[index]->min_col,
                glob_displaysArr[index]->max_row,
                glob_displaysArr[index]->max_col);

        //apply the changes to the screen (if needed)
        if (redraw)
            mmon_redraw(glob_displaysArr[index]);
    }
}

void mmon_init(mmon_data_t *md, int argc, char** argv)
//Initiates the Initialization sequence
//Should only be called once - at the beginning of the program.
{
    int index, index2, index3, key_count; //temp vars

    //split screen init:  cur_scr , screens
    //we allocate a pointer array for all possible screens,
    //which is filled with NULLs except the first, that is inited as main.
    mlog_init();
    mlog_registerColorFormatter((color_formatter_func_t) sprintf_color);
    mlog_registerModule("plugins", "Plugins management", "plugins");
    mlog_registerModule("mmon", "General mmon sections", "mmon");
    mlog_registerModule("disp", "Display section", "disp");
    mlog_registerModule("info", "Info methods", "info");

    mlog_registerModule("side", "Side window", "side");

    glob_displaysArr =
            (mon_disp_prop_t**) malloc(MAX_SPLIT_SCREENS * sizeof (mon_disp_prop_t*));
    md->displayArr = glob_displaysArr;
    for (glob_curr_display = &(glob_displaysArr[0]);
            glob_curr_display <= &(glob_displaysArr[MAX_SPLIT_SCREENS - 1]);
            glob_curr_display++)
        *glob_curr_display = NULL;

    glob_displaysArr[0] = (mon_disp_prop_t*) malloc(sizeof (mon_disp_prop_t));
    glob_curr_display = NULL;

    // First registering the internal modules
    displayModule_init();
    displayModule_registerStandardModules();

    // Initializing the first display (after loading internal display modules)
    displayInit(glob_displaysArr[0]);
    glob_curr_display = &(glob_displaysArr[0]);

    // Parsing command line and setting options on the first display
    parseCommandLine(md, argc, argv, glob_displaysArr[0]);


    //ncurses init
    initscr();
    cbreak();
    noecho();

    start_color();

    // Loading config file (and color definitons)
    loadConfigFile(md);

    // Initializing external display modules 
    if (pConfigurator->Plugins.pluginsNum == 0)
        displayModule_detectExternalPlugins(&pConfigurator->Plugins);
    displayModule_registerExternalPlugins(&pConfigurator->Plugins);
    displayModule_initModules();

    //mon_map init (explained above)
    infoModulesArr = (mon_display_module_t**) malloc((infoDisplayModuleNum + 1) * sizeof (mon_display_module_t*));
    for (index = 0; index < infoDisplayModuleNum; index++) {
        infoModulesArr[index] = &(mon_displays[index]);
    }

    //pos_map init (explained above)
    pos_map = (int*) malloc((infoDisplayModuleNum + 1) * sizeof (int));
    index2 = 0;
    for (index = 0; index < infoDisplayModuleNum; index++) {
        pos_map[index] = index2;
        index2 += infoModulesArr[index]->md_length_func();
    }

    //key_map init (explained above)
    max_key = infoModulesArr[0]->md_key;
    //first - finding the highest key value
    for (index = 0; index < infoDisplayModuleNum; index++)
        if (infoModulesArr[index]->md_key > max_key)
            max_key = infoModulesArr[index]->md_key;
    key_map = (int**) malloc((max_key + 1) * sizeof (int*));

    for (index = 0; index <= max_key; index++)
        //now - for each cell we insert it's bindings
    {
        //init the index-key binding
        key_map[index] = (int*) malloc(sizeof (int));
        *(key_map[index]) = -1;
        key_count = 0;

        for (index3 = 0; index3 < infoDisplayModuleNum; index3++)
            if (infoModulesArr[index3]->md_key == index) {
                key_map[index] =
                        (int*) realloc(key_map[index],
                        (key_count + 2) * sizeof (int));
                //allocating space for previous keys, new key and
                //the -1 truncation at the end.
                *(key_map[index] + key_count) =
                        index3; //adding the display to the index-key binding
                *(key_map[index] + key_count + 1) =
                        -1; //truncation
                key_count++;
            }
    }

    md->confFileName[0] = 0; //init configuration file name to <none>

    if (md->startWinFromCmdline) {
        setStartWindows(md);
    } else if (!md->skeepWinfile) {
        loadCurrentWindows(md);
    }
    //This is a signal to the disp_init to request a destination,
    //but to use parse_cmd_line, which is only relevant to the first display.

    current_set.uid = 65534; //init to nobody
    size_recalculate(1);

    mlog_bn_dy("mmon", " ---- INIT COMPLETE -----\n\n");
}

void mmon_free()
{
    if (glob_info_desc)
        free(glob_info_desc);
    if (glob_info_var_mapping)
        destroy_info_mapping(glob_info_var_mapping);
    free(infoModulesArr);
    free(pos_map);
    for (int index = 0; index <= max_key; index++)
        free(key_map[index]);
    free(key_map);
    free(glob_displaysArr);
}

void mmon_exit(int status)
{
    mmon_free();
    endwin();
    if (dbg_flg) fprintf(dbg_fp, "\nEnded.\n");
    if (dbg_flg) fclose(dbg_fp);

    exit(status);
}

void delete_curr_display()
{
    mon_disp_prop_t *display;
    int index = 0;

    display = *glob_curr_display;
    if (dbg_flg) fprintf(dbg_fp, "Current display: %p\n", display);

    // Searching for the position of the current display
    while ((index < MAX_SPLIT_SCREENS) &&
            (glob_displaysArr[index] != display))
        index++;

    if (index > 0)
        glob_curr_display = &(glob_displaysArr[index - 1]);

    // Deleting the current display
    while (index + 1 < MAX_SPLIT_SCREENS) {
        glob_displaysArr[index] = glob_displaysArr[index + 1];
        index++;
    }
    glob_displaysArr[index] = NULL; //another vacant slot created
    displayFree(display);
}
//terminates the current display

void terminate()
{
    int screenLeft = 0; //total screens left
    int p_usage = (glob_curr_display == NULL);

    if (!p_usage)
        delete_curr_display();
    if (glob_displaysArr[0])
        screenLeft = 1;


    /*      if (!screenLeft) //last screen deleted - time to exit! */
    /*      { */
    /*           if (dbg_flg) fprintf(dbg_fp, "\nExiting... .\n\n"); */
    /*           mmon_exit(0); */
    /*           mmon_free(); */

    // FIXME why the following code was needed??
    /* endwin(); */
    /*           if (p_usage) */
    /*           { */
    /*                p_usage = 0; */
    /*                while (usage_str[p_usage]) */
    /*                { */
    /*                     printf("%s", usage_str[p_usage]); */
    /*                     p_usage++;             */
    /*                } */
    /*           } */
    /*           exit(0); */
    //     }
    //     else
    size_recalculate(1);
}

void mmon_setDefaults(mmon_data_t * md)
{
    memset(md, 0, sizeof (mmon_data_t));

    md->colorMode = 3;
    char buff[1024];

    char *homeDir = getenv("HOME");
    if (!homeDir) {
        fprintf(stderr, "Error getting $HOME environment variable value\n");
        md->winSaveFile = strdup(MMON_DEF_WINDOW_SAVE_FILE);
    } else {
        sprintf(buff, "%s/%s", homeDir, MMON_DEF_WINDOW_SAVE_FILE);
        md->winSaveFile = strdup(buff);
    }

    md->nodesArgStr = NULL;
    md->filterNodesByName = 0;
    
}

int setStartWindows(mmon_data_t * md)
{
    for (int i = 0; md->startWinStrArr[i]; i++) {
        mlog_bn_dg("mmon", "---- Setting %d display ---- %s \n", i, md->startWinStrArr[i]);
        if (!md->displayArr[i]) {
            mlog_bn_dg("mmon", "allocating and initializing display %d\n", i);
        
            md->displayArr[i] = (mmon_display_t*) malloc(sizeof (mmon_display_t));
            displayInit(md->displayArr[i]);
         
        }
        displayInitFromStr(md->displayArr[i], md->startWinStrArr[i]);
        md->displayArr[i]->recount = 1;
    }

    size_recalculate(1);
    return 1;
}

int saveCurrentWindows(mmon_data_t * md)
{
    char str[1024];
    char *arr[MAX_SPLIT_SCREENS];

    int i = 0;
    for (; md->displayArr[i]; i++) {
        displaySaveToStr(md->displayArr[i], str, 1020);
        if (dbg_flg2) {
            fprintf(dbg_fp, "Display %d: %s\n", i, str);
        }
        arr[i] = strdup(str);
    }
    displayFileSave(md->winSaveFile, arr, i);

    for (int j = 0; md->displayArr[j]; j++)
        displayShowMsg(md->displayArr[j], "Saved windows configuration");

    sleep(1);

    for (int j = 0; md->displayArr[j]; j++)
        displayEraseMsg(md->displayArr[j]);

    return 1;
}

int loadCurrentWindows(mmon_data_t * md)
{


    int size = MAX_START_WIN;
    memset(md->startWinStrArr, 0, sizeof (char) * MAX_START_WIN);
    if (!displayFileLoad(md->winSaveFile, md->startWinStrArr, &size))
        return 0;

    return setStartWindows(md);
}

int mmon_redraw(mmon_display_t *display) {
    if(!display)
        return 0;
    int res;
    res = get_nodes_to_display(display);
    if(res) {
        displayRedraw(display);
    }
    return res;
}
int main(int argc, char** argv)
{
    mmon_data_t mmonData;
    int index, res;

    mmon_setDefaults(&mmonData);

    mmon_init(&mmonData, argc, argv); //All initialization
    mlog_bn_info("mmon", "After Init \n");

    res = 1;
    for (index = 0; index < MAX_SPLIT_SCREENS; index++) {
        if(!glob_displaysArr[index]) continue;
        res = res && mmon_redraw(glob_displaysArr[index]);
    }
    mlog_bn_info("mmon", "After First redraw: res = %d \n", res);
    mvprintw(LINES - 1, 0, ""); //move to LRCORNER
    refresh();


    while (glob_displaysArr[0]) {
        //update exity monitor
        if (glob_exiting > 0)
            glob_exiting--;

        sleep_or_input(1);
        if (is_input()) {
            int pressed = my_getch();
            parse_cmd(&mmonData, pressed);
        }

        // TODO fix this to use break instead of continue.
        for (index = 0; index < MAX_SPLIT_SCREENS; index++) {
            if(!glob_displaysArr[index]) continue;
            res = res && mmon_redraw(glob_displaysArr[index]);
            mlog_bn_info("mmon", "Display %p:  res = %d \n", glob_displaysArr[index], res);
        }

        mvprintw(LINES - 1, 0, ""); //move to LRCORNER
        refresh();
    }
    mlog_bn_info("mmon", "mmon done res = %d disp0 = %p \n", res, glob_displaysArr[0]);
    mmon_exit(res);
    if(!res) {
        fprintf(stderr, "Error - when tring to draw screen - probably infod is not there\n");
    }
    return 0;
}
