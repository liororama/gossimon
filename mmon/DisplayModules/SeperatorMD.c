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
 *                          SEPERATOR MODULE                      
 *
 * Represents a column fill for |
 * Usfull for seperating nodes in a multi columns per node display 
 *
 ***************************************************************************/
#include "mmon.h"
#include "DisplayModules.h"

char* seperator_desc[] = {
  " Display a seperator(between bars)",
  (char*)NULL, };

int seperator_get_length ()
{
  return 0;
}
 

char** seperator_display_help ()
{
  return seperator_desc;
}

void seperator_new_item (mmon_info_mapping_t* iMap, const void* source, 
                     void* dest, settings_t* setup)
{
  return;  
}

void seperator_del_item (void* address) { return; }

void seperator_display_item (WINDOW* graph, Configurator* pConfigurator, 
                         const void* source, int base_row, 
                         int min_row, int col, const double max, int width)
{
  char buf[60];
  if(dbg_flg)
    fprintf(dbg_fp, "Base %d min %d\n", base_row, min_row);

  int len = base_row - min_row;
  memset(buf, pConfigurator->Appearance._seperatorChar, len > 0 ? len : 4);
  buf[len]='\0';
  display_rtr(graph, &(pConfigurator->Colors._seperatorBar),
              base_row , col, buf, strlen(buf), 0);
  return;
}

mon_display_module_t seperator_mod_info = {
 md_name:         "seperator",
 md_iscalar:      0,
 md_canrev:       0, 
 md_title:        "SEPERATOR", 
 md_titlength:    5,             
 md_format:       "", 
 md_key:         '|',
 md_length_func:  seperator_get_length, 
 md_scalar_func:  NULL, 
 md_new_func:     seperator_new_item, 
 md_del_func:     seperator_del_item,  
 md_disp_func:    seperator_display_item, 
 md_help_func:    seperator_display_help,
};
