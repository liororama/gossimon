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
int exiting = 0;

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
mon_disp_prop_t** curr_display = NULL; //pointer to selected screen.


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
unsigned short host_port = MSX_INFOD_DEF_PORT; //defines connection port

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
    if (display->displayed_nodes_data) {
        free(display->displayed_nodes_data);
        display->displayed_nodes_data = NULL;
    }
    display->displayed_nodes_num = 0;

    if (display->raw_data) {
        if (display->data_count > 0)
            displayFreeData(display);
    }


    display->data_count = 0;
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
    return (((long) (display->displayed_nodes_data[index]) - (long) display->raw_data)
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
    int index = -1; //index over the display data array
    legend_node_t* lgd_ptr = (display->legend).head;
    //we get to this point only if the legend isn't empty...
    //so lgd_ptr isn't NULL. and displayed != 0.
    if (!(isScalar(item)) || (display->displayed_nodes_num <= 0))
        //there is no max... (irrelevant)
        return 0;

    do {
        index++;
        temp = scalar_div_x
                (item,
                (void*) ((long) display->displayed_nodes_data[index] + get_pos(item)), 1);
    } while ((!(display->life_arr[get_raw_pos(display, index)])) &&
            (index < display->displayed_nodes_num - 1));
    //find first node alive to compare to

    if ((index == display->displayed_nodes_num - 1) &&
            (!(display->life_arr[get_raw_pos(display, index)]))) {
        //no living nodes found
        if (dbg_flg) fprintf(dbg_fp, "All nodes are down.");
        return 0;
    }

    for (index = 0; index < display->displayed_nodes_num;
            index++, lgd_ptr = lgd_ptr->next)
        //going over the display data array and the legend at the same time
    {
        if (lgd_ptr == NULL) //reset lgd_ptr...
            lgd_ptr = (display->legend).head;

        if ((((lgd_ptr->data_type == item) && //same item
                (display->life_arr[get_raw_pos(display, index)])) //alive
                || (item == dm_getIdByName("num"))) && //or we're looking for the number...
                (scalar_div_x(item,
                (void*) ((long) display->displayed_nodes_data[index]
                + get_pos(item)), 1) > temp)) //larger then max
            temp =
                scalar_div_x(item,
                (void*) ((long) display->displayed_nodes_data[index]
                + get_pos(item)),
                1);
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

    for (index = 0; index < display->data_count; index++)
        if (display->life_arr[index]) {
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

int compare_num(const void* item1, const void* item2)
//compares the number of two objects (the pointers point to instances in raw_data)
{
    return (int) scalar_div_x(dm_getIdByName("num"), (void*) ((long) item1 + get_pos(dm_getIdByName("num"))), 0) -
            (int) scalar_div_x(dm_getIdByName("num"), (void*) ((long) item2 + get_pos(dm_getIdByName("num"))), 0);
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

/******************************************************************************
 * 
 *                                DATA RETRIEVAL
 *
 *****************************************************************************/

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
            display->mosix_host, host_port);

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
        glob_info_desc = infolib_info_description(display->mosix_host, host_port);
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

int get_nodes_to_display(mon_disp_prop_t* display)
//This is The heart of the mmon:
//Here, the data is obtained from the host(s).
//If the "get_all" is 1, then all avalable data (and not just what is
//currently displayed) is loaded to the raw_data array.

{
    idata_t *infod_data = NULL;
    idata_entry_t *curInfoEntry = NULL;
    idata_iter_t *iter = NULL;
    void* temp; //temp place-holder for numbers

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

    //bottom status line
    display_totals(display, display->show_status);

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
        return 1;
    }

    // Getting the information itself
    infod_data = infolib_all(display->mosix_host, host_port);
    if (!infod_data) {
        if (dbg_flg) fprintf(dbg_fp, "Error getting info\n");
        // Disable the sigalarm
        setitimer(ITIMER_VIRTUAL, NULL, NULL);
        return 1;
    }

    // Disable the sigalarm
    setitimer(ITIMER_REAL, NULL, NULL); // questionable
    setitimer(ITIMER_VIRTUAL, NULL, NULL);

    display->last_host = display->mosix_host;

    if (!(iter = idata_iter_init(infod_data))) {
        free(infod_data);
        return 0;
    }


    // Calculating the memory size for each node and allocating memory
    if (display->raw_data == NULL) {
        //DATA INIT
        //determine total block length
        display->block_length = 0;
        for (index = 0; index < infoDisplayModuleNum; index++)
            display->block_length += infoModulesArr[index]->md_length_func();

        //allocating data array memory
        if (infod_data->num > 0) {
            display->raw_data = calloc(infod_data->num, display->block_length);
            display->life_arr = (int*) malloc(sizeof (int) * infod_data->num);
        } else {
            display->raw_data = NULL;
            display->life_arr = NULL;
        }
    }

    //set needed info in the current_set
    set_settings(display, &current_set);

    for (i = 0; i < infod_data->num; i++) {
        //first loop only to insert MACHINE NUMBERS
        if (!(curInfoEntry = idata_iter_next(iter)))
            break;

        new_item(dm_getIdByName("num"), &glob_info_map, curInfoEntry,
                (void*) ((long) display->raw_data + display->block_length * i +
                get_pos(dm_getIdByName("num"))), &current_set);
    }


    display->data_count = infod_data->num;
    if (dbg_flg) fprintf(dbg_fp, "Data count : %i.\n", display->data_count);
    idata_iter_done(iter);

    // Sort the raw_data array by number
    qsort(display->raw_data,
            display->data_count,
            display->block_length,
            &compare_num);

    temp = (char*) malloc(display->block_length);
    // temp will use now as a place-holder for numbers.
    // each new number will be put in temp,
    // and then compared with others to find the correct index.

    if (!(iter = idata_iter_init(infod_data))) {
        free(infod_data);
        return 0;
    }
    // Second loop, to put values in correct order
    for (i = 0 ; i < infod_data->num ; i++) {
        if (!(curInfoEntry = idata_iter_next(iter)))
            break;

        // Place desired number in temp
        new_item(dm_getIdByName("num"), &glob_info_map, curInfoEntry, (void*) ((long) temp + get_pos(dm_getIdByName("num"))), &current_set);
        j = 0;
        while ((compare_num(temp, (void*) ((long) display->raw_data + display->block_length * j)) != 0) &&
                (j < display->data_count))
            j++;
        if (j == infod_data->num)
            return 0;


        // Checking if the node is valid:
        display->life_arr[j] = ((curInfoEntry->valid != 0) &&
                               (curInfoEntry->data->hdr.status & INFOD_ALIVE));


        // Filling the data into the apropriate entry. This should be the only
        // place where specific data is accessed
        if (display->life_arr[j]) {
            for(int dispModule = 0 ; dispModule < infoDisplayModuleNum; dispModule++)
            {
                new_item(dispModule, &glob_info_map, curInfoEntry,
                        (void*) ((long) display->raw_data + display->block_length * j +
                        get_pos(dispModule)), &current_set);
            }
        } else {
            // Extracting the status for a dead node
            if (dbg_flg) fprintf(dbg_fp, "#%i is DEAD!\n", j);
            new_item(dm_getIdByName("infod-status"), &glob_info_map, curInfoEntry,
                    (void*) ((long) display->raw_data + display->block_length * j +
                    get_pos(dm_getIdByName("infod-status"))), &current_set);
        }
    }

    idata_iter_done(iter);
    free(infod_data);
    free(temp);
    if (dbg_flg) fprintf(dbg_fp, "Data retrieved.\n");
   
    return 1;
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
    if (*curr_display == display)
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

    if (*curr_display == display)
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
            host_port, &stats)) == 1)
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

        if (exiting > 0)
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
            (1 - display->life_arr[get_raw_pos(display, 0) - offset]))
        offset++; //offset is anyway >=1, or else we don't move

    if (dbg_flg) fprintf(dbg_fp, "Right shift. offset = %i\n", offset);

    if ((get_raw_pos(display, 0) - offset >= 0) &&
            ((display->life_arr[get_raw_pos(display, 0) - offset]) ||
            (display->show_dead)))
        //if we have more room to move left...
    {
        for (index = display->displayed_nodes_num - 1; index >= (display->legend).legend_size; index--) {
            display->displayed_nodes_data[index] =
                    display->displayed_nodes_data[index - (display->legend).legend_size]; //offset*
        }

        for (index = 0; index < (display->legend).legend_size; index++)
            display->displayed_nodes_data[index] =
                (void*) ((long) display->displayed_nodes_data[index] - offset * display->block_length);

        displayRedrawGraph(display);
    }
}

