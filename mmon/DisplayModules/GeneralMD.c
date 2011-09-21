/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


/****************************************************************************
 *                         
 *                              DISPLAY MODULES
 *                              ===============
 *
 *
 *
 * This is the part where each display mode has a set of functions, used for
 * the manipulation of the data representing each mode. Every creation, 
 * change or destruction, is done by these modules, including actual screen
 * display (given the right coordinates, and atarget window pointer).
 * 
 * Here is a prototype, for future reference (substitute "abc" with a name):
 *
 *  //DISP_ABC
 *
 * char* abc_desc[] = 
 *   { 
 *     " Description line 1 ",
 *     " Description line 2 ",
 *     " Description line 3 ",
 *     (char*)NULL, 
 *   };
 *
 *  //This is the description string, and it contains a short (few lines)
 *  //Description of the function of the display type. It must end with a
 *  //NULL, and it is recommended that the last line (#3 in the example
 *  //will be empty - ""). It's also recommended to state the types key 
 *  //binding, next to it's name, for easy reference.
 *
 *  int abc_get_length ();
 *  //This function returns the required length (in bytes) to store all the
 *  //data needed to display the bar later on. For example, if the bar
 *  //represents a number - it may return sizeof(int).
 *  //For variable length, a pointer may be used: sizeof(char*). 
 *  
 * 
 *  void abc_new_item (mmon_info_mapping_t* iMap, const void* source, 
 *                     void* dest, settings_t* setup);
 *  //This function handles the creation of a new instance of the type.
 *  //It's given the pointer to the source of the data, and iMap provides
 *  //the correct offset to reach the data itself.
 *  //The destination is also given - to an already alocated memory,
 *  //of the size requested by "<type>_get_length()".
 *  //CAUTION : memory overflow may overrun and corrupt other data!!!
 *
 * 
 *  void abc_del_item (void* address);
 *  //This function used when the "<type>_new_item()" function allocates
 *  //memory of it's own (in addition to provided memory space).
 *  //For instace, for items of variable length.
 *  //address points to the same place as dest from "<type>_new_item()".
 *  //To avoid memory leaks - at the end of the run of the program -
 *  //mmon invokes these functions for each data type - to clear up the
 *  //memory.For optimization reasons - in runtime memory reallocation 
 *  //is handled by mmon itself:
 *  //Requesting vlen data into an allocated space will cause reallocation.
 *
 *  void abc_display_item (WINDOW* graph, Configurator* pConfigurator, 
 *                         const void* source, int base_row, int min_row, 
 *                          int col, const double max, int width);
 *  //This function must provide display services for the type.
 *  //The graph is an ncurses window (see man ncurses), in which the type
 *  //is to be displayed. source points to the previously stored data,
 *  //and base_row, min_row and col are the position of the display.
 *  //max is the maximal value returned by <type>_scalar_div_x for this
 *  //display. width is the width mode of the display (1, 2 or 3).
 *  //pConfigurator is a pointer to the currently selected color scheme. 
 *
 *  double abc_scalar_div_x (const void* item, double x);
 *  //This function returns a scalar value of the data, if possible.
 *  //The source points directly to the stored data, and x is a double for
 *  //the data to be devided by.
 *  //In fact, mmon could have done the devision, but this structure allows
 *  //more flexibility in the future. For example - special values can be
 *  //returned upon invokation with x=0...
 * 
 *
 * -The two last functions might not exist (NULL) for non-scalar values.
 *
 *
 *
 ***************************************************************************/

#include "mmon.h"
#include "drawHelper.h"
#include "DisplayModules.h"



int addPriodMark(char* proc_watch, char *bar_str, int *len)
     //help function for using_users, using_clusters
{
  proc_watch_entry_t procArr[20];
  int pSize = 20;
  int found = 0;
  int i;
  if(getProcWatchInfo(proc_watch, procArr, &pSize)) 
    {
      for(i=0 ; i< pSize ; i++) 
        {
          if(strcmp(procArr[i].procName, "pd") == 0 &&
             procArr[i].procStat == PS_OK)
            {
              strcat(bar_str, "+");
              found = 1;
              break;
            }
        }
    }
  if(!found) 
    strcat(bar_str, " ");
  
  *len += 1;
  return 1;
}


/****************************************************************************
 *                         
 *                          DISP_NAME MODULE                      
 *
 ***************************************************************************/
 
char* name_desc[] = 
  {
    " Name of nodes",
    (char*)NULL, };

int name_get_length ()
{
  return MACHINE_NAME_SZ * sizeof(char);
}

char** name_display_help ()
{
  
  return name_desc;
}

void name_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if (source)
    {
      memcpy(dest, ((idata_entry_t*)source)->name, MACHINE_NAME_SZ);
      ((char*)dest)[MACHINE_NAME_SZ-1] = '\0';
    }
  else
    ((char*)dest)[0] = '\0';
}

void name_del_item (void* address) { ((char*)address)[0] = '\0'; }

void name_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const  void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  int offset = 0; //offset from bottom axis

  if (base_row - min_row > 6)
    offset += 4;

  display_rtr(graph, &(pConfigurator->Colors._nodeCaption),
              base_row - offset, col, ((char*)source), strlen((char*)source), 0);
  return;
}

double name_scalar_div_x (const void* item, double x)
     //This "scalarization" function uses the length of the name.
{
  if (x == 0)
    return 0;
  return strlen((char*)item) / x;
}

int name_text_info(void *source, char *buff, int *size, int width) {
  int maxSize = *size;
  char *name = (char *)source;
  sprintf(buff, "%s", name);
  *size = strlen(name);
  return *size;
}


mon_display_module_t name_mod_info = {
 md_name:         "name",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Name", 
 md_shortTitle:   "Name",
 md_titlength:    4,             
 md_format:       "", 
 md_key:          'N',
 md_length_func:  name_get_length, 
 md_scalar_func:  name_scalar_div_x, 
 md_new_func:     name_new_item, 
 md_del_func:     name_del_item,  
 md_disp_func:    name_display_item, 
 md_help_func:    name_display_help,
 md_text_info_func: name_text_info,
 md_side_window_display: 0,
};

/****************************************************************************
 *                         
 *                          DISP_NUM MODULE                      
 *
 ***************************************************************************/

char* num_desc[] = {
  " Display node numbers",
  (char*)NULL, };
 
int num_get_length ()
{
  return sizeof(int);
}

char** num_display_help ()
{
  return num_desc;
}

void num_new_item (mmon_info_mapping_t* iMap, const void* source, 
                   void* dest, settings_t* setup)
{
  if (source)
    *((int*)dest) = ((idata_entry_t*)source)->data->hdr.pe;
  else
    *((int*)dest) = 0;
}

void num_del_item (void* address) { return; }

void num_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  int sourceVal = *((int*)source);
  int maxVal =  (int)max;
  if (maxVal > 0) 
    display_bar(graph, base_row, col, 
                ((base_row-min_row) * sourceVal) / maxVal,
                ' ', &(pConfigurator->Colors._loadCaption), 0);  
  return;
}

double num_scalar_div_x (const void* item, double x)
     //This "scalarization" function uses the length of the number.
     //The "x" parameter is ignored in this case.
{
  int count = 0;
  int val   = *((int*)item);
  
  if (x == 0)
    return (double)val;  

  while (val >= 10)
    {
      val = val / 10;
      count++; 
    }  
  return count + 1;
}

mon_display_module_t num_mod_info = {
 md_name:         "num",
 md_iscalar:      1,
 md_canrev:       0, 
 md_title:        "Num", 
 md_titlength:    3,             
 md_shortTitle:   "Num",
 md_format:       "", 
 md_key:          'N',
 md_length_func:  num_get_length, 
 md_scalar_func:  num_scalar_div_x, 
 md_new_func:     num_new_item, 
 md_del_func:     num_del_item,  
 md_disp_func:    num_display_item, 
 md_help_func:    num_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0,
};

/****************************************************************************
 *                         
 *                          SPACE MODULE                      
 *
 * Represents a blank colomn.
 *
 ***************************************************************************/

char* space_desc[] = {
  " Display a space (between bars)",
  (char*)NULL, };

int space_get_length ()
{
  return 0;
}
 

char** space_display_help ()
{
  return space_desc;
}

void space_new_item (mmon_info_mapping_t* iMap, const void* source, 
                     void* dest, settings_t* setup)
{
  return;  
}

void space_del_item (void* address) { return; }

void space_display_item (WINDOW* graph, Configurator* pConfigurator, 
                         const void* source, int base_row, 
                         int min_row, int col, const double max, int width)
{
  return;
}

mon_display_module_t space_mod_info = {
 md_name:         "space",
 md_iscalar:      0,
 md_canrev:       1, 
 md_title:        "SPACE", 
 md_shortTitle:   "Space",
 md_titlength:    5,             
 md_format:       "", 
 md_key:          32,
 md_length_func:  space_get_length, 
 md_scalar_func:  NULL, 
 md_new_func:     space_new_item, 
 md_del_func:     space_del_item,  
 md_disp_func:    space_display_item, 
 md_help_func:    space_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0,
};


/****************************************************************************
 *                         
 *                          DISP_STATUS MODULE                      
 *
 ***************************************************************************/

char* status_desc[] = {
  " MOSIX Status of nodes",
  (char*)NULL, };

int status_get_length ()
{
  return sizeof(unsigned short)
    + MACHINE_NAME_SZ * sizeof(char);
}

char** status_display_help ()
{
  return status_desc;
}

void status_new_item (mmon_info_mapping_t* iMap, const void* source, 
                      void* dest, settings_t* setup)
{
  if (source)
    {
      memcpy(dest + sizeof(unsigned short), 
             ((idata_entry_t*)source)->name, MACHINE_NAME_SZ);
      ((char*)(dest + sizeof(unsigned int)))[MACHINE_NAME_SZ-1] = '\0';
    }
  else
    ((char*)(dest + sizeof(unsigned int)))[0] = '\0';


  if(iMap->status)
    *((unsigned short*)dest)  = 
      *((unsigned short*)(((idata_entry_t*)source)->data->data 
               + iMap->status->offset));
}

void status_del_item (void* address) { return; }

