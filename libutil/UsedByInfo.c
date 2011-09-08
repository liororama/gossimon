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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <msx_debug.h>
#include <msx_error.h>
#include <parse_helper.h>
#include <info.h>
#include <UsedByInfo.h>

// We need to get <name> <number> <number>
/*
  The xml format: Possible senarios

  <usedby stat="grid-use">
     <c1><u1 n=10 m=20/>
         <u2 n=10 m=20/>
     </c1>    
     <c2><u3 n=10 m=20/></c2>
  </usedby>

  <usedby stat="local-use">
     <local><u1 n=10 m=20/>
            <u2 n=10 m=20/>
     </local>    
  </usedby>

  <usedby stat="free" />

  <usedby stat="error" />

*/

#define ROOT_TAG    ITEM_USEDBY_NAME
#define STAT_ATTR   "stat"

#define USER_TAG    "u"
#define UID_ATTR    "uid"

char *node_usage_status_str[] = {
        "INIT", "LOCAL", "GRID", "FREE", "ERROR", NULL };

node_usage_status_t node_usage_status_arr[] = {
        UB_STAT_INIT, UB_STAT_LOCAL_USE, UB_STAT_GRID_USE, UB_STAT_FREE,
        UB_STAT_ERROR};

char *usageStatusStr(node_usage_status_t st) {
        return node_usage_status_str[st];
}        

node_usage_status_t strToUsageStatus(char *str) {
        int i;

        if(!str) return UB_STAT_ERROR;
        for(i = 0 ; node_usage_status_str[i] ; i++) {
                if(strcmp(node_usage_status_str[i], str) == 0)
                        return node_usage_status_arr[i];
                
        }
        return UB_STAT_ERROR;
}

static int verifyFormat(char **splitedLine, used_by_entry_t *e) {
        int i;
        int itemNum;
        
        for(itemNum=0 ; splitedLine[itemNum] != NULL; itemNum++);

        if(itemNum <2)
                return 0;
        strcpy(e->clusterName, splitedLine[0]);
        // First string should be a name, but all the rest numbers
        for(i=1 ; i<itemNum ; i++) {
                int val;
                char *endptr;

                trim_spaces(splitedLine[i]);
                val = strtol(splitedLine[i], &endptr, 10);
                if(val == 0 && *endptr != '\0')
                        return 0;
                if(val < 0)
                        return 0;
                
                e->uid[i-1] = val;
        }
        e->uidNum = itemNum - 1;
        return 1;
}

char *get_using_user(char *str, used_by_entry_t *ent, int *res)
{
        char   tagName[128];
        int    short_tag, close_tag, end;
        char  *ptr = str;
        
        *res = 0;

        
        ptr = get_tag_start(ptr, tagName, &close_tag);
        if(!ptr)
                return NULL;
        // Got into an unexpected closing tag, this should be the end
        // of the cluster
        if(close_tag) 
                return str;

        if(strcmp(tagName, USER_TAG) != 0)
                return NULL;
        
        char attrName[128], attrVal[128];
        
        ptr = get_tag_attr(ptr, attrName, attrVal, &end);
        if(!ptr) return 0;
        // There should not be more than one attribute
        if(end)
                return 0;
        
        if(strcmp(attrName, UID_ATTR) != 0)
                return 0;

        int val;
        char *endptr;
        
        val = strtol(attrVal, &endptr, 10);
        if(val == 0 && *endptr != '\0')
                return NULL;
        if(val < 0)
                return NULL;
        
        ent->uid[ent->uidNum] = val;
        ent->uidNum++;

        // End of uid tag
        ptr = get_tag_end(ptr, &short_tag);
        if(!ptr)
                return NULL;
        if(!short_tag)
                return NULL;
        *res = 1;
        return ptr;
}
                