void move_right(mon_disp_prop_t* display)
//moves given display to the right
{
    int index, offset = 1;
    while ((1 - display->show_dead) &&
            (get_raw_pos(display, display->displayed_nodes_num - 1) + offset < display->data_count) &&
            (1 - display->life_arr[get_raw_pos(display, display->displayed_nodes_num - 1) + offset]))
        offset++; //offset is anyway >=1, or else we don't move

    if (dbg_flg) fprintf(dbg_fp, "Left shift. offset = %i\n", offset);

    if ((get_raw_pos(display, display->displayed_nodes_num - 1) + offset < display->data_count) &&
            ((display->life_arr[get_raw_pos(display, display->displayed_nodes_num - 1) + offset]) ||
            (display->show_dead)))
        //if we have more room to move right...
    {
        for (index = 0; index < display->displayed_nodes_num - (display->legend).legend_size; index++)
        {
            display->displayed_nodes_data[index] =
                    display->displayed_nodes_data[index + (display->legend).legend_size]; //offset*
        }

        for (; index < display->displayed_nodes_num; index++)
            display->displayed_nodes_data[index] =
                (void*) ((long) display->displayed_nodes_data[index] +
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
        fprintf(dbg_fp, "Trying default color configuation...\n");
    if (loadConfig(&pConfigurator, NULL, 1)) //now color_mode has to be "1"
    {
        return;
    } else {
        if (dbg_flg2)
            fprintf(dbg_fp, "Loading default color configuation - ERROR.\n");
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

    int total = 0;
    while ((total < MAX_SPLIT_SCREENS) &&
            (glob_displaysArr[total]))
        total++;
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

    for (index = 0; index < total; index++) {
        //divides the screen equaly to all displays
        glob_displaysArr[index]->max_row = (index + 1) * (LINES - 1) / total + 1;
        glob_displaysArr[index]->max_col = COLS;
        glob_displaysArr[index]->min_row = index * (LINES - 1) / total + 1;
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
    mlog_registerModule("side", "Side window", "side");

    glob_displaysArr =
            (mon_disp_prop_t**) malloc(MAX_SPLIT_SCREENS * sizeof (mon_disp_prop_t*));
    md->displayArr = glob_displaysArr;
    for (curr_display = &(glob_displaysArr[0]);
            curr_display <= &(glob_displaysArr[MAX_SPLIT_SCREENS - 1]);
            curr_display++)
        *curr_display = NULL;

    glob_displaysArr[0] = (mon_disp_prop_t*) malloc(sizeof (mon_disp_prop_t));
    curr_display = NULL;

    // First registering the internal modules
    displayModule_init();
    displayModule_registerStandardModules();

    // Initializing the first display (after loading internal display modules)
    displayInit(glob_displaysArr[0]);
    curr_display = &(glob_displaysArr[0]);

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

    if (dbg_flg) fprintf(dbg_fp, "\nINIT COMPLETE.\n\n");
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

    display = *curr_display;
    if (dbg_flg) fprintf(dbg_fp, "Current display: %p\n", display);

    // Searching for the position of the current display
    while ((index < MAX_SPLIT_SCREENS) &&
            (glob_displaysArr[index] != display))
        index++;

    if (index > 0)
        curr_display = &(glob_displaysArr[index - 1]);

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
    int p_usage = (curr_display == NULL);

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
}

int setStartWindows(mmon_data_t * md)
{
    for (int i = 0; md->startWinStrArr[i]; i++) {
        if (dbg_flg2) fprintf(dbg_fp, "Setting %d display\n", i);
        if (!md->displayArr[i]) {
            md->displayArr[i] = (mmon_display_t*) malloc(sizeof (mmon_display_t));
            displayInit(md->displayArr[i]);
        }
        displayInitFromStr(md->displayArr[i], md->startWinStrArr[i]);
    }

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

    res = 1;
    for (index = 0; index < MAX_SPLIT_SCREENS; index++)
        res = res && mmon_redraw(glob_displaysArr[index]);

    mvprintw(LINES - 1, 0, ""); //move to LRCORNER
    refresh();


    while (res && (glob_displaysArr[0])) {
        //update exity monitor
        if (exiting > 0)
            exiting--;

        sleep_or_input(1);
        if (is_input()) {
            int pressed = my_getch();
            parse_cmd(&mmonData, pressed);
        }
        for (index = 0; index < MAX_SPLIT_SCREENS; index++)
            res = res && mmon_redraw(glob_displaysArr[index]);
        mvprintw(LINES - 1, 0, ""); //move to LRCORNER
        refresh();
    }
    return 0;
}
