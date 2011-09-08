/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __MSX_ERROR
#define __MSX_ERROR

/*
 *
 *     Error Functions  *
 */

#include <stdio.h>


#define SETCOLOR_GREEN   "\033[1;32m"
#define SETCOLOR_RED     "\033[1;31m"
#define SETCOLOR_YELLOW  "\033[1;33m"
#define SETCOLOR_BLUE    "\033[1;34m"
#define SETCOLOR_NORMAL  "\033[0;39m"

char *ctime_fixed();



void msx_critical_error(char *fmt, ...);
void msx_error(FILE *ff, char *fmt, ...);
void msx_sys_error( FILE *ff, char *fmt, ...);
void msx_sys_lerror( int lvl, FILE *ff, char* fmt, ...);

void msx_warning(char *fmt, ...);
void msx_printf(char *fmt, ...);
void msx_fprintf_color(char *color,FILE *ff, char *fmt, ...);
void msx_debug_color(char *color, char *fmt, ...);

#define msx_fprintf(ff,fmt,args...)                     \
do{ msx_fprintf_color(SETCOLOR_NORMAL,ff,fmt,## args);} while(0)

#define msx_fprintf_g(ff,fmt,args...)                   \
do{ msx_fprintf_color(SETCOLOR_GREEN,ff,fmt,## args);} while(0)

#define msx_fprintf_r(ff,fmt,args...)                   \
do{ msx_fprintf_color(SETCOLOR_RED,ff,fmt,## args);} while(0)

#define msx_fprintf_y(ff,fmt,args...)                   \
do{ msx_fprintf_color(SETCOLOR_YELLOW,ff,fmt,## args);} while(0)

#define msx_fprintf_b(ff,fmt,args...)                   \
do{ msx_fprintf_color(SETCOLOR_BLUE,ff,fmt,## args);} while(0)


#define debug_r(fmt,args...)   \
do{ if(msx_debug_level) msx_debug_color(SETCOLOR_RED,fmt, ## args);} while(0)
#define debug_g(fmt,args...)   \
do{ if(msx_debug_level) msx_debug_color(SETCOLOR_GREEN,fmt, ## args);} while(0)
#define debug_y(fmt,args...)   \
do{ if(msx_debug_level) msx_debug_color(SETCOLOR_YELLOW,fmt, ## args);} while(0)
#define debug_b(fmt,args...)   \
do{ if(msx_debug_level) msx_debug_color(SETCOLOR_BLUE,fmt, ## args);} while(0)

#define debug_lr(lvl,fmt,args...)   \
do{ if(msx_debug_level & lvl) msx_debug_color(SETCOLOR_RED,fmt, ## args);} while(0)

#define debug_lg(lvl,fmt,args...)   \
do{ if(msx_debug_level & lvl) msx_debug_color(SETCOLOR_GREEN,fmt, ## args);} while(0)

#define debug_ly(lvl,fmt,args...)   \
do{ if(msx_debug_level & lvl) msx_debug_color(SETCOLOR_YELLOW,fmt, ## args);} while(0)

#define debug_lb(lvl,fmt,args...)   \
do{ if(msx_debug_level & lvl) msx_debug_color(SETCOLOR_BLUE,fmt, ## args);} while(0)

#endif /* __MSX_ERROR */