// Return res = 0 but ptr != NULL when encountering a closing tag
char *get_using_cluster(char *str, used_by_entry_t *ent, int *res)
{
        int i = 0;
        char   tagName[128];
        int    short_tag, close_tag;
        char  *ptr = str;
        int    tmpRes;
        
        *res = 0;
        ptr = get_tag_start(ptr, tagName, &close_tag);
        if(!ptr)
                return NULL;
        if(close_tag) 
                return str;
        
        strcpy(ent->clusterName, tagName);
        ptr = get_tag_end(ptr, &short_tag);
        if(!ptr)
                return NULL;
        // A using cluster tag can not be a short one
        if(short_tag)
                return NULL;
        
        // Getting the uids
        ent->uidNum = 0;

        while(ptr != '\0' && i < MAX_USING_UIDS_PER_CLUSTER)
        {
                ptr = get_using_user(ptr, ent, &tmpRes);
                if(!tmpRes && ptr) break;
                if(!ptr) return 0;
                ptr = eat_spaces(ptr);
                i++;
        } 

        // Getting the closing cluster tag
        ptr = get_tag_start(ptr, tagName, &close_tag);
        if(!ptr)
                return NULL;
        if(!close_tag) 
                return str;
        if(strcmp(ent->clusterName, tagName) != 0)
                return NULL;
        ptr = get_tag_end(ptr, &short_tag);
        if(!ptr)
                return NULL;
        // The closing part of the closing tag can not be a short tag
        if(short_tag)
                return NULL;
                
        *res = 1;
        return ptr;

}
                
node_usage_status_t getUsageInfo(char *str, used_by_entry_t *arr, int *size)
{
        int       res = 0;
        node_usage_status_t  stat ;
        int       i;
        char     *ptr;
        
        if(!str || !arr || *size <= 0)
                return UB_STAT_ERROR;

        
        ptr = str;

        char rootTag[128], attrName[128], attrVal[128];
        int  closeTag;
        int  shortTag;
        int  end;
        
        ptr = get_tag_start(ptr, rootTag, &closeTag);
        if(!ptr) return 0;
        if(closeTag) return 0;
        if(strcmp(rootTag, ROOT_TAG) != 0)
                return 0;
        
        // Get the usedby status 
        
        ptr = get_tag_attr(ptr, attrName, attrVal, &end);
        if(!ptr) return 0;
        // There should not be more than one attribute
        if(end)
                return 0;
        
        if(strcmp(attrName, STAT_ATTR) != 0)
                return 0;
        stat = strToUsageStatus(attrVal);
        
        
        ptr = get_tag_end(ptr, &shortTag);
        if(!ptr) return 0;
        if(shortTag) {
                *size = 0;
                return stat;
        }
        
        
        i = 0;
        while(ptr != '\0' && i < *size) {
                ptr = get_using_cluster(ptr, &arr[i], &res);
                if(!res && ptr) break;
                if(!ptr) return 0;
                ptr = eat_spaces(ptr);
                i++;
        }

        ptr = get_tag_start(ptr, rootTag, &closeTag);
        if(!ptr) return 0;
        if(!closeTag) return 0;
        if(strcmp(rootTag, ROOT_TAG) != 0)
                return 0;
        
        *size = i;
        return stat;
}



