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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <msx_error.h>
#include <msx_debug.h>

#include <parse_helper.h>
#include <ConfFileReader.h>

conf_file_t cf_new(char **validVarName) {

        conf_file_t cf = NULL;
        
        cf = malloc(sizeof(struct conf_file_struct));
        if(!cf) goto fail;;

        cf->validVarHash = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
        cf->varHash = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
        if(!cf->varHash)
                goto fail;


        int i=0;
        while(validVarName[i]) {
                char *tmpVarName = strdup(validVarName[i]);
                char *tmpVal = strdup(validVarName[i]);
                g_hash_table_insert(cf->validVarHash, tmpVarName, tmpVal);
                i++;
        }
        cf->errMsg[0] = '\0';
        return cf;
 fail:
        cf_free(cf);
        return NULL;
}

conf_file_t cf_load(char *fileName, char **validVarNames)
{

    conf_file_t cf;

    if (!(cf = cf_new(validVarNames))) {
        fprintf(stderr, "Error generating conf file object\n");
        return NULL;
    }
    debug_lb(MAP_DEBUG, "Parsing configuration file [%s]\n", fileName);
    if (access(fileName, R_OK) != 0) {
        debug_lr(MAP_DEBUG, "Error accessing configuration files [%s]\n",
                fileName);
        return NULL;
    }

    if (!cf_parseFile(cf, fileName)) {
        debug_lr(MAP_DEBUG, "Error parsing configuration file\n");
        return NULL;
    }
    return cf;
}


char *cf_error(conf_file_t cf) {
        return cf->errMsg;
}
char *readFile(const char* filename, int *res_size)
{
	int fd = -1;
	struct stat s;
	char *buff = NULL;
	
	if(stat(filename, &s) == -1)
		goto failed;
        
	if(!(buff = malloc(s.st_size+1)))
		goto failed;
	
	if((fd = open(filename, O_RDONLY)) == -1)
                goto failed;
        
	if(read(fd, buff, s.st_size) != s.st_size)
		goto failed;
	
	buff[s.st_size]='\0';
        
	close(fd);
	*res_size = s.st_size+1;
	return buff;
        
 failed:
	if(buff) free(buff);
	if(fd != -1) close(fd);
	return NULL;
}

// Reding the conf file from a real file
int cf_parseFile(conf_file_t cf, char *fileName) {
	int   fileSize;
        char *fileBuff = NULL;
        
        fileBuff = readFile(fileName, &fileSize);
        if(!fileBuff)
                return 0;
        int res = cf_parse(cf, fileBuff);
	if(!res) {
		free(fileBuff);
		return 0;
	}
	return res;
}

int cf_parse(conf_file_t cf, char *confStr) {
     
     int   linenum=0;
     char *buff_cur_pos = NULL;
     char  line_buff[200];
     char *varName;
     char *varVal;
     
     if(!confStr)
	  return 0;
     
     cf->varHash = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
     if(!cf->varHash)
	  goto fail;
     
     buff_cur_pos = confStr;
     
     while( buff_cur_pos && !(*buff_cur_pos == '\0') )
     {
	  linenum++;
	  buff_cur_pos = buff_get_line(line_buff, 200, buff_cur_pos);
	  
	  debug_ly(MAP_DEBUG, "Processing line: '%s'\n", line_buff);
	  // Skeeing comments in map
	  if(line_buff[0] == '#')
	  {
	       debug_lg(MAP_DEBUG, "\t\tcomment\n");
	       continue;
	  }
	  // Skeeping empty lines
	  if(strlen(line_buff) == 0)
	       continue;
	  
	  char *equalPtr;
	  equalPtr = strchr(line_buff, '=');
	  if(equalPtr) {
	       varName = line_buff;
	       *equalPtr = '\0';
	       varVal = ++equalPtr;
	       
	       varName = eat_spaces(varName);
	       trim_spaces(varName);
	       
	       varVal = eat_spaces(varVal);
	       trim_spaces(varVal);
	       //if (sscanf(line_buff, "%s=%s", varName, varVal) == 2) {
	       debug_lb(MAP_DEBUG, "Found var #%s# #=# #%s#\n",
			varName, varVal);
	       char *tmpVarName = strdup(varName);
	       
	       if(g_hash_table_lookup(cf->validVarHash, tmpVarName)) {
		    conf_var_t  *varEntPtr;
		    varEntPtr = malloc(sizeof(conf_var_t));
		    varEntPtr->varValue = strdup(varVal);
		    varEntPtr->varName = strdup(varName);
		    g_hash_table_insert(cf->varHash, tmpVarName, varEntPtr);
	       }
	       else {
		    debug_lr(MAP_DEBUG, "Variable %s is not valid\n", tmpVarName);
		    sprintf(cf->errMsg, "Variable %s is not valid\n", tmpVarName);
	       }
	       
	  }
	  else {
	       line_buff[strlen(line_buff)] = '\0';
	       debug_lr(MAP_DEBUG, "Found bad line %s\n", line_buff);
	       sprintf(cf->errMsg, "Bad line <%s>\n", line_buff);
	       goto fail;
	  }
     }
     
     return 1;
     
 fail:
     return 0;
}
void cf_free(conf_file_t cf) {
        if(!cf)
                return;

        if(cf->varHash)
                g_hash_table_destroy(cf->varHash);
        if(cf->validVarHash)
                g_hash_table_destroy(cf->validVarHash);
        free(cf);
}

int  cf_getVar(conf_file_t cf, char *varName, conf_var_t *varEnt)
{
        conf_var_t  *varPtr;
        
        if(!cf || !varName)
                return 0;
        
        varPtr = g_hash_table_lookup(cf->varHash, varName);
        if(!varPtr)
                return 0;
        *varEnt = *varPtr;
        return 1;
}


static int getInt(char *str, int *val) {
     errno = 0;
     *val = strtol(str, (char **) NULL, 10);
     if(errno) {
          return 0;
     }
     return 1;
}

int cf_getIntVar(conf_file_t cf, char *varName, int *varInt)
{
     conf_var_t ent;
     int        intVal;
     if(!cf_getVar(cf, varName, &ent))
	  return 0;
     
     if(!getInt(ent.varValue, &intVal)) {
	  return 0;
     }

     *varInt = intVal;
     return 1;
}

// Getting a list of values seperated by seperator
int  cf_getListVar(conf_file_t cf, char *varName, char *seperator, GPtrArray *gptr) {

    conf_var_t varEnt;

    if (!cf_getVar(cf, varName, &varEnt)) {
        return 0;
    }

    int tmpSize = 200;
    char *tmpNames[200];

    tmpSize = split_str(varEnt.varValue, tmpNames, tmpSize,  seperator);
    if (tmpSize > 0) {
        for (int i = 0, j = 0; i < tmpSize; i++) {
            g_ptr_array_add(gptr, tmpNames[i]);
        }
    }
    return 1;
}




