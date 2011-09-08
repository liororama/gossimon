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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <msx_debug.h>
#include <msx_error.h>
#include <parse_helper.h>
#include <Mapper.h>
#include <MapperInternal.h>

static
int build(mapper_t map, const char *buff, int size);

mapper_t BuildMosixMap(const char *buff, int size, map_input_type type) {

	mapper_t map = NULL;
	char *mapSourceBuff = NULL;
	int inputSize;
	
	mapSourceBuff = getMapSourceBuff(buff, size, type, &inputSize);
	if(!mapSourceBuff)
		return NULL;

	map = mapperInit(0);
	if(!map)
                goto failed;
	
	if(!build(map, mapSourceBuff, inputSize))
		goto failed;
	
	// Checking map validity
	if(!isMapValid(map))
		goto failed;
	
	// Build is compleate so we finalize it
	mapperFinalizeBuild(map);

        if(mapSourceBuff)
                free(mapSourceBuff);
	return map;

 failed:
        if(mapSourceBuff)
                free(mapSourceBuff);
        if(map)
                mapperDone(map);
        
        return NULL;
}

/** Setting the map from a buffer, treating it as  txt formatted (not xml)
 * @param buff     The buffer containing the map
 * @param size     Size of buffer
 * @return a map_parse_status_t enum to indicate the result
 */
static
int build(mapper_t map, const char *buff, int size)
{
    int linenum=0;
    char *buff_cur_pos = NULL;
    cluster_entry_t *cluster_ptr = NULL;
    int base, count;
    char line_buff[200];
    char error_str[80];
    char host_name[80];
    char flag_str[20];

    buff_cur_pos = (char*) buff;
    if(!buff_cur_pos)
	    return 0;

    cluster_ptr = &map->clusterArr[0]; 
    map->nrClusters = 1;
    cluster_ptr->c_id = 0;
    cluster_ptr->c_rangeList = NULL;

    base = 1;
    while( buff_cur_pos && !(*buff_cur_pos == '\0') )
    {
	linenum++;
	buff_cur_pos = buff_get_line(line_buff, 200, buff_cur_pos);
        
	//debug_ly(MAP_DEBUG, "Processing line: '%s'\n", line_buff);
	// Skeeing comments in map
	if(line_buff[0] == '#')
	{
		debug_lg(MAP_DEBUG, "\t\tcomment\n");
		continue;
	}
	// Skeeping empty lines
	if(strlen(line_buff) == 0)
		continue;
        
        // In this case we get addr num op
        if (sscanf(line_buff, "%s %d %s", host_name, &count, flag_str) == 3) {

                int           i;
                mapper_range_info_t ri;

                bzero(&ri, sizeof(ri));
                // Verify that the flag is only o or p
                if(strlen(flag_str) > 2) {
                        sprintf(error_str, "flag string size is > 2");
                        goto failed;
                }
                for(i = 0 ; i < strlen(flag_str) ; i++) {
                        if(flag_str[i] != 'p' && flag_str[i] != 'o') {
                                sprintf(error_str, "flag string size contain non legal characters(%s)\n",
                                        flag_str);
                                goto failed;
                        }
                        if(flag_str[i] == 'p') ri.ri_proximate = 1;
                }

                // Adding the range to the mapper
                if(mapper_addClusterHostRange(map, cluster_ptr, base, host_name, count, &ri))
		{
			base += count; 
		}
                else {
                        return 0;
                }
                
	}
        else if (sscanf(line_buff, "%s %d", host_name, &count) == 2) {
		debug_lb(MAP_DEBUG, "Found userview entry %d %s %d\n",
			 base, host_name, count);
		
		// If the range is valid adding it to the cluster (the single cluster we have)
		// Doing the initialization part only in the first time the map become old
		mapper_range_info_t ri;
		bzero(&ri, sizeof(ri));
		if(mapper_addClusterHostRange(map, cluster_ptr, base, host_name, count, &ri))
		{
			base += count; 
		}
                else {
                        return 0;
                }
	}
	else {
		line_buff[strlen(line_buff)] = '\0';
                sprintf(error_str, "Found bad line %s\n", line_buff);
		goto failed;
	}
    }
    
    return 1;

 failed:
    debug_lr(MAP_DEBUG, error_str);
    strcpy(map->errorMsg, error_str);
    return 0;
}
