/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2010 Cluster Logic Ltd

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright-ClusterLogic.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

/******************************************************************************
 *
 * Author(s): Amar Lior
 *
 *****************************************************************************/

#ifndef __CONF_FILE_READER
#define __CONF_FILE_READER

typedef struct {
        char  *varName;
        char  *varValue;
}conf_var_t; 


#include <glib-2.0/glib.h>

struct conf_file_struct{
        char             errMsg[512];
        GHashTable       *validVarHash;
        GHashTable       *varHash;
};

typedef struct conf_file_struct *conf_file_t;

conf_file_t cf_new(char **validVarNames);
conf_file_t cf_load(char *fileName, char **validVarNames);
void cf_free(conf_file_t cf);
int  cf_parseFile(conf_file_t cf, char *fileName);
int  cf_parse(conf_file_t cf, char *confStr);
char *cf_error(conf_file_t cf);
int  cf_getVar(conf_file_t cf, char *varName, conf_var_t *varEnt); 
int  cf_getIntVar(conf_file_t cf, char *varName, int *varInt); 
int  cf_getListVar(conf_file_t cf, char *varName, char *seperator, GPtrArray *gptr);
void cf_getAllVars(conf_file_t cf);

#endif
