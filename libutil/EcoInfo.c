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
#include <EcoInfo.h>

#define ROOT_TAG      ITEM_ECONOMY_STAT_NAME

char *node_eco_status_str[] = {
        "error", "on", "off", "home", "protected", NULL };

char node_eco_status_char[] = " O HP";
node_economy_status_t node_eco_status_arr[] = {
        ECONOMY_ERROR, ECONOMY_ON, ECONOMY_OFF, ECONOMY_HOME, ECONOMY_PROTECTED,
};

char *ecoStatusStr(node_economy_status_t st) {
        if(st < ECONOMY_ERROR || st > ECONOMY_END)
                return node_eco_status_str[ECONOMY_ERROR];
        return node_eco_status_str[st];
}        

char ecoStatusChar(node_economy_status_t st) {
        if(st < ECONOMY_ERROR || st > ECONOMY_END)
                return node_eco_status_char[ECONOMY_ERROR];
        return node_eco_status_char[st];
}        


// Parsing the string and verifying that the status is ok
// In case of error the return value is 0
static int getEcoStatus(char *str, node_eco_stat_t *stat)
{
     int    res = 0;
     char   buff[256];
     char  *ptr;
     
     strcpy(buff, str);
     buff[255] = '\0';

     ptr = eat_spaces(buff);
     trim_spaces(ptr);
     
     if(strcmp(ptr, NODE_STAT_ON_STR) == 0) {
          *stat = ECONOMY_ON;
          res = 1;
     }
     else if (strcmp(ptr, NODE_STAT_OFF_STR) == 0) {
          *stat = ECONOMY_OFF;
          res = 1;
     }
     else if (strcmp(ptr, NODE_STAT_HOME_STR) == 0) {
          *stat = ECONOMY_HOME;
          res = 1;
     }
     else if (strcmp(ptr, NODE_STAT_PROTECTED_STR) == 0) {
             *stat = ECONOMY_PROTECTED;
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

static int
fillEconomyInfo(xmlDocPtr doc, economy_info_t *eInfo)
{
     xmlNode    *root = NULL;
     xmlNode    *node = NULL;
     xmlChar    *value;
     node_eco_stat_t ecoStat;
     float       floatVal;
     int         res = 0;
     
     root = xmlDocGetRootElement(doc);

     for (node = root->children ; node ; node = node->next)
          if (node->type == XML_ELEMENT_NODE) {
               //printf("node type: Element, name: %s\n", node->name);
               if(!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_ERROR)) {
                    eInfo->status = ECONOMY_ERROR;
                    break;
               }
               if(!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_STATUS)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    res = getEcoStatus((char *)value, &ecoStat);
                    xmlFree(value);
                    if(!res) return 0;
                    eInfo->status = ecoStat;
               }
               else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_MIN_PRICE)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    res = getFloat((char *)value, &floatVal);
                    xmlFree(value);
                    if(!res) return 0;
                    eInfo->minPrice = floatVal;
               }
               else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_CURR_PRICE)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    res = getFloat((char *) value, &floatVal);
                    xmlFree(value);
                    if(!res) return 0;
                    eInfo->currPrice = floatVal;
               }
               else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_TIME_LEFT)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    res = getFloat((char *)value, &floatVal);
                    xmlFree(value);
                    if(!res) return 0;
                    eInfo->timeLeft = floatVal;

               }
               else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_SCHED_PRICE)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    res = getFloat((char *)value, &floatVal);
                    xmlFree(value);
                    if(!res) return 0;
                    eInfo->schedPrice = floatVal;
               }
               else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_NEXT_PRICE)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    res = getFloat((char *)value, &floatVal);
                    xmlFree(value);
                    if(!res) return 0;
                    eInfo->nextPrice = floatVal;
               }
               else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_INC_PRICE)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    res = getFloat((char *)value, &floatVal);
                    xmlFree(value);
                    if(!res) return 0;
                    eInfo->incPrice = floatVal;
               }
               else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_PROTECT_TIME)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    res = getFloat((char *)value, &floatVal);
                    xmlFree(value);
                    if(!res) return 0;
                    eInfo->protectedTime = (int)floatVal;
               }  else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_SCHED_HOST)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    if(!value) return 0;
                    strncpy(eInfo->schedHost, (char *)value, 500);
                    xmlFree(value);
               }  else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_CURR_HOST)) {
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    if(!value) return 0;
                    strncpy(eInfo->currHost, (char *)value, 500);
                    xmlFree(value);
               }  else if (!xmlStrcmp(node->name, (const xmlChar *)ECO_ELEMENT_USER)) {
                    // Fake fair share uid is used for the debugging of the economy fair share
                    value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    if(!value) return 0;
                    strncpy(eInfo->user, (char *)value, USER_MAX_LEN - 1);
                    eInfo->user[USER_MAX_LEN - 1] = '\0';
                    xmlFree(value);
               }
          }
     return 1;
}




int parseEconomyInfo(char *xmlStr, economy_info_t *eInfo)
{
     int         res = 0;
     xmlDocPtr   doc; 

     if(!xmlStr) return 0;
     doc = xmlReadMemory(xmlStr, strlen(xmlStr), "noname.xml", NULL, 0);
     if (doc == NULL) {
             //fprintf(stderr, "Failed to parse document\n");
             return 0;
     }
     
     /*Get the root element node */
     bzero(eInfo, sizeof(economy_info_t));
     res = fillEconomyInfo(doc, eInfo);
     
     /*free the document */
     xmlFreeDoc(doc);
     
     /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();
    return res;
}


