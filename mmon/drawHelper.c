/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <curses.h>
#include <mmon.h>
#include <ConfigurationManager.h>
#include <DisplayModules.h>

/****************************************************************************
 *                         
 *                        SCREEN DRAWING
 *
 ***************************************************************************/
void c_on(ColorDescriptor* pDesc)
//turns on a color discriptor
{
    if (pDesc->color_pair != -1)
        attron(pDesc->attr_flags | COLOR_PAIR(pDesc->color_pair));
}

void c_off(ColorDescriptor* pDesc)
//turns off a color discriptor
{
    if (pDesc->color_pair != -1)
        attroff(pDesc->attr_flags | COLOR_PAIR(pDesc->color_pair));
}

void wc_on(WINDOW* target, ColorDescriptor* pDesc)
//turns on a color discriptor in the window
{
    if (pDesc->color_pair != -1)
        wattron(target, pDesc->attr_flags | COLOR_PAIR(pDesc->color_pair));
}

void wc_off(WINDOW* target, ColorDescriptor* pDesc)
//turns off a color discriptor in the window
{
    if (pDesc->color_pair != -1)
        wattroff(target, pDesc->attr_flags | COLOR_PAIR(pDesc->color_pair));
}

void print_str(WINDOW* target, ColorDescriptor* pDesc,
        int row, int col, char *str, int len, int rev)
//outputs a string of given length in (row, col)
{
    int index;

    wc_on(target, pDesc);
    if (rev)
        wattron(target, A_REVERSE);

    for (index = 0; index < len; index++)
        mvwaddch(target, row, col + index, str[index]);

    if (rev)
        wattroff(target, A_REVERSE);
    wc_off(target, pDesc);
}

void display_str(WINDOW* target, ColorDescriptor* pDesc,
        int base_row, int col, char *str, int len, int rev)
//outputs a vertical form of the given string.
{
    int index;

    wc_on(target, pDesc);
    if (rev)
        wattron(target, A_REVERSE);

    for (index = 0; index < len; index++)
        mvwaddch(target, base_row - index, col, str[index]);

    if (rev)
        wattroff(target, A_REVERSE);
    wc_off(target, pDesc);
}



void display_rtr(WINDOW* target, ColorDescriptor* pDesc,
        int base_row, int col, char *str, int len, int rev)
//outputs a vertical form of the given string - reversed.
{
    int index;

    wc_on(target, pDesc);
    if (rev)
        wattron(target, A_REVERSE);

    for (index = 0; index < len; index++)
        mvwaddch(target, base_row - index, col, str[len - 1 - index]);

    if (rev)
        wattroff(target, A_REVERSE);
    wc_off(target, pDesc);
}


void display_bar(WINDOW* target, int base_row, int col, int total,
        char usedChar, ColorDescriptor *pDesc, int rev)
//draws a "total"-sized bar of "usedChar"s in target window. 
{
    int index;

    wc_on(target, pDesc);
    if (rev)
        wattron(target, A_REVERSE);

    for (index = base_row; index > base_row - total; index--)
        mvwaddch(target, index, col, usedChar);

    if (rev)
        wattroff(target, A_REVERSE);
    wc_off(target, pDesc);
}

//draws 2 bars - both max and actual values.
void display_part_bar(WINDOW* target, int base_row, int col,
                      int total, char freeChar, ColorDescriptor *freeColor, int revfree,
                      int used,  char usedChar,  ColorDescriptor *usedColor, int revused)
{
    if ((total < 0 || used < 0) || (total < used)) {
        display_rtr(target, &(pConfigurator->Colors._errorStr),
                base_row - 1, col, "ERROR", 5, 0);
        return;
    }

    display_bar(target, base_row - used, col, total - used, freeChar, freeColor, revfree);
    display_bar(target, base_row,        col, used,         usedChar, usedColor, revused);
}

void display_dead(WINDOW* graph, void* status,
        int base_row, int min_row, int col)
//This function displayes a dead-node notification
//The function uses the address of the status info of the node
{
    char c;

    switch ((unsigned short) scalar_div_x(dm_getIdByName("infod-status"), status, 1)) {
    case INFOD_DEAD_INIT: c = 'I';
        break;
    case INFOD_DEAD_AGE: c = 'A';
        break;
    case INFOD_DEAD_CONNECT: c = 'C';
        break;
    case INFOD_DEAD_PROVIDER: c = 'P';
        break;
    case INFOD_DEAD_VEC_RESET: c = 'R';
        break;
    default:
        c = '*';
    }

    display_rtr(graph, &(pConfigurator->Colors._deadCaption),
            base_row - 2, col, "DEAD", 4, 0);
    display_rtr(graph, &(pConfigurator->Colors._deadCaption),
            base_row - 7, col, &c, 1, 0);
}


