/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __DRAW_HELPER_H
#define __DRAW_HELPER_H

/****************************************************************************
 *                         
 *                        SCREEN DRAWING
 *
 ***************************************************************************/
void c_on (ColorDescriptor* pDesc);
void c_off (ColorDescriptor* pDesc);
void wc_on (WINDOW* target, ColorDescriptor* pDesc);
void wc_off (WINDOW* target, ColorDescriptor* pDesc);
void print_str (WINDOW* target, ColorDescriptor* pDesc,
                int row, int col, char *str, int len, int rev);

void display_str (WINDOW* target, ColorDescriptor* pDesc,
                  int base_row, int col, char *str, int len, int rev);
void display_rtr(WINDOW* target, ColorDescriptor* pDesc,
                 int base_row, int col, char *str, int len, int rev);

void display_bar (WINDOW* target, int base_row, int col, int total,
                  char usedChar, ColorDescriptor *pDesc, int rev);


void display_part_bar (WINDOW* target, 
                       int base_row, 
                       int col, 
                       int total, 
                       char freeChar, 
                       ColorDescriptor *freeColor, 
                       int revfree,
                       int used, 
                       char usedChar, 
                       ColorDescriptor *usedColor, 
                       int revused);
//                       ColorDescriptor *errorColor);

void display_dead(WINDOW* graph, void* status,
                  int base_row, int min_row, int col);

//void display_dead (WINDOW* graph, void* status, 
//                   int base_row, int min_row, int col,
//                   ColorDescriptor *color
//                   );

#endif
