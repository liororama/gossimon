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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <regex.h>

#include <glib-2.0/glib.h>
#include <errno.h>

#include <ModuleLogger.h>

#include <msx_error.h>
#include <msx_debug.h>

int match(const char *string, char *pattern) {
     int status;
     int err;
     regex_t re;
     char buff[128];
     debug_lb(MAP_DEBUG, "Inside match ... [%s]\n", pattern);
     if((err = regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB)) != 0) {
	  regerror( err, &re, buff, 127 );
	  fprintf(stderr, "Error: %s\n", buff);
	  return 0; 
     }
     status = regexec(&re, string, (size_t)0, NULL, 0);
     debug_lb(MAP_DEBUG, "Checkking %s\n", string);
     regfree(&re);
     if(status != 0) {
	  return 0; 
     }
     return 1; 
} 

static int strPtrCmpFunc(char **a, char **b) {
    return(strcmp(*a, *b));
}

int getFilesInDir(char *dirName, GPtrArray **arr, char *filter)
{
    int len;
    DIR *dirp;
    struct dirent *dp;


    len = strlen(dirName);
    if (!(dirp = opendir(dirName))) {
      fprintf(stderr, "ERR %s %d\n", dirName, len);
        mlog_bn_dg("pim", "Error opening dir %s : %s\n", dirName, strerror(errno));
        return 0;
    }

    *arr = g_ptr_array_new();

    while ((dp = readdir(dirp)) != NULL) {
        if (match(dp->d_name, filter)) {
            mlog_bn_dg("pim", "Got file %s\n", dp->d_name);
            g_ptr_array_add(*arr, strdup(dp->d_name));
        }
    }

    
    mlog_bn_dy("pim", "============ Before Sort ===========\n");
    for (int i = 0; i < (*arr)->len; i++) {
        mlog_bn_dy("pim", "-----> Module file: [%s]\n", g_ptr_array_index(*arr, i));
    }
    g_ptr_array_sort(*arr, (GCompareFunc) strPtrCmpFunc);

    mlog_bn_dy("pim", "============ After Sort ===========\n");
    for (int i = 0; i < (*arr)->len; i++) {
        mlog_bn_dy("pim", "-----> Module file: [%s]\n", g_ptr_array_index(*arr, i));
    }


    (void) closedir(dirp);
    return 1;
}
