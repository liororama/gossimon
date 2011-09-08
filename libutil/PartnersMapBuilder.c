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
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

#include <msx_debug.h>
#include <msx_error.h>
#include <parse_helper.h>
#include <Mapper.h>
#include <MapperInternal.h>
#include <MapperBuilder.h>

static int getAllDirFiles(const char *dirName, char ***fileList, int *fileNum);
static int buildFromText(mapper_t map, char **fileList, int fileNum);
static int addTextPartner(mapper_t map, char *fileName);
static int buildFromBin(mapper_t map, char **fileList, int fileNum);
static int addBinPartner(mapper_t map, char *fileName);

mapper_t BuildPartnersMap(const char *partnerDir, partner_map_t type) {
	
	mapper_t   map = NULL;
	int        fileNum = 0, res;
	char     **fileList = NULL;
	
	
	map = mapperInit(0);
	if(!map)
		return map;

	// Get all files from the directory
	if(!getAllDirFiles(partnerDir, &fileList, &fileNum))
		goto failed;
        
	if(fileNum == 0 )
                goto failed;
        
	switch(type) {
	    case PARTNER_SRC_TEXT:
		    res = buildFromText(map, fileList, fileNum);
		    break;
	    case PARTNER_SRC_BIN:
		    res = buildFromBin(map, fileList, fileNum);
		    break;
	    default:
		    debug_lr(MAP_DEBUG, "Error type %d no supported\n", type);
		    goto failed;
	}
	if(!res)
		goto failed;
	
	if(!isMapValid(map))
		goto failed;
	
	// Build is compleate so we finalize it
	mapperFinalizeBuild(map);
        for(int i=0 ; i<fileNum ; i++)
                if(fileList[i])
                        free(fileList[i]);
        free(fileList);
	return map;
	
 failed:
	mapperDone(map);
	if(fileList) {
                for(int i=0 ; i<fileNum ; i++)
                        if(fileList[i])
                                free(fileList[i]);
		free(fileList);
        }
	return NULL;
}

int getAllDirFiles(const char *dirName, char ***fileList, int *fileNum) {
	DIR            *dir;
	struct dirent  *dirEnt;
	int             i;
	char          **list;
	char            tmpBuff[256];
	struct stat     statBuff;

	if(!dirName)
		return 0;
	dir = opendir(dirName);
	if(!dir) {
		debug_lr(MAP_DEBUG, "Error while opening directory %s\n", dirName);
		return 0;
	}

	list = (char **)malloc(sizeof(char *) * MAPPER_MAX_CLUSTERS);
        bzero(list, sizeof(char *) * MAPPER_MAX_CLUSTERS);
        
	i=0;
	while((dirEnt = readdir(dir))) {
		if( (strcmp(dirEnt->d_name, ".") == 0) ||
		    (strcmp(dirEnt->d_name, "..") == 0))
			continue;
		// Checking if the file is a directory
		sprintf(tmpBuff, "%s/%s", dirName, dirEnt->d_name); 
		if(stat(tmpBuff, &statBuff) == -1) {
			debug_lr(MAP_DEBUG, "Error stating file %s\n", tmpBuff);
			continue;
		}
		if(!S_ISREG(statBuff.st_mode))
			continue;
		
		list[i++] = strdup(tmpBuff);
		if(i == MAPPER_MAX_CLUSTERS) {
			debug_lr(MAP_DEBUG, "Reached max cluster limit in building partner map\n");
			break;
		}
	}

	closedir(dir);
	*fileList = list;
	*fileNum = i;
        debug_lr(MAP_DEBUG, "Got %d files from dir\n", i);
	return 1;
}

char *getClusterName(char *fileName) {
	char *clusterName;
	
	clusterName = strrchr(fileName, '/');
	if(clusterName) {
		clusterName++;
	} else {
		clusterName = fileName;
	}
	return clusterName;
}

int buildFromText(mapper_t map, char **fileList, int fileNum) {
	int i;

	for(i=0 ; i<fileNum ; i++) {
		debug_lg(MAP_DEBUG, "Processing file: %s\n", fileList[i]);
		addTextPartner(map, fileList[i]);
		
	}
	return 1;
}

