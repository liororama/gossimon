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
#include "EcoInfo.h"


int addAssigndMark(char* proc_watch, char *bar_str, int *len);
/****************************************************************************
 *                         
 *                          DISP_INFOD_DEBUG MODULE                      
 *
 ***************************************************************************/

char* eco5_info_desc[] = 
  { 
    " Display ecomomical parameters (testing) ",
    (char*)NULL, };

int eco5_info_get_length ()
{
  return 2*sizeof(char*);
}

char** eco5_info_display_help ()
{
  return eco5_info_desc;
}

void eco5_info_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
     getVlenItem(((idata_entry_t*)source)->data, 
                 iMap->eco_info, (char**)dest); 
     
     getVlenItem(((idata_entry_t*)source)->data, 
                 iMap->proc_watch, (char**)dest + 1);
}

void eco5_info_del_item (void* address)
{
     if (*((char**)address))
          free(*((char**)address));
     
     if (*((char**)address + 1))
          free(*((char**)address + 1));  
}

void eco5_info_display_item (WINDOW* graph, Configurator* pConfigurator, 
                            const  void* source, int base_row, 
                            int min_row, int col, const double max, int width)
{
     economy_info_t      eInfo;
     int                 len = 0, res;
     char                bar_str[base_row - min_row];
     char*               bar = bar_str;
     ColorDescriptor     *pDesc = &(pConfigurator->Colors._errorStr);
     
     if(*((char**)source)) {
          res = parseEconomyInfo(*((char**)source), &eInfo);
          if(!res || eInfo.status == ECONOMY_ERROR)
               len = sprintf(bar, "%s", ecoStatusStr(eInfo.status));
          else if(eInfo.status == ECONOMY_OFF) {
               len = sprintf(bar, "%s", ecoStatusStr(eInfo.status));
               pDesc =  &(pConfigurator->Colors._fairShareInfo);
          }
          else {
               len = sprintf(bar, "%s", eInfo.user);
               pDesc =  &(pConfigurator->Colors._fairShareInfo);
          }
     }
     else 
          len = sprintf(bar_str, "N/A");
     
     
     addAssigndMark(*((char**)source + 1), bar_str, &len);
     display_rtr(graph, pDesc, base_row , col, bar_str, len, 0);     
}


mon_disp_t eco5_info_mod_info = {
 md_item:         DISP_ECO5_INFO,              
 md_name:         "eco5",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "Economy5 Info", 
 md_titlength:    13,             
 md_format:       "", 
 md_key:          '$',
 md_length_func:  eco5_info_get_length, 
 md_scalar_func:  NULL, 
 md_new_func:     eco5_info_new_item, 
 md_del_func:     eco5_info_del_item,  
 md_disp_func:    eco5_info_display_item, 
 md_help_func:    eco5_info_display_help,
};

/*     {DISP_ECO5_INFO,            0, 0, "Economy5 Info", 13,    "", '$', */
/*      eco5_info_get_length, NULL,  */
/*      eco5_info_new_item, eco5_info_del_item,   */
/*      eco5_info_display_item, eco5_info_display_help }, */
