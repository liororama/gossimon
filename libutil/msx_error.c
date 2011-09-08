/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

/******************************************************************************
 *
 * Author(s): Amar Lior, Peer Ilan
 *
 *****************************************************************************/

#include "msx_error.h"
#include "msx_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <parse_helper.h>

int msx_debug_level = 0;
int msx_debug_on = 0;

char *msx_debug_file = NULL;
FILE *msx_debug_filp = NULL;
static char buf[4096];

msx_debug_ent_t debugEntries[] =
  { 
  { COMM_DEBUG,        COMM_DEBUG_STR,           "comm"},
  { MAP_DEBUG,         MAP_DEBUG_STR,            "map"},
  { UPLIST_DEBUG,      UPLIST_DEBUG_STR,         "uplist"},
  { READPROC_DEBUG,    READPROC_DEBUG_STR,       "proc"},
  { VEC_DEBUG,         VEC_DEBUG_STR,            "vec"},
  { WIN_DEBUG,         WIN_DEBUG_STR,            "win"},
  { INFOD_DEBUG,       INFOD_DEBUG_STR,          "infod"},
  { GINFOD_DEBUG,      GINFOD_DEBUG_STR,         "ginfod"},
  { GLOBAL_DEBUG,      GLOBAL_DEBUG_STR,         "global"},
  { CTL_DEBUG,         CTL_DEBUG_STR,            "ctl"},
  { KCOMM_DEBUG,       KCOMM_DEBUG_STR,          "kcomm"},
  { CLEAR_DEBUG,       CLEAR_DEBUG_STR,          "clear"},
  { SETPE_DEBUG,       SETPE_DEBUG_STR,          "setpe"},
  { MMON_DEBUG,        MMON_DEBUG_STR,           "mmon"},
  { LUR_DEBUG,         LUR_DEBUG_STR,            "lur"},
  { MCONF_DEBUG,       MCONF_DEBUG_STR,          "mconf"},
  { OWNERD_DEBUG,      OWNERD_DEBUG_STR,         "ownerd"},
  { FREEZD_DEBUG,      FREEZD_DEBUG_STR,         "freezd"},
  { MOSCTL_DEBUG,      MOSCTL_DEBUG_STR,         "mosctl"},
  { MEASURE_DEBUG,     MEASURE_DEBUG_STR,        "measure"},
  { PRIOD_DEBUG,       PRIOD_DEBUG_STR,          "priod"},
  { PRIOD2_DEBUG,      PRIOD2_DEBUG_STR,         "priod2"},
  { UVR_DEBUG,         UVR_DEBUG_STR,            "uvr"},
  { FST_DEBUG,         FST_DEBUG_STR,            "fst"},
  { 0,                 NULL,                     NULL},
};


void msx_add_debug(int level)
{
    msx_debug_level |= level;
    if(msx_debug_filp == NULL)
	    msx_debug_filp = stderr;
}


void msx_set_debug(int level)
{
    msx_debug_level = level;
    if(msx_debug_filp == NULL)
	    msx_debug_filp = stderr;
}

int msx_get_debug()
{
     return msx_debug_level;
}   




FILE * msx_get_debug_fh() {
     return msx_debug_filp;
}


int get_debug_from_short(char *debugStr)
{
     int i = 0;
     while(debugEntries[i].str) {
          if(strcmp(debugStr, debugEntries[i].shortStr) == 0)
               return debugEntries[i].flag;
          i++;
     }
     return 0;
}

static int get_debug_flag(char *debugStr)
{
     int i = 0;
     while(debugEntries[i].str) {
          if(strcmp(debugStr, debugEntries[i].str) == 0)
               return debugEntries[i].flag;
          i++;
     }
     return 0;
}
/** Reading the debug file, parsing it and setting the msx_debug_level variable
    according to the entries found in the file
 @param *fname       Alternative debug file name
 @return 0 on error 1 on success
*/
int msx_read_debug_file(char *fname) {
	FILE *dbgFile;
	char *ptr;
	char line[256];
	int  found_out_file = 0;
	
	if(!fname)
		fname = DEFAULT_DEBUG_FILE;
	
	if(!(dbgFile = fopen(fname, "r")))
		return 0;

        msx_debug_level = 0;

	while(fgets(line, 256, dbgFile)) {
		char *out_file;
		
                // Skeeping comments
		if(line[0] == '#')
			continue;
		// removing spaces and newline
		ptr = line;
                ptr = eat_spaces(ptr);
                trim_spaces(ptr);

                // Getting the output file to use
		if((out_file = strstr(ptr, MSX_DEBUG_OUT_FILE_STR))) {
			found_out_file = 1;
                        out_file+= strlen(MSX_DEBUG_OUT_FILE_STR);
			out_file = eat_spaces(out_file);
			if(*out_file == '=') {
                             out_file++;
                             out_file = eat_spaces(out_file);
                        }

                        // out_file should now point to the file name
			if(!msx_debug_file ||
			   strcmp(msx_debug_file, out_file) != 0) {
				if(msx_debug_file)
					free(msx_debug_file);
				if(msx_debug_filp)
					fclose(msx_debug_filp);
				msx_debug_filp = NULL;
				msx_debug_file = strdup(out_file);
			}
			
			if(msx_debug_filp == NULL) {
				msx_debug_filp = fopen(out_file, "w");
				if(msx_debug_filp == NULL) {
					free(msx_debug_file);
					msx_debug_file = NULL;
					msx_debug_filp = NULL;
				} else {
					setlinebuf(msx_debug_filp);
				}
			}
		}

                // Getting the flags to set
                int flag;
                if((flag = get_debug_flag(ptr)))
                     msx_debug_level |= flag;
                
        }
        
	fclose(dbgFile);
	if(!found_out_file) {
             if(msx_debug_file) {
                  fclose(msx_debug_filp);
                  free(msx_debug_file);
                  msx_debug_file = NULL;
             }
             msx_debug_filp = NULL;
	}
	
	return 1;
}

