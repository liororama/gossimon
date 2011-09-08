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


#define _XOPEN_SOURCE  600   /* or #define _ISOC99_SOURCE */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <libxml/parser.h>
#include <libxml/tree.h>


#include <parse_helper.h>
#include <info.h>
#include <JMigrateInfo.h>

#define ROOT_TAG      ITEM_JMIGRATE_NAME

char *jmigrate_status_str[] = {
        "error", "host", "vm", NULL };

char jmigrate_status_char[] = " hv ";
jmigrate_status_t jmigrate_status_arr[] = {
        JMIGRATE_ERROR, JMIGRATE_HOST, JMIGRATE_VM,
};

char *jmigrateStatusStr(jmigrate_status_t st) {
     if(st < JMIGRATE_ERROR || st > JMIGRATE_END)
          return jmigrate_status_str[JMIGRATE_ERROR];
     return jmigrate_status_str[st];
}        

char jmigrateStatusChar(jmigrate_status_t st) {
     if(st < JMIGRATE_ERROR || st > JMIGRATE_END)
          return jmigrate_status_char[JMIGRATE_ERROR];
     return jmigrate_status_char[st];
}        


// Parsing the string and verifying that the status is ok
// In case of error the return value is 0
static int getJMigrateStatus(char *str, jmigrate_status_t *stat)
{
     int    res = 0;
     char   buff[256];
     char  *ptr;
     
     strcpy(buff, str);
     buff[255] = '\0';

     ptr = eat_spaces(buff);
     trim_spaces(ptr);
     
     if(strcmp(ptr, JMIGRATE_HOST_STR) == 0) {
             *stat = JMIGRATE_HOST;
             res = 1;
     }
     else if (strcmp(ptr, JMIGRATE_VM_STR) == 0) {
             *stat = JMIGRATE_VM;
             res = 1;
     }
     return res;
}

static int getFloat(char *str, float *value)
{
     char   buff[256];
     char  *ptr;
     
     strcpy(buff, str);
     buff[255] = '\0';
     
     ptr = eat_spaces(buff);
     trim_spaces(ptr);

     float val;
     char *endptr;
     
     val = strtof(ptr, &endptr);
     if(val == 0 && *endptr != '\0')
          return 0;
     
     *value = val;
     return 1;
}

static int getInt(char *str, int *value)
{
     char   buff[256];
     char  *ptr;
     
     strcpy(buff, str);
     buff[255] = '\0';
     
     ptr = eat_spaces(buff);
     trim_spaces(ptr);

     int val;
     char *endptr;
     
     val = strtol(ptr, &endptr, 10);
     if(val == 0 && *endptr != '\0')
          return 0;
     
     *value = val;
     return 1;
}

/* <jmigrate type="host"> */
/*   <machines num="1"> */
/*       <vm> */
/*           <id>bla</id> */
/*           <mem>256</mem> */
/*           <home>cmos-16</home> */
/*       </vm> */
/*   </machines> */
/* </jmigrate> */

int parseVMInfo(xmlDocPtr doc, xmlNode *node, jmigrate_info_t *jinfo) {
     xmlNode *cur;
     xmlChar *value;
     int res = 1;
     
     for (cur = node->children ; cur ; cur = cur->next) {
          if (cur->type != XML_ELEMENT_NODE) continue;
          //printf("element: %s\n", cur->name);
          
          if(!xmlStrcmp(cur->name, (const xmlChar *)JMIG_ELEMENT_ID)) {
               value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
               if(!value) return 0;
               strncpy(jinfo->vmInfoArr[jinfo->vmNum].id, (char *)value, JMIGRATE_MAX_ID_SIZE-1);
               xmlFree(value);
          }
          else if(!xmlStrcmp(cur->name, (const xmlChar *)JMIG_ELEMENT_HOME)) {
               value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
               if(!value) return 0;
               strncpy(jinfo->vmInfoArr[jinfo->vmNum].home, (char *)value, JMIGRATE_MAX_HOST_SIZE-1);
               xmlFree(value);
          }
          else if(!xmlStrcmp(cur->name, (const xmlChar *)JMIG_ELEMENT_HOST)) {
               value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
               if(!value) return 0;
               strncpy(jinfo->vmInfoArr[jinfo->vmNum].host, (char *)value, JMIGRATE_MAX_HOST_SIZE-1);
               xmlFree(value);
          }
          else if(!xmlStrcmp(cur->name, (const xmlChar *)JMIG_ELEMENT_CLIENT)) {
               value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
               if(!value) return 0;
               strncpy(jinfo->vmInfoArr[jinfo->vmNum].client, (char *)value, JMIGRATE_MAX_HOST_SIZE-1);
               xmlFree(value);
          }
          
          else if(!xmlStrcmp(cur->name, (const xmlChar *)JMIG_ELEMENT_MEM)) {
               value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
               if(!value) return 0;
               int memVal;
               res = getInt((char *) value, &memVal); 
               //printf("Mem: %d\n", memVal);
               xmlFree(value); 
               if(!res) return 0; 
               jinfo->vmInfoArr[jinfo->vmNum].mem = memVal;
          }
     }
     return res;
}


