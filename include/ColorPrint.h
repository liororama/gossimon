#ifndef __CL_LIBUTIL_COLOR_
#define __CL_LIBUTIL_COLOR_

typedef enum _color_t {
	color_normal = 0,
	color_red,
	color_blue,
	color_green,
	color_yellow,
	color_max,
	color_bold = 1024,
} color_t;

#include <stdlib.h>
#include <stdio.h>

int sprintf_color(color_t color, char *buff, char *fmt, ...);

void fprintf_color(color_t color, FILE *f, char *fmt, ...);

#define fprintf_green(f,fmt,args...)                   \
do{ fprintf_color(color_green,f,fmt,## args);} while(0)

#define fprintf_red(f,fmt,args...)                   \
do{ fprintf_color(color_red,f,fmt,## args);} while(0)

#define fprintf_blue(f,fmt,args...)                   \
do{ fprintf_color(color_blue,f,fmt,## args);} while(0)

#define fprintf_yellow(f,fmt,args...)                   \
do{ fprintf_color(color_yellow,f,fmt,## args);} while(0)


// Printf_xxx
#define printf_green(fmt,args...)                   \
do{ fprintf_color(color_green,stdout,fmt,## args);} while(0)

#define printf_red(fmt,args...)                   \
do{ fprintf_color(color_red,stdout,fmt,## args);} while(0)

#define printf_blue(fmt,args...)                   \
do{ fprintf_color(color_blue,stdout,fmt,## args);} while(0)

#define printf_yellow(fmt,args...)                   \
  do{ fprintf_color(color_yellow,stdout,fmt,## args);} while(0)


// Debug 
#define color_debug_r(dbg,fmt,args...) \
  do{ if(dbg) {fprintf_color(color_red,stderr,fmt,## args);}} while(0)

#define color_debug_g(dbg,fmt,args...) \
  do{ if(dbg) {fprintf_color(color_green,stderr,fmt,## args);}} while(0)

#define color_debug_b(dbg,fmt,args...) \
  do{ if(dbg) {fprintf_color(color_blue,stderr,fmt,## args);}} while(0)

#define color_debug_y(dbg,fmt,args...) \
  do{ if(dbg) {fprintf_color(color_yellow,stderr,fmt,## args);}} while(0)

#endif