// Removing the \n from the ctime string
char *ctime_fixed()
{
  char *tmp,*tmp2;
  time_t tt;
  
  time(&tt);
  tmp=ctime(&tt);
  
  for(tmp2=tmp;(*tmp2) && (*tmp2)!='\n';tmp2++)
    ;
  *tmp2='\0';
  return tmp;
}

/* Prints a message into the log/tty */
void msx_critical_error(char* fmt, ...) {
  va_list ap;
  char *tmp;

  fprintf(stderr,SETCOLOR_RED);
  fprintf(stderr, "\n***********************************************\n");
  fprintf(stderr, "Critical Error:\n");
  
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  tmp = ctime_fixed();
  fprintf(stderr, "\n%s  %s", tmp, buf);
  va_end(ap);

  
  fprintf(stderr, "\n***********************************************\n");
  fprintf(stderr,SETCOLOR_NORMAL);
  exit(1);
}

void
msx_error(FILE *ff, char* fmt, ...) {
  va_list ap;
  char *tmp;

  fprintf(ff,SETCOLOR_RED);
  fprintf(ff, "\n***********************************************\n");
  fprintf(ff, "Error:\n");
  
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  tmp = ctime_fixed();
  fprintf(ff, "\n%s  %s", tmp, buf);
  va_end(ap);

  
  fprintf(ff, "\n***********************************************\n");
  fprintf(ff,SETCOLOR_NORMAL);
}


void
msx_sys_error(FILE *ff, char* fmt, ...) {
  va_list ap;
  char *tmp;

  fprintf(ff,SETCOLOR_RED);
  fprintf(ff, "\n***********************************************\n");
  fprintf(ff, "Error:\n");
  
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  tmp = ctime_fixed();
  fprintf(ff, "\n%s  %s", tmp, buf);
  va_end(ap);

  fprintf(ff, "\nSys MSG: %s\n", strerror(errno));
  
  
  fprintf(ff, "\n***********************************************\n");
  fprintf(ff,SETCOLOR_NORMAL);
}

void
msx_sys_lerror( int lvl, FILE *ff, char* fmt, ...) {
  va_list ap;
  char *tmp;

  if( !(msx_debug_level & lvl ))
	  return;
  
  fprintf(ff,SETCOLOR_RED);
  fprintf(ff, "\n***********************************************\n");
  fprintf(ff, "Error:\n");
  
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  tmp = ctime_fixed();
  fprintf(ff, "\n%s  %s", tmp, buf);
  va_end(ap);

  fprintf(ff, "\nSys MSG: %s\n", strerror(errno));
  
  
  fprintf(ff, "\n***********************************************\n");
  fprintf(ff,SETCOLOR_NORMAL);
}





void
msx_warning(char* fmt, ...) {
  va_list ap;
  char *tmp;

  fprintf(stderr,SETCOLOR_YELLOW);
  fprintf(stderr, "\n***********************************************\n");
  fprintf(stderr, "Warning:\n");
  
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  tmp = ctime_fixed();
  fprintf(stderr, "\n%s  %s", tmp, buf);
  va_end(ap);
  
  
  fprintf(stderr, "\n***********************************************\n");
  fprintf(stderr,SETCOLOR_NORMAL);
  
}


void
msx_printf(char * fmt, ...) {
  va_list ap;
  
  fprintf(stdout,SETCOLOR_YELLOW);
  
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  fprintf(stdout, "%s", buf);
  va_end(ap);

  fprintf(stdout, SETCOLOR_NORMAL);

}

void
msx_fprintf_color(char *color,FILE *ff, char * fmt, ...) {
  va_list ap;
  
  fprintf(ff ,"%s", color);
  
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  fprintf(ff , "%s", buf);
  va_end(ap);

  fprintf(ff ,SETCOLOR_NORMAL);

}

void
msx_debug_color(char *color, char * fmt, ...) {
  va_list ap;

  if(msx_debug_filp == NULL)
	  return;
  
  fprintf(msx_debug_filp ,"%s", color);
  
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  fprintf(msx_debug_filp , "%s", buf );
  va_end(ap);

  fprintf(msx_debug_filp ,SETCOLOR_NORMAL);

}