void status_display_item (WINDOW* graph, Configurator* pConfigurator, 
                          const void* source, int base_row, 
                          int min_row, int col, const double max, int width)
{
  char temp; //temp char for status marks
  
  //We display node names
  display_rtr(graph, &(pConfigurator->Colors._statusNodeName),
              base_row - 9, col, //position 
              ((char*)(source + sizeof(unsigned short))),
              strlen((char*)(source + sizeof(unsigned short))), 0); 
              
  // Displaying MOSIX status information
  temp = 'V';
  display_str(graph, &(pConfigurator->Colors._statusLetter), 
              base_row - 7, col, &temp, 1,
              ((*((unsigned short*)(source))) & MSX_STATUS_VM));

  temp = 'P';
  display_str(graph, &(pConfigurator->Colors._statusLetter), 
              base_row - 6, col, &temp, 1,
              ((*((unsigned short*)(source))) & MSX_STATUS_NO_PROVIDER));
  
  temp = 'S';	
  display_str(graph, &(pConfigurator->Colors._statusLetter), 
              base_row - 5, col, &temp, 1,
              ((*((unsigned short*)(source))) & MSX_STATUS_STAY));

  temp = 'L';	
  display_str(graph, &(pConfigurator->Colors._statusLetter), 
              base_row - 4, col, &temp, 1,
              ((*((unsigned short*)(source))) & MSX_STATUS_LSTAY));
  
  temp = 'B';	
  display_str(graph, &(pConfigurator->Colors._statusLetter), 
              base_row - 3, col, &temp, 1,
              ((*((unsigned short*)(source))) & MSX_STATUS_BLOCK));

  temp = 'Q';	
  display_str(graph, &(pConfigurator->Colors._statusLetter), 
              base_row - 2, col, &temp, 1, 
              ((*((unsigned short*)(source))) & MSX_STATUS_QUIET));

  temp = 'N';
  display_str(graph, &(pConfigurator->Colors._statusLetter), 
              base_row - 1, col, &temp, 1,
              ((*((unsigned short*)(source))) & MSX_STATUS_NOMFS));
}

mon_display_module_t status_mod_info = {
 md_name:         "status",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Status", 
 md_shortTitle:   "Status",
 md_titlength:    6,             
 md_format:       "", 
 md_key:          's',
 md_length_func:  status_get_length, 
 md_scalar_func:  NULL, 
 md_new_func:     status_new_item, 
 md_del_func:     status_del_item,  
 md_disp_func:    status_display_item, 
 md_help_func:    status_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0,
};


/****************************************************************************
 *                         
 *                          INFOD_STATUS MODULE                      
 *
 ***************************************************************************/

char* infod_status_desc[] = {
  " Infod status of nodes", 
  (char*)NULL, };
 
int infod_status_get_length ()
{
  return sizeof(int);
}

char** infod_status_display_help ()
{
  return infod_status_desc;
}

void infod_status_new_item (mmon_info_mapping_t* iMap, const void* source, 
                   void* dest, settings_t* setup)
{
  if (source)
    *((int*)dest) = ((idata_entry_t*)source)->data->hdr.status;
}

void infod_status_del_item (void* address) { return; }

void infod_status_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  int sourceVal = *((int*)source);
  int maxVal =  (int)max;
  if (maxVal > 0) 
    display_bar(graph, base_row, col, 
                ((base_row-min_row) * sourceVal) / maxVal,
                ' ', &(pConfigurator->Colors._loadCaption), 0);  
  return;
}

// Just returning the status
double infod_status_scalar_div_x (const void* item, double x)
{
  return ((double)*((int*)item));
}

mon_display_module_t infod_status_mod_info = {
 md_name:         "infod-status",
 md_iscalar:      1,
 md_canrev:       0, 
 md_title:        "Infod Status", 
 md_titlength:    12,             
 md_shortTitle:   "Infod",
 md_format:       "", 
 md_key:          'N',
 md_length_func:  infod_status_get_length, 
 md_scalar_func:  infod_status_scalar_div_x, 
 md_new_func:     infod_status_new_item, 
 md_del_func:     infod_status_del_item,  
 md_disp_func:    infod_status_display_item, 
 md_help_func:    infod_status_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0
};

/****************************************************************************
 *                         
 *                          MOSIX VERSION MODULE                      
 *
 ***************************************************************************/

char* version_desc[] = {
  " MOSIX version of kernel", 
  (char*)NULL, 
  }; 

int version_get_length ()
{
  return sizeof(int);
}

char** version_display_help ()
{
  return version_desc;
}

void version_new_item (mmon_info_mapping_t* iMap, const void* source, 
                       void* dest, settings_t* setup)
{
  if(iMap->mosix_version)
    *((int*)dest)  = 
      *((int*)(((idata_entry_t*)source)->data->data 
               + iMap->mosix_version->offset));
}

void version_del_item (void* address) { return; }

void version_display_item (WINDOW* graph, Configurator* pConfigurator, 
                           const void* source, int base_row, 
                           int min_row, int col, const double max, int width)
{
  unsigned int a,b,c;
  unsigned int ver = *((int*)source);
  char verStr[15];

  int offset = 0; //offset from bottom axis

  if (base_row - min_row > 6)
    offset += 4;

  if(ver != 0) {
    a = (ver & 0x00ff0000)>>16;
    b = (ver & 0x0000ff00)>>8;
    c = (ver & 0x000000ff);
    sprintf(verStr, "%d %d %d", a, b, c);
  }
  else {
    sprintf(verStr, "N A");
  }
	
  display_rtr(graph, &(pConfigurator->Colors._statusNodeName),
              base_row - offset, col, verStr, strlen(verStr), 0);
  return;
}

double version_scalar_div_x (const void* item, double x)
{
  return (double)(*((int*)item)) / x;
}

mon_display_module_t version_mod_info = {
 md_name:         "version",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Mosix Version", 
 md_titlength:    13,             
 md_shortTitle:   "MOSIX",
 md_format:       "", 
 md_key:          's',
 md_length_func:  version_get_length, 
 md_scalar_func:  version_scalar_div_x, 
 md_new_func:     version_new_item, 
 md_del_func:     version_del_item,  
 md_disp_func:    version_display_item, 
 md_help_func:    version_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0
};


/****************************************************************************
 *                         
 *                          KERNEL VERSION MODULE                      
 *
 ***************************************************************************/

static mon_display_module_t *kernel_version_md_info;

char* kernel_version_desc[] = {
  " Kernel Version", 
  (char*)NULL, 
  }; 

int kernel_version_get_length ()
{
  return sizeof(char *);
}


char** kernel_version_display_help ()
{
  return version_desc;
}

void kernel_version_new_item (mmon_info_mapping_t* iMap, const void* source, 
                       void* dest, settings_t* setup)
{
      getVlenItem(((idata_entry_t*) source)->data,
            kernel_version_md_info->md_info_var_map, (char**) dest);
  
}

void kernel_version_del_item(void* address) {
    if (*((char**) address))
        free(*((char**) address));
}

void kernel_version_display_item (WINDOW* graph, Configurator* pConfigurator, 
                           const void* source, int base_row, 
                           int min_row, int col, const double max, int width)
{
  unsigned int a,b,c;
  unsigned int ver = *((int*)source);
  char *xmlStr = *(char**) source;
  char *verStr = strdup(xmlStr);
  while(*verStr != '>') verStr++;
  verStr++;
  char *ptr = verStr;
  while(*ptr != '<') ptr++;
  *ptr = '\0';
  
  display_rtr(graph, &(pConfigurator->Colors._statusNodeName),
              base_row - 1, col, verStr, strlen(verStr), 0);
  return;
}

double kernel_version_scalar_div_x (const void* item, double x)
{
  return (double)(*((int*)item)) / x;
}

void kernel_version_update_info_desc(void *d, variable_map_t *glob_info_var_mapping)
{
    mon_display_module_t *md = (mon_display_module_t *) d;

    kernel_version_md_info = md;
    md->md_info_var_map = get_var_desc(glob_info_var_mapping, ITEM_KERNEL_RELEASE_NAME);
    return;
}

int kernel_version_text_info(void *source, char *buff, int *size, int width) {
  int maxSize = *size;
  
  buff[0] = '\0';

  char *verStr = *(char**) source;
  sprintf(buff, "%s", verStr);
  return strlen(buff);
}


mon_display_module_t kernel_version_mod_info = {
 md_name:         "kernel-version",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Kernel Version", 
 md_titlength:    13,             
 md_shortTitle:   "Kernel",
 md_format:       "", 
 md_key:          's',
 md_length_func:  kernel_version_get_length, 
 md_scalar_func:  kernel_version_scalar_div_x, 
 md_new_func:     kernel_version_new_item, 
 md_del_func:     kernel_version_del_item,  
 md_disp_func:    kernel_version_display_item, 
 md_help_func:    kernel_version_display_help,
 md_text_info_func: kernel_version_text_info,
 md_side_window_display: 0,
 md_update_info_desc_func : kernel_version_update_info_desc,

};


/****************************************************************************
 *                         
 *                          DISP_UPTIME MODULE                      
 *
 ***************************************************************************/

char* uptime_desc[] = {
  " Uptime of nodes",
  (char*)NULL, }; 

int uptime_get_length ()
{
  return sizeof(unsigned long);
}


char** uptime_display_help ()
{
  return uptime_desc;
}

void uptime_new_item (mmon_info_mapping_t* iMap, const void* source, 
                       void* dest, settings_t* setup)
{
  if(iMap->uptime)
    *((unsigned long*)dest)  = 
      *((unsigned long*)(((idata_entry_t*)source)->data->data 
                         + iMap->uptime->offset));
}

void uptime_del_item (void* address) { return; }

void get_uptime_str(const void *source, char *uptimeStr) {
  unsigned long uptime = *((unsigned long*)source);

  if (uptime > 60*60*24)
    sprintf(uptimeStr, "%3d d", (int)(uptime / (60*60*24)));
  else if (uptime > 60*60)
    sprintf(uptimeStr, "%3d h", (int)(uptime / (60*60)));
  else if (uptime > 60)
    sprintf(uptimeStr, "%3d m", (int)(uptime / (60)));
  else
    sprintf(uptimeStr, "%3d s", (int)(uptime));

}

void uptime_display_item (WINDOW* graph, Configurator* pConfigurator, 
                           const void* source, int base_row, 
                           int min_row, int col, const double max, int width)
{
  char uptimeStr[32];
  int offset = 0; //offset from bottom axis

  if (base_row - min_row > 10)
    offset += 4;
  
  get_uptime_str(source, uptimeStr);
  display_rtr(graph, &(pConfigurator->Colors._statusNodeName),
              base_row - offset, col, uptimeStr, strlen(uptimeStr), 0);
  return;
}

int uptime_text_info(void *source, char *buff, int *size, int width) {
  int maxSize = *size;
  get_uptime_str(source, buff);
  *size = strlen(buff);
  return *size;
}


mon_display_module_t uptime_mod_info = {
 md_name:         "uptime",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Uptime", 
 md_titlength:    6,             
 md_shortTitle:   "Uptime",
 md_format:       "", 
 md_key:          's',
 md_length_func:  uptime_get_length, 
 md_scalar_func:  NULL, 
 md_new_func:     uptime_new_item, 
 md_del_func:     uptime_del_item,  
 md_disp_func:    uptime_display_item, 
 md_help_func:    uptime_display_help,
 md_text_info_func: uptime_text_info,
 md_side_window_display: 0
};
/*     {DISP_UPTIME,            0, 0, "Uptime", 6,   "", 's', */
/*      uptime_get_length, NULL, uptime_new_item, uptime_del_item,   */
/*      uptime_display_item, uptime_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_LOAD MODULE                      
 *
 ***************************************************************************/

char* load_desc[] = 
  { 
    " Load of nodes",
    (char*)NULL, };

int load_get_length ()
{
  //we hold both the load value and the priority value
  return sizeof(float) + 2*sizeof(unsigned short) + sizeof(char*);
}

