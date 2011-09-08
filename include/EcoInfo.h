/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __ECO_INFO
#define __ECO_INFO

typedef enum {
        ECONOMY_ERROR = 0,
        ECONOMY_ON,
        ECONOMY_OFF,
        ECONOMY_HOME,
        ECONOMY_PROTECTED,
        ECONOMY_END,
} node_economy_status_t;

#define NODE_STAT_ON_STR         "on"
#define NODE_STAT_OFF_STR        "off"
#define NODE_STAT_HOME_STR       "home"
#define NODE_STAT_PROTECTED_STR  "protected"
typedef node_economy_status_t node_eco_stat_t;

#define USER_MAX_LEN     (15)

typedef struct {
     node_eco_stat_t   status;
     float             timeLeft;
     float             minPrice;
     float             currPrice;
     float             nextPrice;
     float             schedPrice;
     float             incPrice;
     int               protectedTime;
     char              user[USER_MAX_LEN];  // This is for the fair share debugging
     char              schedHost[500];
     char              currHost[500];
} economy_info_t;

#define ECO_ELEMENT_ERROR         "error"
#define ECO_ELEMENT_STATUS        "status"
#define ECO_ELEMENT_TIME_LEFT     "on-time"
#define ECO_ELEMENT_MIN_PRICE     "min-price"
#define ECO_ELEMENT_CURR_PRICE    "curr-price"
#define ECO_ELEMENT_NEXT_PRICE    "next-price"

#define ECO_ELEMENT_SCHED_PRICE   "sched-price"
#define ECO_ELEMENT_INC_PRICE     "inc-price"
#define ECO_ELEMENT_PROTECT_TIME  "protect-time"

#define ECO_ELEMENT_USER          "user"

#define ECO_ELEMENT_SCHED_HOST    "sched-host"
#define ECO_ELEMENT_CURR_HOST     "curr-host"

#define ASSIGND_NAME              "assignd"
#define ASSIGND_MARK              "+"
#define ECONOMYD_NAME             "economyd"
#define ECONOMYD_MARK             "$"

//int writeErrorEcoXml(char *xmlStr, char *errorMsg);
char *ecoStatusStr(node_economy_status_t st);
char ecoStatusChar(node_economy_status_t st);

int parseEconomyInfo(char *xmlStr, economy_info_t *eInfo);


#endif
