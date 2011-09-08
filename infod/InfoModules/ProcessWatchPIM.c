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
 * Author(s): Amar Lior
 *
 *****************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include <msx_error.h>
#include <msx_debug.h>
#include <parse_helper.h>
#include <ProcessWatchInfo.h>
#include <ProcessWatchPIM.h>
#include "infoitems.h"

typedef struct proc_watch {
	int                  maxSize;
	int                  size;
	char               **pidFileArr;
	proc_watch_entry_t  *procArr;
} pw_pim_t;

typedef struct proc_watch *proc_watch_t;


int procwatch_pim_init(void **module_data, void *init_data)
{
	pw_pim_t *pw;
 	char         *initStr = (char *)init_data;

        int           size = 100;
        int           linesNum;
        char         *linesArr[100];
        
	pw = malloc(sizeof(struct proc_watch));
	if(!pw) return 0;
        
	bzero(pw, sizeof(pw_pim_t));
	pw->maxSize = 100;
	pw->pidFileArr   = malloc(sizeof(char *) * pw->maxSize);
	pw->procArr      = malloc(sizeof(proc_watch_entry_t) * pw->maxSize);
	if(!pw->pidFileArr || !pw->procArr)
		goto failed;

        pw->size = 0;

        // Nothing to parse
	if(!initStr) {
                *module_data = pw;
                return 1;
	}
        
	linesNum = split_str(initStr, linesArr, size, "\n");
        int res;
        for(int i=0; i<linesNum ; i++) {
                pw->pidFileArr[i] = malloc(256);
                if(!pw->pidFileArr[i])
                        goto failed;
                res = sscanf(linesArr[i], "%s %s", pw->procArr[i].procName, pw->pidFileArr[i]);
                if(res != 2) {
                        return 0;
                }
                debug_lr(KCOMM_DEBUG, "PIM PROCWATCH (init): %s %s\n",
                         pw->procArr[i].procName, pw->pidFileArr[i]);
                
                trim_spaces(pw->pidFileArr[i]);
                pw->procArr[i].procStat = PS_NO_INFO;
        }
        
        pw->size = linesNum;
	*module_data = pw;
        return 1;
 failed:
	procwatch_pim_free((void **)&pw);
	return 0;
}

int procwatch_pim_free(void **module_data) {
	pw_pim_t *pw = (pw_pim_t*) (*module_data);
	
	if(!pw)
		return 0;
	if(pw->pidFileArr)
		free(pw->pidFileArr);
	if(pw->procArr)
		free(pw->procArr);
	free(pw);
	*module_data = NULL;
        return 1;
}

int procwatch_pim_update(void *module_data)
{
        int           res;
	pw_pim_t     *pw = (pw_pim_t*)module_data;
        
	for(int i=0 ; i<pw->size ; i++) {
		res = checkProcPidFile(pw->pidFileArr[i], &pw->procArr[i]);
                if(!res)
                        pw->procArr[i].procStat = PS_ERR;
	}
        
        return 1;
}

int procwatch_pim_get(void *module_data, void *data, int *size)
{
        int            res;
	pw_pim_t      *pw = (pw_pim_t *)module_data;
        char          *buff = (char *) data;

        if(pw->size == 0) {
                *size = 0;
                return 1;
        }
        res = writeProcWatchInfo(pw->procArr, pw->size, 0, buff);
        if(res) *size = strlen(buff)+1;
        return res;
}


int procwatch_pim_desc(void *module_data, char *buff) {
  sprintf(buff, "\t<vlen name=\"" ITEM_PROC_WATCH_NAME "\" type=\"string\" unit=\"xml\" />\n" );
  return 1;
}
