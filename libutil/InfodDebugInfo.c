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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <info.h>
#include <parse_helper.h>
#include <InfodDebugInfo.h>

#define ROOT_TAG          ITEM_INFOD_DEBUG_NAME
#define ATTR_CHILD_NUM    "nchild"

static
int add_idbg_attr(infod_debug_info_t *idbg, char *attrName, char *attrVal)
{

        if(strcmp(attrName, ATTR_CHILD_NUM) == 0) {
                int val;
              char *endptr;
              
              val = strtol(attrVal, &endptr, 10);
              if(val == 0 && *endptr != '\0')
                      return 0;
              if(val < 0)
                      return 0;
              
                idbg->childNum = val;    
        }
        else {
                return 0;
        }
        return 1;
}

int getInfodDebugInfo(char *str, infod_debug_info_t *idbg)
{
        char     *ptr;
        
        if(!str || !idbg)
                return 0;
        
        ptr = str;
        
        char   rootTag[128];
        char   tagContent[128];
        char   attrName[128];
        char   attrVal[128];
        int    end;
        int    closeTag;
        int    shortTag;
        
        ptr = get_tag_start(ptr, rootTag, &closeTag);
        if(!ptr) return 0;
        if(closeTag) return 0;
        if(strcmp(rootTag, ROOT_TAG) != 0)
                return 0;

        do {
                ptr = get_tag_attr(ptr, attrName, attrVal, &end);
                if(!ptr)
                        return 0;
                
                add_idbg_attr(idbg, attrName, attrVal);
                
        } while(end == 0);

        ptr = get_tag_end(ptr,& shortTag);
        if(!ptr) return 0;

        // Getting the content of the tag
        ptr = get_upto_tag_start(ptr, tagContent);
        if(!ptr) return 0;

        int val;
        char *endptr;
        
        val = strtol(tagContent, &endptr, 10);
        if(val == 0 && *endptr != '\0')
                return 0;
        if(val < 0)
                return 0;
        
        idbg->uptimeSec = val;
        
        // Getting the end tag
        ptr = get_tag_start(ptr, rootTag, &closeTag);
        if(!ptr) return 0;
        if(strcmp(rootTag, ROOT_TAG) != 0)
                return 0;
        if(!closeTag) return 0;
        ptr = get_tag_end(ptr,& shortTag);
        if(!ptr)
                return 0;
        return 1;

}