char** load_display_help ()
{
  return load_desc;
}

void load_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if ((iMap->load) && (iMap->priority))
    {
      *((float*)dest) = 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                           + iMap->load->offset));
      *((float*)dest) *= 
        (float)(*(float*)(setup->yardstick) / 
                        (float)MOSIX_STD_SPEED);
      *((float*)dest) /= 100;

      *((unsigned short*)(dest + sizeof(float))) = 
        *((unsigned short*)(((idata_entry_t*)source)->data->data 
                            + iMap->priority->offset));

      if (setup->uid < 65534)
        {
          *((unsigned short*)(dest + sizeof(float) + sizeof(unsigned short))) 
            = setup->uid;
          getVlenItem(((idata_entry_t*)source)->data, iMap->usedby, 
                      (char**)(dest + sizeof(float) + 2*sizeof(unsigned short)));
        }
      else 
        *((char**)(dest + sizeof(float) + 2*sizeof(unsigned short))) = NULL;
    }
}

void load_del_item (void* address) 
{ 
  if (*((char**)(address + sizeof(float) + 2*sizeof(unsigned short))))
    free(*((char**)(address + sizeof(float) + 2*sizeof(unsigned short))));
}

void load_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  float sourceVal = *((float*)source);
  float maxVal = (float)max;
  float priority = *((unsigned short*)
                     (source + sizeof(float)));
  
  //following only used if username option used
  char* users = *((char**)
                  (source + sizeof(float) + 2*sizeof(unsigned short)));
  unsigned short uid = *((unsigned short*)
                         (source + sizeof(float) + sizeof(unsigned short)));
  node_usage_status_t  usageStatus;
  used_by_entry_t      useArr[20];
  int                  size = 20;
  if (maxVal > 0) 
    {
      //if option lit - detect if machine is used:
      if (users != NULL)
        {
          usageStatus = getUsageInfo(users, useArr, &size);
          if ((usageStatus == UB_STAT_GRID_USE) ||
              (usageStatus == UB_STAT_LOCAL_USE))
            {            
              size = 0; 
              while ((size < useArr[0].uidNum)  && (useArr[0].uid[size] != uid))
                size++;
            }
        }
      
      //draw load bar itsef
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0); 
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
      
      //add bottom options
      if ((((base_row - min_row) * sourceVal) / maxVal > 1) && 
          (MSX_IS_GRID_PRIO(priority))) 
        display_bar(graph, base_row, col, 1,
                    'G', &(pConfigurator->Colors._loadCaption), (width != 1));
      if ((users != NULL) && (size < useArr[0].uidNum))
        display_bar(graph, base_row - 1, col, 1,
                    'U', &(pConfigurator->Colors._loadCaption), 
                    (((base_row - min_row) * sourceVal) / maxVal > 1));
    }
}

int load_text_info(void *source, char *buff, int *size, int width) {
  int maxSize = *size;
  float loadVal = *((float*)source);
  *size = sprintf(buff, "%.2f", loadVal);
  return *size;
}


double load_scalar_div_x (const void* item, double x)
{
  return (double)(*((float*)item)) / x;
}

mon_display_module_t load_mod_info = {
 md_name:         "load",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Load", 
 md_titlength:    4,             
 md_shortTitle:   "Load",
 md_format:       "%5.2f", 
 md_key:          'l',
 md_length_func:  load_get_length, 
 md_scalar_func:  load_scalar_div_x, 
 md_new_func:     load_new_item, 
 md_del_func:     load_del_item,  
 md_disp_func:    load_display_item, 
 md_help_func:    load_display_help,
 md_text_info_func: load_text_info,
 md_side_window_display: 0
};

/*     {DISP_LOAD,              1, 1, "Load", 4,             "%5.2f", 'l', */
/*      load_get_length, load_scalar_div_x, load_new_item, load_del_item,   */
/*      load_display_item, load_display_help }, */



/****************************************************************************
 *                         
 *                          DISP_LOAD_VAL MODULE                      
 *
 ***************************************************************************/

char* load_val_desc[] = 
  { 
    " Load Value",
    (char*)NULL, };

int load_val_get_length ()
{
  //we hold both the load value and the priority value
  return sizeof(float);
}


char** load_val_display_help ()
{
  return load_val_desc;
}

void load_val_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if (iMap->load)
    {
      *((float*)dest) = 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                           + iMap->load->offset));
      *((float*)dest) *= 
        (float)(*(float*)(setup->yardstick) / 
                        (float)MOSIX_STD_SPEED);
      *((float*)dest) /= 100;
    }
}

void load_val_del_item (void* address) { return; }

void load_val_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  float sourceVal = *((float*)source);
  char loadStr[15]; 
  int offset = 0; //offset from bottom axis

  if (base_row - min_row > 6)
    offset += 4;
  
  sprintf(loadStr, "%.2f", sourceVal);  
  display_rtr(graph,  &(pConfigurator->Colors._loadCaption),
              base_row - offset, col, loadStr, strlen(loadStr), 0);  
}

double load_val_scalar_div_x (const void* item, double x)
{
  return (double)(*((float*)item)) / x;
}

mon_display_module_t load_val_mod_info = {
 md_name:         "load-val",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Load Values", 
 md_shortTitle:   "Load",
 md_titlength:    11,             
 md_format:       "%5.2f", 
 md_key:          'L',
 md_length_func:  load_val_get_length, 
 md_scalar_func:  load_val_scalar_div_x, 
 md_new_func:     load_val_new_item, 
 md_del_func:     load_val_del_item,  
 md_disp_func:    load_val_display_item, 
 md_help_func:    load_val_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0
};
/*     {DISP_LOAD_VAL,              1, 1, "Load Values", 11, "%5.2f", 'L', */
/*      load_val_get_length, load_val_scalar_div_x,  */
/*      load_val_new_item, load_val_del_item,  */
/*      load_val_display_item, load_val_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_FROZEN MODULE                      
 *
 ***************************************************************************/

char* frozen_desc[] = {
  " Frozen processes load",
  (char*)NULL, };

int frozen_get_length ()
{
  return sizeof(float);
}


char** frozen_display_help ()
{
  return frozen_desc;
}

void frozen_new_item (mmon_info_mapping_t* iMap, const void* source, 
                      void* dest, settings_t* setup)
{
  if (iMap->frozen)
    {
         float frozen = (float) (*((unsigned short*)(((idata_entry_t*)source)->data->data 
                                                     + iMap->frozen->offset)));
         unsigned char ncpus = *((unsigned char*)(((idata_entry_t*)source)->data->data 
                                                  + iMap->ncpus->offset));
         
         if (ncpus > 0)
              frozen /= ncpus;
         *((float*)dest) = frozen;
         if(dbg_flg)
              fprintf(dbg_fp, "Frozen = %.3f  ncpuse %hhd\n", frozen, ncpus);
         
    }
}

void frozen_del_item (void* address) { }

void frozen_display_item (WINDOW* graph, Configurator* pConfigurator, 
                          const void* source, int base_row, 
                          int min_row, int col, const double max, int width)
{
  float sourceVal = *((float*)source);
  float maxVal =  (float)max;

  if(dbg_flg)
       fprintf(dbg_fp, "Frozen = %.3f  Max %.3f\n", sourceVal, maxVal);
  

  if (maxVal > 0)
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._frozenCaption), 0);
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._frozenCaption), 1);
    }
}

double frozen_scalar_div_x (const void* item, double x)
{
  return (double)(*((float*)item)) / x;
}

mon_display_module_t frozen_mod_info = {
 md_name:         "frozen",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Frozen", 
 md_titlength:    6,             
 md_shortTitle:   "Frozen",
 md_format:       "%5.0f", 
 md_key:          'l',
 md_length_func:  frozen_get_length, 
 md_scalar_func:  frozen_scalar_div_x, 
 md_new_func:     frozen_new_item, 
 md_del_func:     frozen_del_item,  
 md_disp_func:    frozen_display_item, 
 md_help_func:    frozen_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0
};

/*     {DISP_FROZEN,            1, 1, "Frozen", 6,           "%5.0f", 'l', */
/*      frozen_get_length, frozen_scalar_div_x,  */
/*      frozen_new_item, frozen_del_item,   */
/*      frozen_display_item, frozen_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_LOAD2 MODULE
 *
 ***************************************************************************/

char* load2_desc[] = {
  " Load of nodes toghether with frozen processes",
  (char*)NULL, };

int load2_get_length ()
{
  //we hold both the load value and the priority value
  return 2*sizeof(float) + sizeof(unsigned short);
}

char** load2_display_help ()
{
  return load2_desc;
}

void load2_new_item (mmon_info_mapping_t* iMap, const void* source, 
                     void* dest, settings_t* setup)
{
     if ((iMap->load) && (iMap->frozen) && (iMap->ncpus)) {
          //load
          float load =  *((unsigned int*)(((idata_entry_t*)source)->data->data 
                                           + iMap->load->offset));
          
          
          load  *=  (float)(*(float*)(setup->yardstick) / (float)MOSIX_STD_SPEED);
          load  /= 100;
      *((float*)dest + 1) = load;

      //frozen
      float frozen = (float) (*((unsigned short*)(((idata_entry_t*)source)->data->data 
                               + iMap->frozen->offset)));

      unsigned char ncpus = *((unsigned char*)(((idata_entry_t*)source)->data->data 
                                               + iMap->ncpus->offset));

     
      if (ncpus > 0)
           frozen = load + frozen/ncpus;   
      else
           frozen += load;
      *((float*)dest) = frozen;

      //priority
      *((unsigned short*)(dest + 2 * sizeof(float))) = 
           *((unsigned short*)(((idata_entry_t*)source)->data->data 
                               + iMap->priority->offset));
     }
}

void load2_del_item (void* address) { return; }

void load2_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  float sourceVal = *((float*)source + 1); //L
  float sourceTotal = *((float*)source); //F + L
  float maxVal = (float)max;
  unsigned long priority = *((unsigned short*)(source + 
                             2 * sizeof(float)));


  if (maxVal > 0) 
    {
      if (width == 1)
        display_part_bar(graph, base_row, col, 
                         //FROZEN + LOAD
                         ((base_row - min_row) * sourceTotal) / maxVal,
                         '|', &(pConfigurator->Colors._frozenCaption), 0,
                         //LOAD
                         ((base_row - min_row) * sourceVal) / maxVal,
                         '|', &(pConfigurator->Colors._loadCaption), 0);
      else
        display_part_bar(graph, base_row, col, 
                         //FROZEN + LOAD
                         ((base_row - min_row) * sourceTotal) / maxVal,
                         'f', &(pConfigurator->Colors._frozenCaption), 1,
                         //LOAD
                         ((base_row - min_row) * sourceVal) / maxVal,
                         ' ', &(pConfigurator->Colors._loadCaption), 1);
      
      if ((((base_row - min_row) * sourceVal) / maxVal > 1) && 
          (MSX_IS_GRID_PRIO(priority))) 
        display_bar(graph, base_row, col, 1,
                    'G', &(pConfigurator->Colors._loadCaption), 1); 
    }
}

