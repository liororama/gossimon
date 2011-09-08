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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <msx_debug.h>
#include <msx_error.h>
#include <parse_helper.h>
#include <Mapper.h>
#include <MapperInternal.h>

static
int build(mapper_t map, const char *buff, int size);

mapper_t BuildUserViewMap(const char *buff, int size, map_input_type type) {

	mapper_t map = NULL;
        mapper_t res = NULL;
	char *mapSourceBuff = NULL;
	int inputSize;
	
	mapSourceBuff = getMapSourceBuff(buff, size, type, &inputSize);
	if(!mapSourceBuff)
		goto out;;

	map = mapperInit(0);
	if(!map)
		goto out;
	
	if(!build(map, mapSourceBuff, inputSize))
		goto out;
	
	// Checking map validity
	if(!isMapValid(map))
		return NULL;
	
	// Build is compleate so we finalize it
	mapperFinalizeBuild(map);
        res = map;
 out:
        if(!res) {
                if(map)
                        mapperDone(map);
        }
        if(mapSourceBuff)
                free(mapSourceBuff);
	return res;
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
//    int current_cluster_id = 0;
//    int current_part_id = 0;
    cluster_entry_t *cluster_ptr = NULL;
    int base, count;
//    char *tmp_ptr;

//    int saw_zone_def = 0;
    char line_buff[200];
    char host_name[80];
//    char default_zone_str[10];

    buff_cur_pos = (char*) buff;
    if(!buff_cur_pos)
	    return 0;

    cluster_ptr = &map->clusterArr[0]; 
    map->nrClusters = 1;
    cluster_ptr->c_id = 0;
    cluster_ptr->c_rangeList = NULL;

    
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

	
	if (sscanf(line_buff, "%d %s %d", &base, host_name, &count) == 3) {
		debug_lb(MAP_DEBUG, "Found userview entry %d %s %d\n",
			 base, host_name, count);
		
		// If the range is valid adding it to the cluster (the single cluster we have)
		// Doing the initialization part only in the first time the map become old
		mapper_range_info_t ri;
		bzero(&ri, sizeof(ri));
		if(!mapper_addClusterHostRange(map, cluster_ptr, base, host_name, count, &ri))
		{
			return 0; 
		}
	}
	else {
		line_buff[strlen(line_buff)] = '\0';
		debug_lr(MAP_DEBUG, "Found bad line %s\n", line_buff);
		sprintf(map->errorMsg, "Bad line <%s>\n", line_buff);
		return 0;
	}
    }
    
    return 1;
}