int addTextPartner(mapper_t map, char *fileName) {

	int              res = 0;
	int              size;
	int              linenum=0;
	char            *buffCurrPos = NULL;
	char            *buff = NULL;

	char            *clusterName = NULL;
	cluster_entry_t *cEnt = NULL;
	mapper_cluster_info_t ci;
	mapper_range_info_t   ri;
	int              lineBuffSize = 200;
	char             lineBuff[200];

	char             host_name[255];
	    
	cluster_info_init(&ci);
	
	buff = getMapSourceBuff(fileName, 0, INPUT_FILE, &size);
	if(!buff) {
		debug_lr(MAP_DEBUG, "Error reading file\n");
		goto out;
	}
	buffCurrPos = (char*) buff;
	if(!buffCurrPos)
		goto out;

	// Reading the cluster commet/description
	if(lineBuffSize > size)
		lineBuffSize = size;
	buffCurrPos = buff_get_line(lineBuff, lineBuffSize, buffCurrPos);
	if(!buffCurrPos) {
		debug_lr(MAP_DEBUG, "First line is not ok\n");
		goto out;
	}
	
	linenum++;
	debug_lg(MAP_DEBUG, "Processing line: %s\n", lineBuff);

	ci.ci_desc = strdup(lineBuff);
	
	
	// Reading the cluster prio, dont-take dontgo...
	buffCurrPos = buff_get_line(lineBuff, lineBuffSize, buffCurrPos);
	if(!buffCurrPos) {
		debug_lr(MAP_DEBUG, "Second line is ok\n");
		goto out;
	}
	
	linenum++;
	debug_lg(MAP_DEBUG, "Processing line: %s\n", lineBuff);

	int cango, cantake, canexpend;
	if(sscanf(lineBuff, "%d %d %d %d",
		  &ci.ci_prio, &cango, &cantake, &canexpend) != 4)
		
	{
		debug_lr(MAP_DEBUG, "Second line is not in correct format\n");
		goto out;
	}
	ci.ci_cango     = (char)cango;
	ci.ci_cantake   = (char)cantake;
	ci.ci_canexpend = (char)canexpend;
	
	clusterName = getClusterName(fileName);
	
	if(!mapper_addCluster(map, -1, clusterName, &ci)) {
		debug_lr(MAP_DEBUG, "Failed to add cluster %s\n", clusterName);
		goto out;
	}
	free(ci.ci_desc);
	ci.ci_desc = NULL;
	
	// Checking that the cluster is not already there
	cEnt = get_cluster_entry_by_name(map, clusterName);
	if(!cEnt) {
		debug_lr(MAP_DEBUG, "Just added cluster is not there\n");
		goto out;
	}

	// Reading the ranges
	int base = 1;
	while( buffCurrPos && !(*buffCurrPos == '\0') )
	{
		linenum++;
		buffCurrPos = buff_get_line(lineBuff, lineBuffSize, buffCurrPos);
		debug_lg(MAP_DEBUG, "Processing line: %s\n", lineBuff);
		

		// Skeeing comments in map
		if(lineBuff[0] == '#')
		{
			debug_lg(MAP_DEBUG, "\t\tcomment\n");
			continue;
		}
		// Skeeping empty lines
		if(strlen(lineBuff) == 0)
			continue;
		
		int count, core, participate, proximate;
		if (sscanf(lineBuff, "%s %d %d %d %d",
			   host_name, &count, &core, &participate, &proximate) == 5) {
			debug_lb(MAP_DEBUG, "Found partner range entry %s %d\n",
				 host_name, count);
                        
			ri.ri_core        = (char) core;
			ri.ri_participate = (char) participate;
			ri.ri_proximate   = (char) proximate;
			
			// If the range is valid adding it to the cluster (the
			// single cluster we have)
			// Doing the initialization part only in the first time
			// the map become old
			
			if(mapper_addClusterHostRange(map, cEnt, base, host_name, count, &ri))
			{
				base += count;
			}
			else {
				goto out;
			}
		}
		else {
			lineBuff[strlen(lineBuff)] = '\0';
			debug_lr(MAP_DEBUG, "Found bad line %s\n", lineBuff);
			sprintf(map->errorMsg, "Bad line <%s>\n", lineBuff);
			goto out;
		}
	}
	
	res = 1;
	
 out:
        if(buff)
		free(buff);
	cluster_info_free(&ci);
	return res;
}




int buildFromBin(mapper_t map, char **fileList, int fileNum)
{
	int i;
	
	for(i=0 ; i<fileNum ; i++) {
		debug_lg(MAP_DEBUG, "Processing file: %s\n", fileList[i]);
		addBinPartner(map, fileList[i]);
		
	}
	return 1;
}