double load2_scalar_div_x (const void* item, double x)
{
  return (double)(*((float*)item)) / x;
}

mon_display_module_t load2_mod_info = {
 md_name:         "f+l",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "F + L", 
 md_titlength:    2,             
 md_shortTitle:   "F + L",
 md_format:       "%5.2f", 
 md_key:          'l',
 md_length_func:  load2_get_length, 
 md_scalar_func:  load2_scalar_div_x, 
 md_new_func:     load2_new_item, 
 md_del_func:     load2_del_item,  
 md_disp_func:    load2_display_item, 
 md_help_func:    load2_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0
};

/*     {DISP_LOAD2,             1, 1, "F + L", 5,            "%5.2f", 'l', */
/*      load2_get_length, load2_scalar_div_x,  */
/*      load2_new_item, load2_del_item,   */
/*      load2_display_item, load2_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_MEM MODULE                      
 *
 ***************************************************************************/

char* mem_desc[] = 
  { 
    " Used Memory; Swap Memory; Disk Memory, Freeze Space ",
    (char*)NULL, };

int mem_get_length ()
{
  return 2 * sizeof(float);
  //this one actually holds two adjecent longs - the actual and total
  //values. The total goes first (to the left).
}

char** mem_display_help ()
{
  return mem_desc;
}

void mem_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
     if ((iMap->tmem) && (iMap->freepages))
     {
          //total
          float totalMem = *((unsigned long*)(((idata_entry_t*)source)->data->data 
                                              + iMap->tmem->offset));
          totalMem /= units_per_mb;
          *((float*)dest) = totalMem;
          
          //used
          float freeMem = *((unsigned long*)(((idata_entry_t*)source)->data->data +
                                             iMap->freepages->offset));
          freeMem /= units_per_mb;
          *((float*)dest + 1) = freeMem;
          
          // Calculating the used memory from the total and free
          *((float*)dest + 1) = *((float*)dest)
               - *((float*)dest + 1);
     }
}

void mem_del_item (void* address) { return; }

void mem_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  float sourceVal = *((float*)source + 1);
  float maxVal =  (float)max;
  float sourceTotal = *((float*)source);

  if (maxVal > 0) 
    {
      if (width == 1)
        display_part_bar (graph, base_row, col, 
                          //total
                          ((base_row - min_row) * sourceTotal) / maxVal,
                          pConfigurator->Appearance._memswapGraphChar, 
                          &(pConfigurator->Colors._swapCaption), 0,
                          //used
                          ((base_row - min_row) * sourceVal) / maxVal,
                          '|', &(pConfigurator->Colors._swapCaption), 0);
      else
        display_part_bar (graph, base_row, col, 
                          //total
                          ((base_row - min_row) * sourceTotal) / maxVal,
                          pConfigurator->Appearance._memswapGraphChar, 
                          &(pConfigurator->Colors._swapCaption), 0,
                          //used
                          ((base_row - min_row) * sourceVal) / maxVal,
                          ' ', &(pConfigurator->Colors._swapCaption), 1);
    }
}

int mem_text_info(void *source, char *buff, int *size, int width) {
  int maxSize = *size;
  float memUsed = *((float*)source + 1);
  float memTotal = *((float*)source);

  // We normalize to GB
  memUsed /= 1024.0;
  memTotal /= 1024.0;

  *size = sprintf(buff, "%.2f/%.2f", memUsed, memTotal);
  return *size;
}



double mem_scalar_div_x (const void* item, double x)
{
  return (double)(*((float*)item)) / x;
  //we return the maximal total value
}

mon_display_module_t mem_mod_info = {
 md_name:         "mem",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Used Memory MB", 
 md_titlength:    14,             
 md_shortTitle:   "Mem",
 md_format:       "%5.0f", 
 md_key:          'm',
 md_length_func:  mem_get_length, 
 md_scalar_func:  mem_scalar_div_x, 
 md_new_func:     mem_new_item, 
 md_del_func:     mem_del_item,  
 md_disp_func:    mem_display_item, 
 md_help_func:    mem_display_help,
 md_text_info_func: mem_text_info,
 md_side_window_display: 0
 
};


/*     {DISP_MEM,              1, 1, "Used Memory MB", 14,   "%5.0f", 'm', */
/*      mem_get_length, mem_scalar_div_x, mem_new_item, mem_del_item,   */
/*      mem_display_item, mem_display_help }, */

/****************************************************************************
 *                         
 *                          DISP_SWAP MODULE                      
 *
 ***************************************************************************/

char* swap_desc[] = {
  " Swap space",
  (char*)NULL, };

int swap_get_length ()
{
  return 2 * sizeof(float);
  //this one actually holds two adjecent longs - the actual and total
  //values. The total goes first (to the left).
}

char** swap_display_help ()
{
  return swap_desc;
}

void swap_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if ((iMap->tswap) && (iMap->fswap))
    {
      //total
      *((float*)dest) = 
        *((unsigned long*)(((idata_entry_t*)source)->data->data 
                           + iMap->tswap->offset));
      *((float*)dest) /= units_per_mb;
      
      //used
      *((float*)dest + 1) = 
        *((unsigned long*)(((idata_entry_t*)source)->data->data 
                           + iMap->fswap->offset));
      *((float*)dest + 1) /= units_per_mb;
      *((float*)dest + 1) = *((float*)dest)
        - *((float*)dest + 1);      
    }
}

void swap_del_item (void* address) { return; }

void swap_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  float sourceVal = *((float*)source + 1);
  float maxVal =  (float)max;
  float sourceTotal = *((float*)source);

  if (maxVal > 0) 
    {
      if (width == 1)
        display_part_bar (graph, base_row, col, 
                          //total
                          ((base_row - min_row) * sourceTotal) / maxVal,
                          pConfigurator->Appearance._memswapGraphChar, 
                          &(pConfigurator->Colors._swapCaption), 0,
                          //used
                          ((base_row - min_row) * sourceVal) / maxVal,
                          '|', &(pConfigurator->Colors._swapCaption), 0);
      else
        display_part_bar (graph, base_row, col, 
                          //total
                          ((base_row - min_row) * sourceTotal) / maxVal,
                          pConfigurator->Appearance._memswapGraphChar, 
                          &(pConfigurator->Colors._swapCaption), 0,
                          //used
                          ((base_row - min_row) * sourceVal) / maxVal,
                          ' ', &(pConfigurator->Colors._swapCaption), 1);
    }
}

int swap_text_info(void *source, char *buff, int *size, int width) {
  int maxSize = *size;
  float swapUsed = *((float*)source + 1);
  float swapTotal = *((float*)source);

  // We normalize to GB
  swapUsed /= 1024.0;
  swapTotal /= 1024.0;

  *size = sprintf(buff, "%.2f/%.2f", swapUsed, swapTotal);
  return *size;
}



double swap_scalar_div_x (const void* item, double x)
{
  return (double)(*((float*)item)) / x;
  //we return the maximal total value
}


mon_display_module_t swap_mod_info = {
 md_name:         "swap",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Used Swap GB", 
 md_titlength:    12,             
 md_shortTitle:   "Swap",
 md_format:       "%5.0f", 
 md_key:          'm',
 md_length_func:  swap_get_length, 
 md_scalar_func:  swap_scalar_div_x, 
 md_new_func:     swap_new_item, 
 md_del_func:     swap_del_item,  
 md_disp_func:    swap_display_item, 
 md_help_func:    swap_display_help,
 md_text_info_func: swap_text_info,
 md_side_window_display: 0

};

/*     {DISP_SWAP,              1, 1, "Used Swap GB", 12,    "%5.0f", 'm', */
/*      swap_get_length, swap_scalar_div_x, swap_new_item, swap_del_item,   */
/*      swap_display_item, swap_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_DISK MODULE                      
 *
 ***************************************************************************/

char* disk_desc[] = {
  " Disk space (of monitored directories)",
  (char*)NULL, };

int disk_get_length ()
{
  return 2 * sizeof(float);
  //this one actually holds two adjecent longs - the actual and total
  //values. The total goes first (to the left).
}


char** disk_display_help ()
{
  return disk_desc;
}

void disk_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if ((iMap->tdisk) && (iMap->fdisk))
    {
      //total
      *((float*)dest) = 
        *((unsigned long*)(((idata_entry_t*)source)->data->data 
                           + iMap->tdisk->offset));
      *((float*)dest) /= units_per_mb * 1024;
      
      //used
      *((float*)dest + 1) = 
        *((unsigned long*)(((idata_entry_t*)source)->data->data 
                           + iMap->fdisk->offset));
      *((float*)dest + 1) /= units_per_mb * 1024;
      *((float*)dest + 1) = *((float*)dest)
        - *((float*)dest + 1);      
    }
}

void disk_del_item (void* address) { return; }

void disk_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  float sourceVal = *((float*)source + 1);
  float maxVal =  (float)max;
  float sourceTotal = *((float*)source);

  if (maxVal > 0)
    { 
      if (width == 1)
        display_part_bar (graph, base_row, col, 
                          //total
                          ((base_row - min_row) * sourceTotal) / maxVal,
                          pConfigurator->Appearance._memswapGraphChar, 
                          &(pConfigurator->Colors._swapCaption), 0,
                          //used
                          ((base_row - min_row) * sourceVal) / maxVal,
                          '|', &(pConfigurator->Colors._swapCaption), 0);
      else 
        display_part_bar (graph, base_row, col, 
                          //total
                          ((base_row - min_row) * sourceTotal) / maxVal,
                          pConfigurator->Appearance._memswapGraphChar, 
                          &(pConfigurator->Colors._swapCaption), 0,
                          //used
                          ((base_row - min_row) * sourceVal) / maxVal,
                          ' ', &(pConfigurator->Colors._swapCaption), 1);
    }
}

double disk_scalar_div_x (const void* item, double x)
{
    return (double)(*((float*)item)) / x;
  //we return the maximal total value
}