node_usage_status_t getUsageInfo_old(char *str, used_by_entry_t *arr, int *size)
{
        node_usage_status_t  res = UB_STAT_INIT;
        int       i;
        int       lineBuffSize = 200;
        char      lineBuff[200];

        char     *buffCurrPos = str;
        char     *splitedLine[40];

        splitedLine[0] = NULL;
        
        if(!str || !arr || *size <= 0)
                return UB_STAT_ERROR;

        // Reading the first line (status line)
        buffCurrPos = buff_get_line(lineBuff, lineBuffSize, buffCurrPos);
	
        // Retrieving the status 
        trim_spaces(lineBuff);
        res = strToUsageStatus(lineBuff);

        // Continue further only if we expect more info
        if(res != UB_STAT_GRID_USE && res != UB_STAT_LOCAL_USE)
                goto out;

        if(!buffCurrPos) {
		goto out;
	}
        
        i = 0;
        splitedLine[0] = NULL;
        while( buffCurrPos && !(*buffCurrPos == '\0') )
	{
                int j;
                // We free the splited line from previous round
                for(j=0; splitedLine[j] != NULL; j++) {
                        free(splitedLine[j]);
                        splitedLine[j] = NULL;
                }

                // Getting one line of input
		buffCurrPos = buff_get_line(lineBuff, lineBuffSize, buffCurrPos);
                
                trim_spaces(lineBuff);
                // Skeeping empty lines
                if(strlen(lineBuff) == 0)
                        continue;
                split_str(lineBuff, splitedLine, 40, " \t");
                // verify the format of each string
                if(!verifyFormat(splitedLine, &arr[i])) {
                        res = UB_STAT_ERROR;
                        goto out;
                }
                i++;
                // Checking we have enough memory
                if(i>= *size)
                        break;
        }
        *size = i;
 out:
        for(i=0; splitedLine[i] != NULL; i++)
                free(splitedLine[i]);
        
        return res;
}

/*
 * Writing usage information into a buffer, current format
 *
 * GRID_USE
 * cluster-name-1 uid1 uid2 ...
 * cluster-name-2 uid1 uid2 ...
 * ...
 *
 * or
 *
 * FREE
 *
 * or
 *
 * LOCAL_USE
 *
 * Returning the length of the new string
 */
int writeUsageInfo(node_usage_status_t stat, used_by_entry_t *arr,
                   int arrSize, char *buff)
{

        int                i=0, j;
        //int                len;
        used_by_entry_t   *uce;
        char              *currPtr = buff;
        //char               line[256];

        // First we print the status line
        currPtr += sprintf(currPtr, "<%s %s=\"%s\" ",
                           ITEM_USEDBY_NAME, STAT_ATTR, usageStatusStr(stat));

        // Now specific information for each status
        switch(stat) {
            case UB_STAT_GRID_USE:
            case UB_STAT_LOCAL_USE:
                    currPtr += sprintf(currPtr, ">");
                    for(i = 0 ; i< arrSize ; i++) {
                            uce = &arr[i];
                            currPtr += sprintf(currPtr, "<%s>", uce->clusterName);
                            for(j = 0 ; j< uce->uidNum ; j++) {
                                    currPtr += sprintf(currPtr, "<%s %s=\"%d\" />",
                                                       USER_TAG, UID_ATTR, uce->uid[j]);
                            }
                            currPtr += sprintf(currPtr, "</%s>\n", uce->clusterName);
                    }
                    currPtr += sprintf(currPtr, "</%s>", ITEM_USEDBY_NAME);
                    break;
            default:
                    currPtr += sprintf(currPtr, " />");
                    break;
        }
        return currPtr - buff;
}


int writeUsageInfo_old(node_usage_status_t stat, used_by_entry_t *arr,
                   int arrSize, char *buff)
{
        int                i=0, j;
        int                len;
        used_by_entry_t   *uce;
        char              *currPtr = buff;
        //char               line[256];

        // First we print the status line
        len = sprintf(currPtr, "%s\n", usageStatusStr(stat));
        currPtr += len;

        // Now specific information for each status
        switch(stat) {
            case UB_STAT_GRID_USE:
            case UB_STAT_LOCAL_USE:
                    for(i = 0 ; i< arrSize ; i++) {
                            uce = &arr[i];
                            len = sprintf(currPtr, "%s ", uce->clusterName);
                            currPtr += len;
                            for(j = 0 ; j< uce->uidNum ; j++) {
                                    len = sprintf(currPtr, "%d ", uce->uid[j]);
                                    currPtr += len;
                            }
                            len = sprintf(currPtr, "\n");
                            currPtr += len;
                    }
                    break;
            default:
                    break;
        }
        return currPtr - buff;
}
