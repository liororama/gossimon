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
#include "DisplayModules.h"
#include "drawHelper.h"

int addAssigndMark(char* proc_watch, char *bar_str, int *len);
/****************************************************************************
 *                         
 *                          DISP_INFOD_DEBUG MODULE                      
 *
 ***************************************************************************/

char* eco_info_desc[] = 
  { 
    " Display ecomomical parameters (testing) ",
    (char*)NULL, };

int eco_info_get_length ()
{
  return 2*sizeof(char*);
}

char** eco_info_display_help ()
{
  return eco_info_desc;
}

void eco_info_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
     getVlenItem(((idata_entry_t*)source)->data, 
                 iMap->eco_info, (char**)dest); 
     
     getVlenItem(((idata_entry_t*)source)->data, 
                 iMap->proc_watch, (char**)dest + 1);
}

void eco_info_del_item (void* address)
{
     if (*((char**)address))
          free(*((char**)address));
     
     if (*((char**)address + 1))
          free(*((char**)address + 1));  
}

void eco_info_display_item (WINDOW* graph, Configurator* pConfigurator, 
                            const  void* source, int base_row, 
                            int min_row, int col, const double max, int width)
{
     economy_info_t      eInfo;
     int                 len = 0, res;
     char                bar_str[base_row - min_row];
     char*               bar = bar_str;
     ColorDescriptor     *pDesc = &(pConfigurator->Colors._fairShareInfo);

     memset(bar_str, 0, base_row - min_row);
     if(*((char**)source)) {
             res = parseEconomyInfo(*((char**)source), &eInfo);
            
             if(!res || eInfo.status == ECONOMY_ERROR) {
                 len = sprintf(bar, "%c", 'E');//ecoStatusStr(eInfo.status));
                 pDesc =  &(pConfigurator->Colors._errorStr);
             }
             else if(eInfo.status == ECONOMY_OFF) {
                 len = sprintf(bar, "%c", 'D');//ecoStatusStr(eInfo.status));
                 pDesc =  &(pConfigurator->Colors._errorStr);
             }
             else if (eInfo.status == ECONOMY_PROTECTED) {
                 len = sprintf(bar, "%c", 'P');//
                 //              len = sprintf(bar, "%s  %4d", ecoStatusStr(eInfo.status), eInfo.protectedTime);
                 //                 pDesc =  &(pConfigurator->Colors._fairShareInfo);
             }
             /*else {
                 len = sprintf(bar, "r%5.1f c%5.1f t%4.1f",
                eInfo.minPrice, eInfo.currPrice, eInfo.timeLeft);
                 pDesc =  &(pConfigurator->Colors._fairShareInfo);
                 }*/
                     
             if (eInfo.currPrice < eInfo.minPrice) {
                 display_bar(graph, base_row, col, 
                             ((base_row - min_row) * eInfo.minPrice) / (float)max,
                             ' ', &(pConfigurator->Colors._fairShareInfo), 1);
             } else {
               display_part_bar (graph, base_row, col, 
                                   //total
                                   ((base_row - min_row) * eInfo.currPrice) / (float)max,
                                   ' ', 
                                   &(pConfigurator->Colors._swapCaption), 1,
                                   //used
                                   ((base_row - min_row) * eInfo.minPrice) / (float)max,
                                   ' ', &(pConfigurator->Colors._fairShareInfo), 1);   
             }
     }
     //     }
     else 
         len = sprintf(bar_str, "N/A");
     
     
     addAssigndMark(*((char**)source + 1), bar_str, &len);
     display_rtr(graph, pDesc, base_row , col, bar_str, len, 1);     
}


int addAssigndMark(char* proc_watch, char *bar_str, int *len)
     //help function for using_users, using_clusters
{
  proc_watch_entry_t procArr[20];
  int pSize = 20;
  int found = 0, foundAD = 0, foundED=0;
  int i;
  if(getProcWatchInfo(proc_watch, procArr, &pSize)) 
    {
      for(i=0 ; i< pSize && found < 2; i++) 
        {
                if(strcmp(procArr[i].procName, "assignd") == 0 &&
                   procArr[i].procStat == PS_OK) {
                        foundAD = 1;
                        found++;
                }
                if(strcmp(procArr[i].procName, "economyd") == 0 &&
                   procArr[i].procStat == PS_OK) {
                        foundED = 1;
                        found++;
                }
        }
    }

  /*
  if(foundED && !foundAD)
          strcat(bar_str, "- ");
  else if(!foundED && foundAD)
          strcat(bar_str, "| ");
  else if(foundED && foundAD)
          strcat(bar_str, "+ ");
  else
          strcat(bar_str, "  ");
  */
  strcat(bar_str, foundED ? "&" : " ");
  strcat(bar_str, foundAD ? "+" : " ");
  
  *len += 2;
  return 1;
}

double eco_info_scalar_div_x (const void* item, double x)
{
    economy_info_t eInfo;
    parseEconomyInfo(*((char**)item), &eInfo);

    if (eInfo.currPrice > eInfo.minPrice)
        return eInfo.currPrice/x;
    
        return eInfo.minPrice/x;
}

mon_display_module_t eco_info_mod_info = {
 md_name:         "eco",
 md_iscalar:      1,
 md_canrev:       0, 
 md_title:        "Economy Info", 
 md_titlength:    12,             
 md_format:       "%5.2f", 
 md_key:          '$',
 md_length_func:  eco_info_get_length, 
 md_scalar_func:  eco_info_scalar_div_x, 
 md_new_func:     eco_info_new_item, 
 md_del_func:     eco_info_del_item,  
 md_disp_func:    eco_info_display_item, 
 md_help_func:    eco_info_display_help,
 md_text_info_func: NULL
};

/*     {DISP_ECO_INFO,             1, 0, "Economy Info", 12,    "%5.2f", '$', */
/*      eco_info_get_length, eco_info_scalar_div_x,  */
/*      eco_info_new_item, eco_info_del_item,   */
/*      eco_info_display_item, eco_info_display_help }, */