mon_display_module_t disk_mod_info = {
 md_name:         "disk",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Used Disk GB", 
 md_shortTitle:   "Disk",
 md_titlength:    12,             
 md_format:       "%5.0f", 
 md_key:          'm',
 md_length_func:  disk_get_length, 
 md_scalar_func:  disk_scalar_div_x, 
 md_new_func:     disk_new_item, 
 md_del_func:     disk_del_item,  
 md_disp_func:    disk_display_item, 
 md_help_func:    disk_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_DISK,              1, 1, "Used Disk GB", 12,    "%5.0f", 'm', */
/*      disk_get_length, disk_scalar_div_x, disk_new_item, disk_del_item,   */
/*      disk_display_item, disk_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_FREEZE_SPACE MODULE                      
 *
 ***************************************************************************/

char* freeze_space_desc[] = {
  " Freeze space of node",
   (char*)NULL, };

int freeze_space_get_length ()
{
  return sizeof(char*);
}

char** freeze_space_display_help ()
{
  return disk_desc;
}

void freeze_space_new_item (mmon_info_mapping_t* iMap, const void* source,
                            void* dest, settings_t* setup)
{
  getVlenItem(((idata_entry_t*)source)->data, 
              iMap->freeze_info, (char**)dest);
}

void freeze_space_del_item (void* address)
{
  if (*((char**)address))
    free(*((char**)address)); 
}

void freeze_space_display_item (WINDOW* graph, Configurator* pConfigurator,
                        const void* source, int base_row,
                        int min_row, int col, const double max, int width)
{
  freeze_info_status_t status;
  int                  len = 0, res;
  freeze_info_t        fi;
  char                 bar_str[base_row - min_row];
  ColorDescriptor      *pDesc = &(pConfigurator->Colors._errorStr);
  float                sourceVal;
  float                maxVal;
  float                sourceTotal;
        
  if(*((char**)source)) 
    {
      res = getFreezeInfo(*((char**)source), &fi);
      if(!res)
        status = FRZINF_ERR;
      else
        {
          status = fi.status;
          sourceTotal = (float)fi.total_mb/1024.0;
          sourceVal = sourceTotal - (float)fi.free_mb/1024.0;
        }         
    
      pDesc =  &(pConfigurator->Colors._fairShareInfo);
      switch(status) 
        {
        case FRZINF_OK:
          maxVal =  (float)max;
          if (maxVal > 0)
            {
              if (width == 1)
                display_part_bar (graph, base_row, col,
                                  //total
                                  ((base_row - min_row) * sourceTotal) / maxVal,
                                  pConfigurator->Appearance._memswapGraphChar,
                                  &(pConfigurator->Colors._memswapCaption), 0,
                                  //used
                                  ((base_row - min_row) * sourceVal) / maxVal,
                                  '|', &(pConfigurator->Colors._swapCaption), 0);
              else
                display_part_bar (graph, base_row, col,
                                  //total
                                  ((base_row - min_row) * sourceTotal) / maxVal,
                                  pConfigurator->Appearance._memswapGraphChar,
                                  &(pConfigurator->Colors._memswapCaption), 0,
                                  //used
                                  ((base_row - min_row) * sourceVal) / maxVal,
                                  ' ', &(pConfigurator->Colors._swapCaption), 1);
            }          
          return;
        case FRZINF_ERR:
          pDesc =  &(pConfigurator->Colors._errorStr);
        default:
          len = sprintf(bar_str, "%s", freezeInfoStatusStr(status));
        }
    }
  else // In case there is no info
    len = sprintf(bar_str, "N/A");
  
  display_rtr(graph, pDesc, base_row - 1, col, bar_str, len, 0); 
}

double freeze_space_scalar_div_x (const void* item, double x)
{
  freeze_info_t fi;
  if ((*((char**)item)) &&
      (getFreezeInfo(*((char**)item), &fi)))
    return (double)(fi.total_mb/1024.0) / x;
  //we return the maximal total value
  return 0;
}

mon_display_module_t freeze_space_mod_info = {
 md_name:         "freeze-space",
 md_iscalar:      1,
 md_canrev:       0, 
 md_title:        "Freeze Space GB", 
 md_titlength:    15,             
 md_shortTitle:   "Freeze",
 md_format:       "%5.0f", 
 md_key:          'm',
 md_length_func:  freeze_space_get_length, 
 md_scalar_func:  freeze_space_scalar_div_x, 
 md_new_func:     freeze_space_new_item, 
 md_del_func:     freeze_space_del_item,  
 md_disp_func:    freeze_space_display_item, 
 md_help_func:    freeze_space_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};

/*     {DISP_FREEZE_SPACE,      1, 0, "Freeze Space GB", 15, "%5.0f", 'm', */
/*      freeze_space_get_length, freeze_space_scalar_div_x,  */
/*      freeze_space_new_item, freeze_space_del_item,   */
/*      freeze_space_display_item, freeze_space_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_FREEZE_SPACE_VAL MODULE                      
 *
 ***************************************************************************/

char* freeze_space_val_desc[] =
  {
    " Freeze Space Value ",
    (char*)NULL, };

int freeze_space_val_get_length ()
{
  return sizeof(char*);
}

char** freeze_space_val_display_help ()
{
  return freeze_space_val_desc;
}

void freeze_space_val_new_item (mmon_info_mapping_t* iMap, const void* source,
                            void* dest, settings_t* setup)
{
  getVlenItem(((idata_entry_t*)source)->data, 
              iMap->freeze_info, (char**)dest); 
}

void freeze_space_val_del_item (void* address)
{
  if (*((char**)address))
    free(*((char**)address)); 
}

void freeze_space_val_display_item (WINDOW* graph, Configurator* pConfigurator,
                        const void* source, int base_row,
                        int min_row, int col, const double max, int width)
{
  freeze_info_status_t status;
  int                  len = 0, res;
  freeze_info_t        fi;
  char                 bar_str[base_row - min_row];
  char                 *ptr;
  ColorDescriptor      *pDesc = &(pConfigurator->Colors._errorStr);

  if(*((char**)source)) 
    {
      res = getFreezeInfo(*((char**)source), &fi);
      if(!res)
        status = FRZINF_ERR;
      else
        status = fi.status;
      
      pDesc =  &(pConfigurator->Colors._fairShareInfo);
      switch(status) 
        {
        case FRZINF_OK:
          ptr = bar_str;
          bar_str[0] = '\0';
          len = sprintf(ptr, "%6.1f - %6.1f ",
                        (float)fi.total_mb/1024.0, (float)fi.free_mb/1024.0);
          break;
        case FRZINF_ERR:
          pDesc =  &(pConfigurator->Colors._errorStr);
        default:
          len = sprintf(bar_str, "%s", freezeInfoStatusStr(status));
        }
    }
  else // In case there is no info
    len = sprintf(bar_str, "N/A");
  
  display_rtr(graph, pDesc, base_row - 1, col, bar_str, len, 0); 
}

double freeze_space_val_scalar_div_x (const void* item, double x)
{
  freeze_info_t fi;
  if ((*((char**)item)) &&
      (getFreezeInfo(*((char**)item), &fi)))
    return (double)(fi.total_mb/1024.0) / x;
  //we return the maximal total value
  return 0;
}


mon_display_module_t freeze_space_val_mod_info = {
 md_name:         "freeze-space-val",
 md_iscalar:      1,
 md_canrev:       0, 
 md_title:        "Freeze Space GB", 
 md_titlength:    15,             
 md_shortTitle:   "Freeze",
 md_format:       "%5.0f", 
 md_key:          'M',
 md_length_func:  freeze_space_val_get_length, 
 md_scalar_func:  freeze_space_val_scalar_div_x, 
 md_new_func:     freeze_space_val_new_item, 
 md_del_func:     freeze_space_val_del_item,  
 md_disp_func:    freeze_space_val_display_item, 
 md_help_func:    freeze_space_val_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};

/*     {DISP_FREEZE_SPACE_VAL,  1, 0, "Freeze Space GB", 15, "%5.0f", 'M', */
/*      freeze_space_val_get_length, freeze_space_val_scalar_div_x,  */
/*      freeze_space_val_new_item, freeze_space_val_del_item,   */
/*      freeze_space_val_display_item, freeze_space_val_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_GRID MODULE                      
 *
 ***************************************************************************/

char* grid_desc[] = 
  { 
    " Grid Info",
    (char*)NULL, };

int grid_get_length ()
{
  return 3 * sizeof(int) + sizeof(unsigned short);
}

char** grid_display_help ()
{
  return grid_desc;
}

void grid_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if(iMap->locals)
    *((int*)dest) = 
      *((int*)(((idata_entry_t*)source)->data->data 
               + iMap->locals->offset));
  
  if(iMap->guests)
    *((int*)dest + 1) =       
      *((int*)(((idata_entry_t*)source)->data->data 
               + iMap->guests->offset));
  
  if(iMap->maxguests)
    *((int*)dest + 2) = 
      *((int*)(((idata_entry_t*)source)->data->data 
               + iMap->maxguests->offset));
  
  if(iMap->priority) 
    *((unsigned short*)(dest + 3 * sizeof(int))) = 
      *((int*)(((idata_entry_t*)source)->data->data 
               + iMap->priority->offset));
}

void grid_del_item (void* addess) { }

void grid_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  char gridStr[40];
  char prioStr[10];
  int  gridStrLen = 0;
	
  if(*((unsigned short*)(source + 3*sizeof(int))) 
     == MSX_AVAIL_NODE_PRI) 
    sprintf(prioStr, "++++ ");
  else 
    sprintf(prioStr, "%-5d", 
            *((unsigned short*)(source + 3*sizeof(int))));
  sprintf(gridStr,"P%s G%2d-%-2d L%-3d",
          prioStr,
          *((int*)source + 2),
          *((int*)source + 1),
          *((int*)source));
  gridStrLen = 18;
    
  display_rtr(graph, &(pConfigurator->Colors._gridCaption), 
              base_row , col, gridStr, gridStrLen, 0);
}

mon_display_module_t grid_mod_info = {
 md_name:         "grid",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Grid Info", 
 md_titlength:    9,             
 md_shortTitle:   "Grid",
 md_format:       "", 
 md_key:          'g',
 md_length_func:  grid_get_length, 
 md_scalar_func:  NULL, 
 md_new_func:     grid_new_item, 
 md_del_func:     grid_del_item,  
 md_disp_func:    grid_display_item, 
 md_help_func:    grid_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_GRID,              0, 0, "Grid Info", 9,        "", 'g', */
/*      grid_get_length, NULL, grid_new_item, grid_del_item,   */
/*      grid_display_item, grid_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_TOKEN MODULE                      
 *
 ***************************************************************************/

char* token_desc[] = {
  " Token takens by queue managers",
  (char*)NULL, };

int token_get_length ()
{
  return 2 * sizeof(unsigned short);
}

char** token_display_help ()
{
  return token_desc;
}

void token_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if (iMap->status)
    *((unsigned short*)dest) = 
      *((unsigned short*)(((idata_entry_t*)source)->data->data 
                          + iMap->status->offset));
  
  if (iMap->status)
    *((unsigned short*)dest + 1) = 
      *((unsigned short*)(((idata_entry_t*)source)->data->data 
                          + iMap->token_level->offset));
}

void token_del_item (void* addess) { return; }

void token_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  char tokenStr[40];
  int  tokenStrLen = 0;
  
  if(*((unsigned short*)source) & MSX_STATUS_TOKEN) 
    {
      if(*((unsigned short*)source + 1) == 0) 
        {
          sprintf(tokenStr, "  C      ");
          tokenStrLen = 9;
        }
      else 
        {
          sprintf(tokenStr, "  G %-5d", *((unsigned short*)source + 1));
          tokenStrLen = 9;
        }
    }
  else 
    {
      sprintf(tokenStr, "         ");
      tokenStrLen = 9;
    }
  
  display_rtr(graph, &(pConfigurator->Colors._gridCaption), 
              base_row, col, tokenStr, tokenStrLen, 0);
}