struct bin_partner {
        unsigned int    ip;        /* first IP in range */
        int             n;         /* number of IP's in range, or 0=alias */
        char            dontgo;
        char            donttake;
        char            proximate; /* no need for compression */
        char            outsider;  /* processes without -G{n} cannot go there */
        unsigned short  priority;  /* lower=better */
        unsigned short  its_me;    /* 0 in our case! */
        unsigned int    alias_to;  /* IP address.  0 for a regular range */
};

int addBinPartner(mapper_t map, char *fileName) {
	int                 res = 0;
	int                 size, i;
	char               *buff;
	struct bin_partner *partnerArr;
	int                 rangeNum;
	char               *clusterName = NULL;
	
	cluster_entry_t *cEnt;
	mapper_cluster_info_t ci;
	mapper_range_info_t   ri;
	
	    
	
	buff = getMapSourceBuff(fileName, 0, INPUT_FILE_BIN, &size);
	if(!buff) {
		debug_lr(MAP_DEBUG, "Error reading file\n");
		goto out;
	}
	if(size == 0 || (size % sizeof(struct bin_partner)) != 0) {
		debug_lr(MAP_DEBUG, "Error number of records is not integer (%d %d)\n",
			 size, sizeof(struct bin_partner));
		goto out;
	}

	clusterName = getClusterName(fileName);
	partnerArr = (struct bin_partner *) buff;
	rangeNum = size / sizeof(struct bin_partner);

	// Adding the cluster using the first range
	ci.ci_prio      =  partnerArr[0].priority;
	ci.ci_cango     =  !partnerArr[0].dontgo;
	ci.ci_cantake   =  !partnerArr[0].donttake;
	ci.ci_canexpend = 0;
	ci.ci_desc      = NULL;
	
	clusterName = getClusterName(fileName);
	
	if(!mapper_addCluster(map, -1, clusterName, &ci)) {
		debug_lr(MAP_DEBUG, "Failed to add cluster %s\n", clusterName);
		goto out;
	}
	// Checking that the cluster is not already there
	cEnt = get_cluster_entry_by_name(map, clusterName);
	if(!cEnt) {
		debug_lr(MAP_DEBUG, "Just added cluster is not there\n");
		goto out;
	}

	// Adding all the ranges
	int base = 1;
	for(i = 0 ; i < rangeNum ; i++) {
		ri.ri_core        =  0;
		ri.ri_participate =  1;
		ri.ri_proximate   =  partnerArr[i].proximate;

		struct in_addr addr;
		addr.s_addr = htonl(partnerArr[i].ip);

		//printf("Got range %s %d\n", inet_ntoa(addr), partnerArr[i].n);
		// If the range is valid adding it to the cluster (the single cluster we have)
		if(mapper_addClusterAddrRange(map, cEnt, base, &addr, partnerArr[i].n, &ri))
		{
			base += partnerArr[i].n;
		}
		else {
			goto out;
		}
	}
	
	res = 1;
 out:
	if(buff)
		free(buff);
	return res;
}




/// Writer Part /////


int generateTextPartner(mapper_t map, const char *clusterName,
                        char *buff, int *size)
{
	int                    res = 0;
	mapper_cluster_info_t  ci;
	mapper_range_iter_t    rIter;
	char                  *currPtr = buff;
	int                    currSize = 0, sz;

	cluster_info_init(&ci);
	if(!mapper_getClusterInfo(map, clusterName, &ci)) {
		debug_lr(MAP_DEBUG, "Very strange cluster %s has no info\n",
			 clusterName);
		goto out;
	}
	sz = sprintf(currPtr, "%s\n", ci.ci_desc);
	currSize += sz; currPtr  += sz;

	sz = sprintf(currPtr, "%d  %hhd %hhd %hhd\n",
		     ci.ci_prio, ci.ci_cango, ci.ci_cantake, ci.ci_canexpend);
	currSize += sz; currPtr  += sz;
	// Print the cluster ranges
	rIter = mapperRangeIter(map, clusterName);
	if(!rIter)
		goto out;
	
	
	mapper_range_t       mr;
	mapper_range_info_t  ri;
	while(mapperRangeIterNext(rIter, &mr, &ri)) {
		struct hostent  *hostent;
		char            *hostname;
		
		if ((hostent = gethostbyaddr(&mr.baseIp, sizeof(struct in_addr), AF_INET)))
			hostname = hostent->h_name;
		else
			hostname = inet_ntoa(mr.baseIp);
		
		sz = sprintf(currPtr,"%s %d %hhd %hhd %hhd\n",
			     hostname, mr.count,
			     ri.ri_core, ri.ri_participate, ri.ri_proximate);
		currSize += sz; currPtr  += sz;
		if(currSize > *size) {
			debug_lr(MAP_DEBUG, "Not enough memory in buffer to hold a single partner file???\n");
			goto out;
		}
		
	}

        *size = currSize;
	res = 1;
 out:
	
	mapperRangeIterDone(rIter);
	cluster_info_free(&ci);
	return res;
}

