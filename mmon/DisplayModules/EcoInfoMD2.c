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


int addProviderdMark(char* proc_watch, char *bar_str, int *len);
/****************************************************************************
 *                         
 *                          DISP_INFOD_DEBUG MODULE                      
 *
 ***************************************************************************/

char* eco2_info_desc[] = 
  { 
    " ECONOMY2 INFO : '$' ",
    " - Display the distributed ecomomical parameters of the cluster ",
    "",
    (char*)NULL, };

int eco2_info_get_length ()
{
  return sizeof(char*);
}

char** eco2_info_display_help ()
{
  return eco2_info_desc;
}

void eco2_info_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
        getVlenItem(((idata_entry_t*)source)->data, 
                 iMap->eco_info, (char**)dest); 
     
}

void eco2_info_del_item (void* address)
{
     if (*((char**)address))
          free(*((char**)address));
     
}

void eco2_info_display_item (WINDOW* graph, Configurator* pConfigurator, 
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
               len = sprintf(bar, "error");
          else if(eInfo.status == ECONOMY_OFF) {
               len = sprintf(bar, " off ");
               pDesc =  &(pConfigurator->Colors._fairShareInfo);
          }
          else {
               if(eInfo.protectedTime > 0) {
                    len = sprintf(bar, "s%5.1f n%5.1f t%4d",
                                  eInfo.schedPrice, eInfo.nextPrice, eInfo.protectedTime);
               } else {
                    len = sprintf(bar, "s%5.1f n%5.1f i%4.1f",
                                  eInfo.schedPrice, eInfo.nextPrice, eInfo.incPrice);
               }
               pDesc =  &(pConfigurator->Colors._fairShareInfo);
          }
     }
     else 
             len = sprintf(bar_str, "N/A");

     // This is in order to make this display aligned with the eco1 display
     // which also present the + for the assignd
          char statusChar = ecoStatusChar(eInfo.status);
     char b[3];
     b[0] = ' '; b[1] = statusChar; b[2] = '\0';
     
     strcat(bar_str, b);
     len += 2;
     
     display_rtr(graph, pDesc, base_row , col, bar_str, len, 0);     
}