mon_display_module_t token_mod_info = {
 md_name:         "token",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Tokens Given", 
 md_titlength:    12,             
 md_shortTitle:   "Tokens",
 md_format:       "", 
 md_key:          'g',
 md_length_func:  token_get_length, 
 md_scalar_func:  NULL, 
 md_new_func:     token_new_item, 
 md_del_func:     token_del_item,  
 md_disp_func:    token_display_item, 
 md_help_func:    token_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_TOKEN,             0, 0, "Token Given", 11,    "", 'g', */
/*      token_get_length, NULL, token_new_item, token_del_item,   */
/*      token_display_item, token_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_SPEED MODULE                      
 *
 ***************************************************************************/

char* speed_desc[] = 
  { 
    " Speed",
    (char*)NULL, };

int speed_get_length ()
{
  return sizeof(float) + sizeof(unsigned char);
}

char** speed_display_help ()
{
  return speed_desc;
}

void speed_new_item (mmon_info_mapping_t* iMap, const void* source, 
                     void* dest, settings_t* setup)
{
  if (iMap->speed)
    *((float*)dest) = 
      *((unsigned int*)(((idata_entry_t*)source)->data->data 
                         + iMap->speed->offset));
  if (iMap->ncpus)
    *((unsigned char*)(dest + sizeof(float))) =
    *((unsigned char*)(((idata_entry_t*)source)->data->data 
                       + iMap->ncpus->offset));
}

void speed_del_item (void* address) { return; }

void speed_display_item (WINDOW* graph, Configurator* pConfigurator, 
                         const void* source, int base_row, 
                         int min_row, int col, const double max, int width)
{
  float sourceVal = *((float*)source);
  float maxVal = (float)max;
  unsigned char nCPUs = 
    *((unsigned char*)(source + sizeof(float)));
  
  // Testing number of cpus >= 10;
  //if(sourceVal > maxVal - 1) {
  //     nCPUs = 16;
  //}

  int offset = 1;
  //if(nCPUs > 10) offset++;
  //if(nCPUs > 100) offset++;

  char ncpusStr[15];
  sprintf(ncpusStr, "%d", nCPUs);

  if (maxVal > 0) {
       display_bar(graph, base_row, col,
                   ((base_row - min_row) * sourceVal) / maxVal,
                   ' ', &(pConfigurator->Colors._speedCaption), 0);  

       // now printing the number of cpus 2 places above bottom
       if (((base_row - min_row) * sourceVal) / maxVal > 1) {
            display_rtr(graph, &(pConfigurator->Colors._speedCaption),
                        base_row - offset, col, ncpusStr, strlen(ncpusStr), 0);
            
       }
  }
}

int speed_text_info(void *source, char *buff, int *size, int width) {
  int maxSize = *size;
  float speedVal =  *((float*)source);

  *size = sprintf(buff, "%.1f", speedVal);
  return *size;
}


double speed_scalar_div_x (const void* item, double x)
{
  return (double)(*((float*)item)) / x;
}

mon_display_module_t speed_mod_info = {
 md_name:         "speed",
 md_iscalar:      1,
 md_canrev:       0, 
 md_title:        "Speed", 
 md_titlength:    5,             
 md_shortTitle:   "Speed",
 md_format:       "%5.2f", 
 md_key:          's',
 md_length_func:  speed_get_length, 
 md_scalar_func:  speed_scalar_div_x, 
 md_new_func:     speed_new_item, 
 md_del_func:     speed_del_item,  
 md_disp_func:    speed_display_item, 
 md_help_func:    speed_display_help,
 md_text_info_func: speed_text_info,
 md_side_window_display: 0
};
/*     {DISP_SPEED,             1, 0, "Speed", 5,           "%5.2f", 's', */
/*      speed_get_length, speed_scalar_div_x,  */
/*      speed_new_item, speed_del_item,   */
/*      speed_display_item, speed_display_help }, */





/****************************************************************************
 *                         
 *                          DISP_NCPUS MODULE                      
 *
 ***************************************************************************/

char* ncpus_desc[] = 
  { 
    " NCPUS",
    (char*)NULL, };

int ncpus_get_length ()
{
  return sizeof(unsigned char);
}

char** ncpus_display_help ()
{
  return speed_desc;
}

void ncpus_new_item (mmon_info_mapping_t* iMap, const void* source, 
                     void* dest, settings_t* setup)
{

  if (iMap->ncpus)
    *((unsigned char*)(dest)) =
    *((unsigned char*)(((idata_entry_t*)source)->data->data 
                       + iMap->ncpus->offset));
}

void ncpus_del_item (void* address) { return; }

void ncpus_display_item (WINDOW* graph, Configurator* pConfigurator, 
                         const void* source, int base_row, 
                         int min_row, int col, const double max, int width)
{

  return;
}

int ncpus_text_info(void *source, char *buff, int *size, int width) {
  int maxSize = *size;
  float speedVal =  *((float*)source);

  unsigned char nCPUs =  *((unsigned char*)(source));

  *size = sprintf(buff, "%d", nCPUs);
  return *size;
}


double ncpus_scalar_div_x (const void* item, double x)
{
  return (double)(*((float*)item)) / x;
}

mon_display_module_t ncpus_mod_info = {
 md_name:         "ncpus",
 md_iscalar:      1,
 md_canrev:       0, 
 md_title:        "NCPUs", 
 md_titlength:    5,             
 md_shortTitle:   "CPUs",
 md_format:       "%5.2f", 
 md_key:          '!',
 md_length_func:  ncpus_get_length, 
 md_scalar_func:  ncpus_scalar_div_x, 
 md_new_func:     ncpus_new_item, 
 md_del_func:     ncpus_del_item,  
 md_disp_func:    ncpus_display_item, 
 md_help_func:    ncpus_display_help,
 md_text_info_func: ncpus_text_info,
 md_side_window_display: 0

};



/****************************************************************************
 *                         
 *                          DISP_UTIL MODULE                      
 *
 ***************************************************************************/

char* util_desc[] = 
  { 
    " Utilization ",
    (char*)NULL, };

int util_get_length ()
{
  return sizeof(unsigned char);
}

char** util_display_help ()
{
  return util_desc;
}

void util_new_item (mmon_info_mapping_t* iMap, const void* source, 
                     void* dest, settings_t* setup)
{
  if (iMap->util)
    *((unsigned char*)dest) = 
      *((unsigned char*)(((idata_entry_t*)source)->data->data 
                         + iMap->util->offset));
}

void util_del_item (void* address) { return; }

void util_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  int sourceVal = *((unsigned char*)source);
  int maxVal = (int)max;
  
  if (maxVal > 0) 
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0);  
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
    }
}

double util_scalar_div_x (const void* item, double x)
{
  return (double)(*((unsigned char*)item)) / x;
}

mon_display_module_t util_mod_info = {
 md_name:         "util",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Utilizability", 
 md_titlength:    13,             
 md_shortTitle:   "Util",
 md_format:       "%4.0f%%", 
 md_key:          'u',
 md_length_func:  util_get_length, 
 md_scalar_func:  util_scalar_div_x, 
 md_new_func:     util_new_item, 
 md_del_func:     util_del_item,  
 md_disp_func:    util_display_item, 
 md_help_func:    util_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};


/****************************************************************************
 *                         
 *                          IO Wait module                      
 *
 ***************************************************************************/

char* iowait_desc[] = 
  { 
    " I/O wait (%) ",
    (char*)NULL, };

int iowait_get_length ()
{
  return sizeof(unsigned char);
}

char** iowait_display_help ()
{
  return iowait_desc;
}

void iowait_new_item (mmon_info_mapping_t* iMap, const void* source, 
                     void* dest, settings_t* setup)
{
  if (iMap->util)
    *((unsigned char*)dest) = 
      *((unsigned char*)(((idata_entry_t*)source)->data->data 
                         + iMap->iowait->offset));
}

void iowait_del_item (void* address) { return; }

void iowait_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  int sourceVal = *((unsigned char*)source);
  int maxVal = (int)max;
  
  if (maxVal > 0) 
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0);  
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
    }
}

double iowait_scalar_div_x (const void* item, double x)
{
  return (double)(*((unsigned char*)item)) / x;
}


int iowait_text_info(void *source, char *buff, int *size, int width) {
  int maxSize = *size;
  int iowaitVal = *((unsigned char*)source);
 
  *size = sprintf(buff, "%d %%", iowaitVal);
  return *size;
}


mon_display_module_t iowait_mod_info = {
 md_name:         "iowait",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "% IO wait", 
 md_titlength:    7,             
 md_shortTitle:   "IOwait",
 md_format:       "%3.1f%%", 
 md_key:          'u',
 md_length_func:  iowait_get_length, 
 md_scalar_func:  iowait_scalar_div_x, 
 md_new_func:     iowait_new_item, 
 md_del_func:     iowait_del_item,  
 md_disp_func:    iowait_display_item, 
 md_help_func:    iowait_display_help,
 md_text_info_func: iowait_text_info,
 md_side_window_display: 0

};


/*     {DISP_UTIL,              1, 1, "Utilizability", 13,   "%4.0f%%", 'u', */
/*      util_get_length, util_scalar_div_x, util_new_item, util_del_item,   */
/*      util_display_item, util_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_RIO MODULE                      
 *
 ***************************************************************************/

char* rio_desc[] = 
  { 
    " Read rate of local disks",
    (char*)NULL, };

int rio_get_length ()
{
  return sizeof(unsigned int);
}

char** rio_display_help ()
{
  return rio_desc;
}

void rio_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if (iMap->disk_read_rate)
    {
      *((unsigned int*)dest) = 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                          + iMap->disk_read_rate->offset));
      *((unsigned int*)dest) /= 1024;
    }
}

void rio_del_item (void* address) { return; }

void rio_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  unsigned int sourceVal = *((unsigned int*)source);
  unsigned int maxVal = (unsigned int)max;
  
  if (maxVal > 0) 
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0); 
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
    }
}

double rio_scalar_div_x (const void* item, double x)
{
  return (double)(*((unsigned int*)item)) / x;
}


mon_display_module_t rio_mod_info = {
 md_name:         "rio",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "READ I/O MB/Sec", 
 md_titlength:    15,             
 md_format:       "%5.2f", 
 md_shortTitle:   "DiskRead",
 md_key:          'i',
 md_length_func:  rio_get_length, 
 md_scalar_func:  rio_scalar_div_x, 
 md_new_func:     rio_new_item, 
 md_del_func:     rio_del_item,  
 md_disp_func:    rio_display_item, 
 md_help_func:    rio_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_RIO,               1, 1, "READ I/O MB/Sec", 15,   "%5.2f", 'i', */
/*      rio_get_length, rio_scalar_div_x, rio_new_item, rio_del_item,   */
/*      rio_display_item, rio_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_WIO MODULE                      
 *
 ***************************************************************************/

char* wio_desc[] = {
  " Write rate of local disks",
   (char*)NULL, };

int wio_get_length ()
{
  return sizeof(unsigned int);
}

char** wio_display_help ()
{
  return wio_desc;
}

void wio_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if (iMap->disk_write_rate)
    {
      *((unsigned int*)dest) = 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                          + iMap->disk_write_rate->offset));
      *((unsigned int*)dest) /= 1024;
    }
}

void wio_del_item (void* address) { return; }

