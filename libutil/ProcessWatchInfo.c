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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>


#include <parse_helper.h>
#include <info.h>
#include <ProcessWatchInfo.h>

#define ROOT_TAG     ITEM_PROC_WATCH_NAME
#define TAG_NAME     "proc"
#define ATTR_NAME    "name"
#define ATTR_PID     "pid"
#define ATTR_STAT    "stat"

char *pw_status_str[] = {
        "no info",
        "no pid file",
        "error pid file",
        "error no pid",
        "error",
        "no proc",
        "ok",
        NULL
};

proc_status_t pw_status_arr[] = {
	PS_NO_INFO,
	PS_NO_PID_FILE,
	PS_ERR_PID_FILE,
	PS_ERR_NO_PID,
	PS_ERR,
	PS_NO_PROC,
	PS_OK,
};

char *procStatusStr(proc_status_t st) {
        if(st < 0 || st > PS_OK)
                return NULL;
        return pw_status_str[st];
}        

int  strToProcStatus(char *str, proc_status_t *stat) {
        int i;

        if(!str) return 0;
        for(i = 0 ; pw_status_str[i] ; i++) {
                if(strcmp(pw_status_str[i], str) == 0) {
                        * stat = pw_status_arr[i];
                        return 1;
                }
        }
        return 0;
}




static void init_proc_ent(proc_watch_entry_t *ent) {
        ent->procName[0] = '\0';
        ent->procPid = 0;
        ent->procStat = PS_NO_INFO;
}

static 
int add_proc_attr(proc_watch_entry_t *ent, char *attrName, char *attrVal)
{
        if(strcmp(attrName, ATTR_NAME) == 0) {
                strncpy(ent->procName, attrVal,  PROC_NAME_LEN-1);
                ent->procName[PROC_NAME_LEN-1] = '\0';
        }
        else if (strcmp(attrName, ATTR_PID) == 0 ) {
                int val;
                char *endptr;
                
                val = strtol(attrVal, &endptr, 10);
                if(val == 0 && *endptr != '\0')
                        return 0;
                if(val < 0)
                        return 0;
                
                ent->procPid = val;
        }
        else if(strcmp(attrName, ATTR_STAT) == 0 ) {
                if(!strToProcStatus(attrVal, &ent->procStat))
                        return 0;
        }
        else {
                return 0;
        }
        return 1;
}
static 
char *get_proc_entry(char *str, proc_watch_entry_t *ent, int *res)
{
        char   tagName[128];
        char   attrName[128];
        char   attrVal[128];
        int    end, short_tag, close_tag;
        char  *ptr = str;

        *res = 0;
        ptr = get_tag_start(ptr, tagName, &close_tag);
        if(!ptr)
                return NULL;
        if(close_tag) 
                return str;
        
        if(strcmp(tagName, TAG_NAME) != 0)
                return str;
        init_proc_ent(ent);
        do {
                ptr = get_tag_attr(ptr, attrName, attrVal, &end);
                if(!ptr)
                        return NULL;
                
                add_proc_attr(ent, attrName, attrVal);
                
        } while(end == 0);

        ptr = get_tag_end(ptr, &short_tag);
        *res = 1;
        return ptr;
}

int  getProcWatchInfo(char *str, proc_watch_entry_t *procArr, int *size)
{
        char     *ptr;
        int       i;
        
        if(!str || !procArr || *size <= 0)
                return 0;
        
        ptr = str;

        char rootTag[128];
        int  closeTag;
        int  shortTag;
        
        ptr = get_tag_start(ptr, rootTag, &closeTag);
        if(!ptr) return 0;
        if(closeTag) return 0;
        if(strcmp(rootTag, ROOT_TAG) != 0)
                return 0;
        ptr = get_tag_end(ptr,& shortTag);
        if(!ptr) return 0;

        
        
        i = 0;
        int res;
        while(ptr != '\0' && i < *size) {
                ptr = get_proc_entry(ptr, &procArr[i], &res);
                if(!res && ptr) break;
                if(!ptr) return 0;
                
                ptr = eat_spaces(ptr);
                i++;
        }
        
        ptr = get_tag_start(ptr, rootTag, &closeTag);
        if(!ptr) return 0;
        if(strcmp(rootTag, ROOT_TAG) != 0)
                return 0;
        if(!closeTag) return 0;
        ptr = get_tag_end(ptr,& shortTag);
        if(!ptr)
                return 0;
        *size = i;
        return 1;
}


// Converting to a string a list of proc-watch entries
int  writeProcWatchInfo(proc_watch_entry_t *procArr, int size, int no_root, char *buff)
{
        char  *ptr = buff;
        
        if(!procArr || size <= 0 || !buff)
                return 0;

        // Printing the root element
        if(!no_root) {
                ptr += sprintf(ptr,"<%s> ", ROOT_TAG); 
        }
        
        for(int i=0; i<size ; i++) {
                proc_watch_entry_t *ent = &procArr[i];
                ptr += sprintf(ptr, "<%s %s=\"%s\" %s=\"%d\" %s=\"%s\" />",
                               TAG_NAME,
                               ATTR_NAME, ent->procName, 
                               ATTR_PID,  ent->procPid,
                               ATTR_STAT, procStatusStr(ent->procStat));
        }
        if(!no_root) {
                ptr += sprintf(ptr, "</%s> ", ROOT_TAG); 
        }
        
        return 1;
}


int checkProcPidFile(char *pidFile, proc_watch_entry_t *ent)
{
	int   fd;
	char  buff[128];
	pid_t pid;
	
	if(!pidFile || !ent)
		return 0;
	
	fd = open(pidFile, O_RDONLY);
	if(fd < 0) {
		if(errno == ENOENT) 
			ent->procStat = PS_NO_PID_FILE;
                else
                        ent->procStat = PS_ERR_PID_FILE;
                return 1;
        }

	int res;
	res = read(fd, buff, 127);
	close(fd);
	if(res <= 0 ){
                ent->procStat = PS_ERR_PID_FILE;
		return 1;
	}

        buff[res] = '\0';
	int val;
	char *endptr;
	
	val = strtol(buff, &endptr, 10);
	if(val == 0 && *endptr != '\0') {
                ent->procStat = PS_ERR_NO_PID;
                return 1;
	}
	pid = (pid_t)val;
        ent->procPid = pid;
	res = kill(pid, 0);
	if(res == -1) {
		if(errno == ESRCH)
			ent->procStat = PS_NO_PROC;
                else
                        ent->procStat = PS_ERR;
                return 1;
	}
        
        ent->procStat = PS_OK;
        return 1;
}