int parseMachineTag(xmlDocPtr doc, xmlNode *node, jmigrate_info_t *jinfo) {
     int      res;
     xmlNode *cur;
     xmlChar *vmNumAttr = NULL;

     vmNumAttr = xmlGetProp(node, (xmlChar *)JMIG_ATTR_VM_NUM);
     if(!vmNumAttr) {
          return 0;
     }
     int vmNum;
     res = getInt((char *)vmNumAttr, &vmNum);
     xmlFree(vmNumAttr);
     if(!res)
          return 0;
     
     //printf("vmNum = %d\n", vmNum);
     if(vmNum < 0)
          return 0;
     jinfo->vmNum = vmNum;

     // There is no need to continue for a host with 0 vms on it.
     if(jinfo->vmNum == 0)
          return 1;

     jinfo->vmInfoArr = malloc(vmNum * sizeof(vm_info_t));
     if(!jinfo->vmInfoArr)
          return 0;
     jinfo->vmNum = 0;

     for (cur = node->children ; cur ; cur = cur->next) {
          if (cur->type != XML_ELEMENT_NODE) continue;
          //printf("element: %s\n", cur->name);
  
          if(!xmlStrcmp(cur->name, (const xmlChar *)JMIG_ELEMENT_VM_INFO)) {
               if(jinfo->vmNum == vmNum)
                    return 0;
               if(!parseVMInfo(doc, cur, jinfo))
                    return 0;

               jinfo->vmNum++;
          }
     }
     return 1;
}

static int
fillJMigrateInfo(xmlDocPtr doc, jmigrate_info_t *jinfo)
{
     xmlNode    *root = NULL;
     xmlNode    *node = NULL;
     xmlChar    *value;
     jmigrate_status_t stat;
     int         res = 0;
     
     root = xmlDocGetRootElement(doc);

     xmlChar *typeAttr = NULL;
     typeAttr = xmlGetProp(root, (xmlChar *)JMIG_ATTR_TYPE);

     // In case there is no type attribute, probably an error element
     if(!typeAttr) {
          jinfo->status = JMIGRATE_ERROR;
          for (node = root->children ; node ; node = node->next) {
               if (node->type != XML_ELEMENT_NODE) continue;
               if(!xmlStrcmp(node->name, (const xmlChar *)JMIG_ELEMENT_ERROR)) {
                    
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    if(!value) return 0;
                    strncpy(jinfo->errMsg, (char *)value, 31);
                    jinfo->errMsg[31]='\0';
                    xmlFree(value);
                    //printf("Got error %s\n", jinfo->errMsg);
                    break;
               }
          }
          return 1;
     }
     
     //printf("Got type:(%s)\n", typeAttr);
     
     if(!xmlStrcmp(typeAttr, (const xmlChar *)JMIGRATE_HOST_STR)) {
          jinfo->status = JMIGRATE_HOST;
          for (node = root->children ; node ; node = node->next) {
               if (node->type != XML_ELEMENT_NODE) continue;
               
               if(!xmlStrcmp(node->name, (const xmlChar *)JMIG_ELEMENT_MACHINE)) {
                    res = parseMachineTag(doc, node, jinfo);
                    if(!res) jinfo->status = JMIGRATE_ERROR;
                    break;
               }
          }
     }
     else if(!xmlStrcmp(typeAttr, (const xmlChar *)JMIGRATE_VM_STR)) {
          jinfo->status = JMIGRATE_VM;
          jinfo->vmInfoArr = malloc(1 * sizeof(vm_info_t));
          if(!jinfo->vmInfoArr)
               return 0;
          jinfo->vmNum = 0;
          if((res = parseVMInfo(doc, root, jinfo)))
               jinfo->vmNum++;
     }
     else {
          jinfo->status = JMIGRATE_ERROR;
          res = 0;
     }
     xmlFree(typeAttr);
     return res;
          
     return 1;
}


int parseJMigrateInfo(char *xmlStr, jmigrate_info_t *jinfo)
{
     int         res = 0;
     xmlDocPtr   doc; 
     
     doc = xmlReadMemory(xmlStr, strlen(xmlStr), "noname.xml", NULL, 0);
     if (doc == NULL) {
             //fprintf(stderr, "Failed to parse document\n");
             return 0;
     }
     
     /*Get the root element node */
     bzero(jinfo, sizeof(jmigrate_info_t));
     res = fillJMigrateInfo(doc, jinfo);
     
     /*free the document */
     xmlFreeDoc(doc);
     
     /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();
    return res;
}


void freeJMigrateInfo(jmigrate_info_t *jinfo) {
     if(!jinfo)
          return;
     
     for(int i=0 ; i< jinfo->vmNum; i++)
          free(jinfo->vmInfoArr);
     return;
}