void wio_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  unsigned int sourceVal = *((unsigned int*)source);
  unsigned int maxVal = (unsigned int)max;
  
  if (maxVal > 0) 
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0); 
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
    }
}

double wio_scalar_div_x (const void* item, double x)
{
  return (double)(*((unsigned int*)item)) / x;
}

mon_display_module_t wio_mod_info = {
 md_name:         "wio",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "WRITE I/O MB/Sec", 
 md_titlength:    16,             
 md_shortTitle:   "DiskWrite",
 md_format:       "%5.2f", 
 md_key:          'i',
 md_length_func:  wio_get_length, 
 md_scalar_func:  wio_scalar_div_x, 
 md_new_func:     wio_new_item, 
 md_del_func:     wio_del_item,  
 md_disp_func:    wio_display_item, 
 md_help_func:    wio_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_WIO,               1, 1, "WRITE I/O MB/Sec", 16,   "%5.2f", 'i', */
/*      wio_get_length, wio_scalar_div_x, wio_new_item, wio_del_item,   */
/*      wio_display_item, wio_display_help }, */

/****************************************************************************
 *                         
 *                          DISP_TIO MODULE                      
 *
 ***************************************************************************/

char* tio_desc[] = {
  " Total I/O rate of local disks",
  (char*)NULL, };

int tio_get_length ()
{
  return sizeof(unsigned int);
}

char** tio_display_help ()
{
  return tio_desc;
}

void tio_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if ((iMap->disk_read_rate) &&
      (iMap->disk_write_rate))
    {
      *((unsigned int*)dest) = 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                          + iMap->disk_read_rate->offset));
      *((unsigned int*)dest) += 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                          + iMap->disk_write_rate->offset));
      *((unsigned int*)dest) /= 1024;
    }
}

void tio_del_item (void* address) { return; }

void tio_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  unsigned int sourceVal = *((unsigned int*)source);
  unsigned int maxVal = (unsigned int)max;
  
  if (maxVal > 0) 
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0); 
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
    } 
}

double tio_scalar_div_x (const void* item, double x)
{
  return (double)(*((unsigned int*)item)) / x;
}


mon_display_module_t tio_mod_info = {
 md_name:         "tio",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "TOTAL I/O MB/Sec", 
 md_titlength:    16,             
 md_shortTitle:   "DiskIO",
 md_format:       "%5.2f", 
 md_key:          'i',
 md_length_func:  tio_get_length, 
 md_scalar_func:  tio_scalar_div_x, 
 md_new_func:     tio_new_item, 
 md_del_func:     tio_del_item,  
 md_disp_func:    tio_display_item, 
 md_help_func:    tio_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_TIO,               1, 1, "TOTAL I/O MB/Sec", 16,   "%5.2f", 'i', */
/*      tio_get_length, tio_scalar_div_x, tio_new_item, tio_del_item,   */
/*      tio_display_item, tio_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_NET_IN MODULE                      
 *
 ***************************************************************************/

char* net_in_desc[] = 
  { 
    " Network In rate",
    (char*)NULL, };

int net_in_get_length ()
{
  return sizeof(unsigned int);
}

char** net_in_display_help ()
{
  return net_in_desc;
}

void net_in_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if (iMap->net_rx_rate)
    {
      *((unsigned int*)dest) = 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                          + iMap->net_rx_rate->offset));
      *((unsigned int*)dest) /= 1024;
    }
}

void net_in_del_item (void* address) { return; }

void net_in_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  unsigned int sourceVal = *((unsigned int*)source);
  unsigned int maxVal = (unsigned int)max;
  
  if (maxVal > 0) 
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0); 
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
    }
}

double net_in_scalar_div_x (const void* item, double x)
{
  return (double)(*((unsigned int*)item)) / x;
}

mon_display_module_t net_in_mod_info = {
 md_name:         "net-in",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Net IN Mbps", 
 md_titlength:    11,             
 md_shortTitle:   "Net-In",
 md_format:       "%5.2f", 
 md_key:          'o',
 md_length_func:  net_in_get_length, 
 md_scalar_func:  net_in_scalar_div_x, 
 md_new_func:     net_in_new_item, 
 md_del_func:     net_in_del_item,  
 md_disp_func:    net_in_display_item, 
 md_help_func:    net_in_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_NET_IN,               1, 1, "Net IN Mbps", 11,   "%5.2f", 'o', */
/*      net_in_get_length, net_in_scalar_div_x,  */
/*      net_in_new_item, net_in_del_item,   */
/*      net_in_display_item, net_in_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_NET_OUT MODULE                      
 *
 ***************************************************************************/

char* net_out_desc[] = {
  " Network OUT rate",
   (char*)NULL, };

int net_out_get_length ()
{
  return sizeof(unsigned int);
}

char** net_out_display_help ()
{
  return net_out_desc;
}

void net_out_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if (iMap->net_tx_rate)
    {
      *((unsigned int*)dest) = 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                          + iMap->net_tx_rate->offset));
      *((unsigned int*)dest) /= 1024;
    }
}

void net_out_del_item (void* address) { return; }

void net_out_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  unsigned int sourceVal = *((unsigned int*)source);
  unsigned int maxVal = (unsigned int)max;
  
  if (maxVal > 0) 
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0); 
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
    } 
}

double net_out_scalar_div_x (const void* item, double x)
{
  return (double)(*((unsigned int*)item)) / x;
}

mon_display_module_t net_out_mod_info = {
 md_name:         "net-out",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Net OUT Mbps", 
 md_titlength:    12,             
 md_shortTitle:   "Net-Out",
 md_format:       "%5.2f", 
 md_key:          'o',
 md_length_func:  net_out_get_length, 
 md_scalar_func:  net_out_scalar_div_x, 
 md_new_func:     net_out_new_item, 
 md_del_func:     net_out_del_item,  
 md_disp_func:    net_out_display_item, 
 md_help_func:    net_out_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_NET_OUT,               1, 1, "Net OUT Mbps", 12,   "%5.2f", 'o', */
/*      net_out_get_length, net_out_scalar_div_x,  */
/*      net_out_new_item, net_out_del_item,   */
/*      net_out_display_item, net_out_display_help }, */


/****************************************************************************
 *                         
 *                          DISP_NET_TOTAL MODULE                      
 *
 ***************************************************************************/

char* net_total_desc[] = {
  " Network total I/O rate",
   (char*)NULL, };

int net_total_get_length ()
{
  return sizeof(unsigned int);
}

char** net_total_display_help ()
{
  return net_total_desc;
}

void net_total_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if ((iMap->net_rx_rate) &&
      (iMap->net_tx_rate))
    {
      *((unsigned int*)dest) = 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                          + iMap->net_rx_rate->offset));
      *((unsigned int*)dest) += 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                          + iMap->net_tx_rate->offset));
      *((unsigned int*)dest) /= 1024;
    }
}

void net_total_del_item (void* address) { return; }

void net_total_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  unsigned int sourceVal = *((unsigned int*)source);
  unsigned int maxVal = (unsigned int)max;
  
  if (maxVal > 0) 
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0); 
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
    }  
}

double net_total_scalar_div_x (const void* item, double x)
{
  return (double)(*((unsigned int*)item)) / x;
}

mon_display_module_t net_total_mod_info = {
 md_name:         "net-total",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "Net TOTAL Mbps", 
 md_titlength:    14,             
 md_shortTitle:   "Net-IO",
 md_format:       "%5.2f", 
 md_key:          'o',
 md_length_func:  net_total_get_length, 
 md_scalar_func:  net_total_scalar_div_x, 
 md_new_func:     net_total_new_item, 
 md_del_func:     net_total_del_item,  
 md_disp_func:    net_total_display_item, 
 md_help_func:    net_total_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_NET_TOTAL,               1, 1, "Net TOTAL Mbps", 14,   "%5.2f", 'o', */
/*      net_total_get_length, net_total_scalar_div_x,  */
/*      net_total_new_item, net_total_del_item,   */
/*      net_total_display_item, net_total_display_help }, */

/****************************************************************************
 *                         
 *                          DISP_NFS_RPC MODULE                      
 *
 ***************************************************************************/

char* nfs_rpc_desc[] = {
  " NFS RPC operation rate",
   (char*)NULL, };

int nfs_rpc_get_length ()
{
  return sizeof(unsigned int);
}

char** nfs_rpc_display_help ()
{
  return nfs_rpc_desc;
}

void nfs_rpc_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  if (iMap->nfs_client_rpc_rate)
    {
      *((unsigned int*)dest) = 
        *((unsigned int*)(((idata_entry_t*)source)->data->data 
                          + iMap->nfs_client_rpc_rate->offset));
      *((unsigned int*)dest) /= 100;
    }
}

void nfs_rpc_del_item (void* address) { return; }

void nfs_rpc_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  unsigned int sourceVal = *((unsigned int*)source);
  unsigned int maxVal = (unsigned int)max;
  
  if (maxVal > 0) 
    {
      if (width == 1)
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    '|', &(pConfigurator->Colors._loadCaption), 0); 
      else
        display_bar(graph, base_row, col, 
                    ((base_row - min_row) * sourceVal) / maxVal,
                    ' ', &(pConfigurator->Colors._loadCaption), 1);  
    }
}

double nfs_rpc_scalar_div_x (const void* item, double x)
{
  return (double)(*((unsigned int*)item)) / x;
}


mon_display_module_t nfs_rpc_mod_info = {
 md_name:         "nfs-rpc",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "RPC HOps/sec", 
 md_titlength:    12,             
 md_shortTitle:   "RPC",
 md_format:       "%5.2f", 
 md_key:          'o',
 md_length_func:  nfs_rpc_get_length, 
 md_scalar_func:  nfs_rpc_scalar_div_x, 
 md_new_func:     nfs_rpc_new_item, 
 md_del_func:     nfs_rpc_del_item,  
 md_disp_func:    nfs_rpc_display_item, 
 md_help_func:    nfs_rpc_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_NFS_RPC,             1, 1, "RPC HOps/sec", 12,    "%5.2f", 'o', */
/*      nfs_rpc_get_length, nfs_rpc_scalar_div_x,  */
/*      nfs_rpc_new_item, nfs_rpc_del_item,   */
/*      nfs_rpc_display_item, nfs_rpc_display_help }, */



/****************************************************************************
 *                         
 *                          DISP_USING_CLUSTERS MODULE                      
 *
 ***************************************************************************/
 
char* using_clusters_desc[] = 
  { 
    " Using Clusters",
    (char*)NULL, };

int using_clusters_get_length ()
{
  return 2*sizeof(char*);
}

char** using_clusters_display_help ()
{
  return using_clusters_desc;
}

void using_clusters_new_item (mmon_info_mapping_t* iMap, const void* source, 
                              void* dest, settings_t* setup)
{
  getVlenItem(((idata_entry_t*)source)->data, 
              iMap->usedby, (char**)dest); 
  getVlenItem(((idata_entry_t*)source)->data, 
              iMap->proc_watch, (char**)dest + 1);
}

void using_clusters_del_item (void* address)
{
  if (*((char**)address))
    free(*((char**)address));
  if (*((char**)address + 1))
    free(*((char**)address + 1));  
}

void using_clusters_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const  void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  node_usage_status_t usageStatus;
  int j, len, size = 20;
  char bar_str[base_row - min_row];
  char* ptr_str;
  used_by_entry_t useArr[20];
  ColorDescriptor *pDesc;
        
  usageStatus = getUsageInfo(*((char**)source), useArr, &size);
  pDesc =  &(pConfigurator->Colors._fairShareInfo);
  switch(usageStatus) 
    {
    case UB_STAT_GRID_USE:
      ptr_str = bar_str;
      for(j=0, len=0; j< size ; j++) 
        {
          len += sprintf(ptr_str, "%s ", useArr[j].clusterName);
          ptr_str += len;
        }
      break;
    case UB_STAT_FREE:
      bar_str[0]='\0'; len = 0;
      break;
    case UB_STAT_ERROR:
      pDesc =  &(pConfigurator->Colors._errorStr);
      len = sprintf(bar_str, "%s", usageStatusStr(usageStatus));
      break;
    default:
      len = sprintf(bar_str, "%s", usageStatusStr(usageStatus));
    }
  
  addPriodMark(*((char**)source + 1), bar_str, &len);
  display_rtr(graph, pDesc, base_row - 1, col, bar_str, len, 0);
}


