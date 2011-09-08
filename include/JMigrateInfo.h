/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __JMIG_INFO
#define __JMIG_INFO

typedef enum {
        JMIGRATE_ERROR = 0,
        JMIGRATE_HOST,
        JMIGRATE_VM,
        JMIGRATE_END,
} jmigrate_status_t;

#define JMIGRATE_HOST_STR         "host"
#define JMIGRATE_VM_STR           "vm"

#define JMIGRATE_MAX_ID_SIZE 256
#define JMIGRATE_MAX_HOST_SIZE 128
typedef struct {
     char       id[JMIGRATE_MAX_ID_SIZE];
     char       home[JMIGRATE_MAX_HOST_SIZE];
     char       host[JMIGRATE_MAX_HOST_SIZE];
     char       client[JMIGRATE_MAX_HOST_SIZE];
     int        mem;
} vm_info_t;


typedef struct {
     jmigrate_status_t status;
     int               vmNum;
     vm_info_t        *vmInfoArr;
     
     char             errMsg[32];
} jmigrate_info_t;

#define JMIG_ELEMENT_ERROR         "error"
#define JMIG_ATTR_TYPE             "type"
#define JMIG_ELEMENT_STATUS        "status"
#define JMIG_ELEMENT_MACHINE       "machines"
#define JMIG_ATTR_VM_NUM           "num"
#define JMIG_ELEMENT_VM_INFO       "vm"
#define JMIG_ELEMENT_ID            "id"
#define JMIG_ELEMENT_HOME          "home"
#define JMIG_ELEMENT_HOST          "host"
#define JMIG_ELEMENT_MEM           "mem"
#define JMIG_ELEMENT_CLIENT        "client"

#define JMIGRATE_NAME              "jmigrate"
#define JMIGRATE_MARK              "+"

//int writeErrorJmigXml(char *xmlStr, char *errorMsg);
char *jmigrateStatusStr(jmigrate_status_t st);
char jmigrateStatusChar(jmigrate_status_t st);

int parseJMigrateInfo(char *xmlStr, jmigrate_info_t *jinfo);
void freeJMigrateInfo(jmigrate_info_t *jinfo);

#endif
