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
#include <string.h>

#include <parse_helper.h>
#include <info.h>
#include <FreezeInfo.h>

#define ROOT_TAG      ITEM_FREEZE_INFO_NAME
#define ATTR_TOTAL    "total"
#define ATTR_FREE     "free"
#define ATTR_STAT     "stat"

char *frzinf_status_str[] = {
        "init",
        "error",
        "ok",
        NULL
};

freeze_info_status_t frzinf_status_arr[] = {
	FRZINF_INIT,
	FRZINF_ERR,
	FRZINF_OK,
};

char *freezeInfoStatusStr(freeze_info_status_t st) {
        if(st < 0 || st > FRZINF_OK)
                return NULL;
        return frzinf_status_str[st];
}        

int  strToFreezeInfoStatus(char *str, freeze_info_status_t *stat) {
        int i;

        if(!str) return 0;
        for(i = 0 ; frzinf_status_str[i] ; i++) {
                if(strcmp(frzinf_status_str[i], str) == 0) {
                        * stat = frzinf_status_arr[i];
                        return 1;
                }
        }
        return 0;
}


static 
int add_freeze_info_attr(freeze_info_t *fi, char *attrName, char *attrVal)
{
        if(strcmp(attrName, ATTR_STAT) == 0) {
                if(!strToFreezeInfoStatus(attrVal, &fi->status))
                        return 0;
        }
        else if (strcmp(attrName, ATTR_TOTAL) == 0 ) {
                int val;
                char *endptr;
                
                val = strtol(attrVal, &endptr, 10);
                if(val == 0 && *endptr != '\0')
                        return 0;
                if(val < 0)
                        return 0;
                
                fi->total_mb = val;
        }
        else if (strcmp(attrName, ATTR_FREE) == 0 ) {
                int val;
                char *endptr;
                
                val = strtol(attrVal, &endptr, 10);
                if(val == 0 && *endptr != '\0')
                        return 0;
                if(val < 0)
                        return 0;
                
                fi->free_mb = val;
        }
        else {
                return 0;
        }
        return 1;
}

int  getFreezeInfo(char *str, freeze_info_t *fi)
{
        char   tagName[128];
        char   attrName[128];
        char   attrVal[128];
        int    end, short_tag, close_tag;
        char  *ptr = str;
        
        ptr = get_tag_start(ptr, tagName, &close_tag);
        if(!ptr)
                return 0;
        if(close_tag) 
                return 0;
        
        if(strcmp(tagName, ROOT_TAG) != 0)
                return 0;

        do {
                ptr = get_tag_attr(ptr, attrName, attrVal, &end);
                if(!ptr)
                        return 0;
                
                add_freeze_info_attr(fi, attrName, attrVal);
                
        } while(end == 0);
        
        ptr = get_tag_end(ptr, &short_tag);
        if(!short_tag)
                return 0;
        return 1;
}


// Converting to a string a list of proc-watch entries
int  writeFreezeInfo(freeze_info_t *fi, char *buff)
{
        char  *ptr = buff;
        
        if(!fi || !buff)
                return 0;
        
        ptr += sprintf(ptr, "<%s %s=\"%s\" %s=\"%d\" %s=\"%d\" />",
                       ROOT_TAG,
                       ATTR_STAT,  freezeInfoStatusStr(fi->status),
                       ATTR_TOTAL, fi->total_mb, 
                       ATTR_FREE,  fi->free_mb);
        return 1;
}
