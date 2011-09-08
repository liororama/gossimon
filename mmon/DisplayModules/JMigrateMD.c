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
#include "JMigrateInfo.h"


//int addAssigndMark(char* proc_watch, char *bar_str, int *len);
/****************************************************************************
 *                         
 *                          DISP_INFOD_DEBUG MODULE                      
 *
 ***************************************************************************/

char* jmig_desc[] = 
  { 
    " JMIGRATE : '$' ",
    " - Display jmigrate of the cluster (research)",
    "",
    (char*)NULL, };

int jmig_get_length ()
{
  return 2*sizeof(char*);
}

char** jmig_display_help ()
{
  return jmig_desc;
}

void jmig_new_item (mmon_info_mapping_t* iMap, const void* source, 
                    void* dest, settings_t* setup)
{
     getVlenItem(((idata_entry_t*)source)->data, 
                 iMap->jmig, (char**)dest); 
     
     getVlenItem(((idata_entry_t*)source)->data, 
                 iMap->proc_watch, (char**)dest + 1);
}

void jmig_del_item (void* address)
{
     if (*((char**)address))
          free(*((char**)address));
     
     if (*((char**)address + 1))
          free(*((char**)address + 1));  
}

void jmig_display_item (WINDOW* graph, Configurator* pConfigurator, 
                            const  void* source, int base_row, 
                            int min_row, int col, const double max, int width)
{
     jmigrate_info_t     jinfo;
     int                 len = 0, res;
     char                bar_str[base_row - min_row];
     char*               bar = bar_str;
     ColorDescriptor     *pDesc = &(pConfigurator->Colors._fairShareInfo);

     memset(bar_str, 0, base_row - min_row);
     if(*((char**)source)) {
          res = parseJMigrateInfo(*((char**)source), &jinfo);
          
          if(!res || jinfo.status == JMIGRATE_ERROR) {
               len = sprintf(bar, "%c", 'E');//ecoStatusStr(eInfo.status));
               pDesc =  &(pConfigurator->Colors._errorStr);
          }
          else if(jinfo.status == JMIGRATE_HOST) {
               len = sprintf(bar, "%3d H",jinfo.vmNum);
               pDesc =  &(pConfigurator->Colors._fairShareInfo);
          }
          else if (jinfo.status == JMIGRATE_VM) {
               len = sprintf(bar, "%10s V", jinfo.vmInfoArr[0].host);
               pDesc =  &(pConfigurator->Colors._fairShareInfo);
          }
     }
     else 
          len = sprintf(bar_str, "N/A");
     
     
//     addAssigndMark(*((char**)source + 1), bar_str, &len);
     display_rtr(graph, pDesc, base_row , col, bar_str, len, 1);     
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

/* double jmig_scalar_div_x (const void* item, double x) */
/* { */
/*     economy_info_t eInfo; */
/*     parseEconomyInfo(*((char**)item), &eInfo); */

/*     if (eInfo.currPrice > eInfo.minPrice) */
/*         return eInfo.currPrice/x; */
    
/*         return eInfo.minPrice/x; */
/* } */