int generateBinPartner(mapper_t map, const char *clusterName,
			      char *buff, int *size)
{
	int                    res = 0;
	mapper_cluster_info_t  ci;
	mapper_range_iter_t    rIter;
	int                    currSize = 0;
	struct bin_partner    *binPartner;
	int                    rangeNum;

        
        binPartner = (struct bin_partner *) buff;
        
        
	cluster_info_init(&ci);
	if(!mapper_getClusterInfo(map, clusterName, &ci)) {
		debug_lr(MAP_DEBUG, "Very strange cluster %s has no info\n",
			 clusterName);
		goto out;
	}

	// Print the cluster ranges
	rIter = mapperRangeIter(map, clusterName);
	if(!rIter)
		goto out;
	
	rangeNum = 0;
	mapper_range_t       mr;
	mapper_range_info_t  ri;
	while(mapperRangeIterNext(rIter, &mr, &ri)) {

                bzero(&binPartner[rangeNum], sizeof(struct bin_partner));
                binPartner[rangeNum].ip        =  ntohl(mr.baseIp.s_addr);
		binPartner[rangeNum].n         =  mr.count;
                binPartner[rangeNum].proximate =  ri.ri_proximate;
                binPartner[rangeNum].dontgo    = !ci.ci_cango;
                binPartner[rangeNum].donttake  = !ci.ci_cantake;
                binPartner[rangeNum].outsider  = 1;
                binPartner[rangeNum].priority  = ci.ci_prio;

                currSize += sizeof(struct bin_partner);
		if(currSize > *size) {
			debug_lr(MAP_DEBUG, "Buffer too small to hold a single partner file!!\n");
			goto out;
		}
	}

        *size = currSize;
	res = 1;
 out:
	
	mapperRangeIterDone(rIter);
	cluster_info_free(&ci);
	return res;
}

int _writePartnerMap(mapper_t map, const char *dirName, partner_map_t type) {
	
	int                    res = 0;	
	mapper_cluster_iter_t  cIter = NULL;
	char                  *clusterName;
	char                  *tmpBuff = NULL;
	int                    tmpBuffSize = 4096;
	int                    size = 0;
	int                    fd = -1;
	char                   fileName[MAPPER_MAX_CLUSTER_NAME];
	
	tmpBuff = malloc(tmpBuffSize);
	if(!tmpBuff) 
		goto out;
	
	cIter = mapperClusterIter(map);
	if(!cIter)
		goto out;

	while(mapperClusterIterNext(cIter, &clusterName)) {
		// Print the cluster details
		size = tmpBuffSize;
                switch (type) {
                    case PARTNER_SRC_TEXT:
                            generateTextPartner(map, clusterName, tmpBuff, &size);
                            break;
                    case PARTNER_SRC_BIN:
                            generateBinPartner(map, clusterName, tmpBuff, &size);
                            break;
                    default:
                            goto out;
                }
		sprintf(fileName, "%s/%s", dirName, clusterName);
		
		fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if(fd == -1) {
			debug_lr(MAP_DEBUG, "Error opening partner file\n");
			goto out;
		}
		if(write(fd, tmpBuff, size) != size) {
			debug_lr(MAP_DEBUG, "Error writing partner to file\n");
			goto out;
		}
		close(fd);
		fd = -1;
	}
	mapperClusterIterDone(cIter);
        cIter = NULL;
	res = 1;

 out:
	if(tmpBuff)
		free(tmpBuff);
	if(fd >= 0 )
		close(fd);
        if(cIter)
                mapperClusterIterDone(cIter);
        
	return res;
}


int WritePartnerMap(mapper_t map, const char *partnerDir)
{
        return _writePartnerMap(map, partnerDir, PARTNER_SRC_TEXT);
}
int WriteBinPartnerMap(mapper_t map, const char *partnerDir) {
        return _writePartnerMap(map, partnerDir, PARTNER_SRC_BIN);
}