mon_display_module_t using_clusters_mod_info = {
 md_name:         "cluster",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Using Clusters", 
 md_titlength:    14,             
 md_shortTitle:   "UsingClusters",
 md_format:       "", 
 md_key:          'f',
 md_length_func:  using_clusters_get_length, 
 md_scalar_func:  NULL,
 md_new_func:     using_clusters_new_item, 
 md_del_func:     using_clusters_del_item,  
 md_disp_func:    using_clusters_display_item, 
 md_help_func:    using_clusters_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_USING_CLUSTERS,              0, 0, "Using Clusters", 14, "", 'f', */
/*      using_clusters_get_length, NULL,  */
/*      using_clusters_new_item, using_clusters_del_item,   */
/*      using_clusters_display_item, using_clusters_display_help }, */



/****************************************************************************
 *                         
 *                          DISP_USING_USERS MODULE                      
 *
 ***************************************************************************/
 
char* using_users_desc[] = {
  " Using Users",
  (char*)NULL, };

int using_users_get_length ()
{
  return 2*sizeof(char*);
}

char** using_users_display_help ()
{
  return using_users_desc;
}

void using_users_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  getVlenItem(((idata_entry_t*)source)->data, 
              iMap->usedby, (char**)dest); 
  getVlenItem(((idata_entry_t*)source)->data, 
              iMap->proc_watch, (char**)dest + 1); 
}

void using_users_del_item (void* address)
{
  if (*((char**)address))
    free(*((char**)address));
  if (*((char**)address + 1))
    free(*((char**)address + 1));  
}

void using_users_display_item (WINDOW* graph, Configurator* pConfigurator, 
                        const  void* source, int base_row, 
                        int min_row, int col, const double max, int width)
{
  int                  j;
  node_usage_status_t  usageStatus;
  int                  size = 20;
  int                  len;
  used_by_entry_t      useArr[20];
  char                 bar_str[base_row - min_row];
  char*                ptr_str;
  struct passwd       *pwEnt;
  ColorDescriptor     *pDesc;
  
  usageStatus = getUsageInfo(*((char**)source), useArr, &size);
  pDesc =  &(pConfigurator->Colors._fairShareInfo);
  switch(usageStatus) {
  case UB_STAT_GRID_USE:
  case UB_STAT_LOCAL_USE:
    ptr_str = bar_str;
    bar_str[0] = '\0';
                    
    for(j =0; j< useArr[0].uidNum ; j++) {
      pwEnt = getpwuid(useArr[0].uid[j]);
      if(pwEnt)
        ptr_str += sprintf(ptr_str, "%s ", pwEnt->pw_name);
      else
        ptr_str += sprintf(ptr_str, "%d ", useArr[0].uid[j]);
    }
    if(usageStatus == UB_STAT_GRID_USE)
      ptr_str += sprintf(ptr_str, "G");
    else
      ptr_str += sprintf(ptr_str, " ");
    len = strlen(bar_str);
    break;
  case UB_STAT_FREE:
    bar_str[0] = '\0';
    len = 0;
    break;
  case UB_STAT_ERROR:
    pDesc = &(pConfigurator->Colors._errorStr);
  default:
    len = sprintf(bar_str, "%s", usageStatusStr(usageStatus));
  }

  addPriodMark(*((char**)source + 1), bar_str, &len);
  display_rtr(graph, pDesc, base_row - 1, col, bar_str, len, 0);
}

mon_display_module_t using_users_mod_info = {
 md_name:         "users",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Using Users", 
 md_titlength:    11,             
 md_shortTitle:   "Users",
 md_format:       "", 
 md_key:          'f',
 md_length_func:  using_users_get_length, 
 md_scalar_func:  NULL,
 md_new_func:     using_users_new_item, 
 md_del_func:     using_users_del_item,  
 md_disp_func:    using_users_display_item, 
 md_help_func:    using_users_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_USING_USERS,             0, 0, "Using Users", 11,    "", 'f', */
/*      using_users_get_length, NULL,  */
/*      using_users_new_item, using_users_del_item,   */
/*      using_users_display_item, using_users_display_help }, */

/****************************************************************************
 *                         
 *                          DISP_INFOD_DEBUG MODULE                      
 *
 ***************************************************************************/

char* infod_debug_desc[] = 
  { 
    " Infod Debug Info ",
    (char*)NULL, };

int infod_debug_get_length ()
{
  return sizeof(char*);
}

char** infod_debug_display_help ()
{
  return infod_debug_desc;
}

void infod_debug_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  getVlenItem(((idata_entry_t*)source)->data, 
              iMap->infod_debug, (char**)dest); 
}

void infod_debug_del_item (void* address)
{
  if (*((char**)address))
    free(*((char**)address));
}

void infod_debug_display_item (WINDOW* graph, Configurator* pConfigurator, 
                               const  void* source, int base_row, 
                               int min_row, int col, const double max, int width)
{
  infod_debug_info_t  idi;
  int                 len = 0, res;
  char                uptimeStr[128];
  char                bar_str[base_row - min_row];
  char*               bar = bar_str;
  ColorDescriptor     *pDesc = &(pConfigurator->Colors._errorStr);
        
  if(*((char**)source)) {
    res = getInfodDebugInfo(*((char**)source), &idi);
    if(!res)
      len = sprintf(bar, "error");
    else 
      {
        if (idi.uptimeSec > 60*60*24)
          sprintf(uptimeStr, "%3d d", (int)(idi.uptimeSec / (60*60*24)));
        else if (idi.uptimeSec > 60*60)
          sprintf(uptimeStr, "%3d h", (int)(idi.uptimeSec / (60*60)));
        else if (idi.uptimeSec > 60)
          sprintf(uptimeStr, "%3d m", (int)(idi.uptimeSec / (60)));
        else
          sprintf(uptimeStr, "%3d s", (int)(idi.uptimeSec));
        len = sprintf(bar, "c %d  t%6s", idi.childNum, uptimeStr);
        pDesc =  &(pConfigurator->Colors._fairShareInfo);
      }
  }
  else 
    len = sprintf(bar_str, "N/A");
         
  display_rtr(graph, pDesc, base_row - 1, col, bar_str, len, 0);     
}


mon_display_module_t infod_debug_mod_info = {
 md_name:         "infod-debug",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Infod Debug", 
 md_titlength:    11,             
 md_shortTitle:   "Debug",
 md_format:       "", 
 md_key:          '!',
 md_length_func:  infod_debug_get_length, 
 md_scalar_func:  NULL,
 md_new_func:     infod_debug_new_item, 
 md_del_func:     infod_debug_del_item,  
 md_disp_func:    infod_debug_display_item, 
 md_help_func:    infod_debug_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_INFOD_DEBUG,             0, 0, "Infod Debug", 11,    "", '!', */
/*      infod_debug_get_length, NULL,  */
/*      infod_debug_new_item, infod_debug_del_item,   */
/*      infod_debug_display_item, infod_debug_display_help }, */

/****************************************************************************
 *                         
 *                          DISP_CLUSTER_ID MODULE                      
 *
 *
 * - This display type is actaully used only for the cluster id bottom line.
 *
 ***************************************************************************/

char* clsid[28] = {0}; //array for every letter A-Z

char* cluster_id_desc[] = 
  { (char*)NULL, };

int cluster_id_get_length ()
{
  return sizeof(char);
}

char** cluster_id_display_help ()
{
  return cluster_id_desc;
}

void cluster_id_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
  int index = 0;
  char* cluster_id = NULL;

  getVlenItem(((idata_entry_t*)source)->data, 
              iMap->cluster_id, &cluster_id);

  if (setup->clsinit)
    {
      for (int i = 0; i < 28; i++)
        if (clsid[i]) 
          free(clsid[i]); //free entry
      bzero(clsid,28);
    }
  setup->clsinit = false;
  
  while ((index < 28) &&
         (clsid[index] != 0) &&
         (strcmp(cluster_id, clsid[index]) != 0))
    index++;
  
  if (!clsid[index]) //allocate new entry
    clsid[index] = strdup(cluster_id);
  free(cluster_id);
    
  if (index < 28) //set id for this instance
    *((char*)dest) = 'A' + index;
  else 
    *((char*)dest) = '0';
}

void cluster_id_del_item (void* address) { }

void cluster_id_display_item (WINDOW* graph, Configurator* pConfigurator, 
                               const  void* source, int base_row, 
                               int min_row, int col, const double max, int width)
{ }

double cluster_id_scalar_div_x (const void* item, double x)
{
  if (*((char*)item))
    return (double)(*((char*)item));
  return (double)(' ');
}

    


mon_display_module_t cluster_id_mod_info = {
 md_name:         "cluster-id",
 md_iscalar:      1,
 md_canrev:       1, 
 md_title:        "", 
 md_titlength:    0,             
 md_shortTitle:   "",
 md_format:       "", 
 md_key:          'v',
 md_length_func:  cluster_id_get_length, 
 md_scalar_func:  cluster_id_scalar_div_x,
 md_new_func:     cluster_id_new_item, 
 md_del_func:     cluster_id_del_item,  
 md_disp_func:    cluster_id_display_item, 
 md_help_func:    cluster_id_display_help,
 md_text_info_func: NULL,
 md_side_window_display: 0

};
/*     {DISP_CLUSTER_ID,             1, 1, "", 0,    "", 'v', */
/*      cluster_id_get_length, cluster_id_scalar_div_x,  */
/*      cluster_id_new_item, cluster_id_del_item,   */
/*      cluster_id_display_item, cluster_id_display_help }, */




/*     {DISP_JMIG1,            0, 0, "JMigrate Info", 13,    "", '@', */
/*      jmig_get_length, NULL,  */
/*      jmig_new_item, jmig_del_item,   */
/*      jmig_display_item, jmig_display_help }, */

    


/*   }; */


