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

char* eco3_info_desc[] = 
  { 
    " ECONOMY INFO : '$' ",
    " - Display ecomomical parameters of the cluster ",
    "",
    (char*)NULL, };

int eco3_info_get_length ()
{
  return 2*sizeof(char*);
}

char** eco3_info_display_help ()
{
  return eco3_info_desc;
}

void eco3_info_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
     getVlenItem(((idata_entry_t*)source)->data, 
                 iMap->eco_info, (char**)dest); 
     
     getVlenItem(((idata_entry_t*)source)->data, 
                 iMap->proc_watch, (char**)dest + 1);
}

void eco3_info_del_item (void* address)
{
     if (*((char**)address))
          free(*((char**)address));
     
     if (*((char**)address + 1))
          free(*((char**)address + 1));  
}

void eco3_info_display_item (WINDOW* graph, Configurator* pConfigurator, 
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
              if(eInfo.schedHost) {
                  char *p = strchr(eInfo.schedHost, (int)'.');
                  if (p)
                      (*p) = 0;
              }

               if(eInfo.currHost) {
                  char *p = strchr(eInfo.currHost, (int)'.');
                  if (p)
                      (*p) = 0;
              }
              
              len = sprintf(bar, "S-%s R-%s", (eInfo.schedHost?eInfo.schedHost:"N/A"),
                            (eInfo.schedHost?eInfo.currHost:"N/A"));

              pDesc =  &(pConfigurator->Colors._fairShareInfo);
          }
     }
     else 
             len = sprintf(bar_str, "N/A");
     
             
     addAssigndMark(*((char**)source + 1), bar_str, &len);
     display_rtr(graph, pDesc, base_row , col, bar_str, len, 0);     
}


/* int addAssigndMark(char* proc_watch, char *bar_str, int *len) */
/*      //help function for using_users, using_clusters */
/* { */
/*   proc_watch_entry_t procArr[20]; */
/*   int pSize = 20; */
/*   int found = 0, foundAD = 0, foundED=0; */
/*   int i; */
/*   if(getProcWatchInfo(proc_watch, procArr, &pSize))  */
/*     { */
/*       for(i=0 ; i< pSize && found < 2; i++)  */
/*         { */
/*                 if(strcmp(procArr[i].procName, "assignd") == 0 && */
/*                    procArr[i].procStat == PS_OK) { */
/*                         foundAD = 1; */
/*                         found++; */
/*                 } */
/*                 if(strcmp(procArr[i].procName, "economyd") == 0 && */
/*                    procArr[i].procStat == PS_OK) { */
/*                         foundED = 1; */
/*                         found++; */
/*                 } */
/*         } */
/*     } */

/*   /\* */
/*   if(foundED && !foundAD) */
/*           strcat(bar_str, "- "); */
/*   else if(!foundED && foundAD) */
/*           strcat(bar_str, "| "); */
/*   else if(foundED && foundAD) */
/*           strcat(bar_str, "+ "); */
/*   else */
/*           strcat(bar_str, "  "); */
/*   *\/ */
/*   strcat(bar_str, foundED ? "&" : " "); */
/*   strcat(bar_str, foundAD ? "+" : " "); */
  
/*   *len += 2; */
/*   return 1; */
/* } */
