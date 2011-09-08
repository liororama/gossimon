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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <parse_helper.h>
#include <DiskInfo.h>

char proc_file_buff[8192];

int getDiskIOInfo(diskInfo_t *arr, int *size)
{
	int     fd,res;
	
	if((fd = open(DISK_PROC_FILE, O_RDONLY)) == -1)
		return 0;
	if((res = read(fd, proc_file_buff, 8192)) == -1 ) {
		close(fd);
		return 0;
	}
	close(fd);
	proc_file_buff[res] = '\0';
	return parseDiskIOInfo(proc_file_buff, arr, size);
}

diskInfo_t *findDiskInfo( diskInfo_t *diArr, int size, char *name)
{
     for(int i=0 ; i < size ; i++) {
	  if(strcmp(diArr[i].name, name) == 0) 
	       return &diArr[i];
     }
     return NULL;
}

inline int parseDiskLine(char *line, diskInfo_t *di)
{
     unsigned int tmp;
     
     if(sscanf(line, "%4d %4d %s %u %u %u %u %u %u %u %u %u %u %u",
	       &di->major, &di->minor, di->name,
	       &tmp, &tmp, &di->readSectors, &tmp,
	       &tmp, &tmp, &di->writeSectors, &tmp,
	       &tmp, &tmp, &tmp) != 14)
	  return 0;
     return 1;
}

int parseDiskIOInfo(char *buff, diskInfo_t *arr, int *size)
{
     int      devFound = 0;
     char     line[512];
     char    *buff_ptr;
     
     buff_ptr = buff;
     do {
	  diskInfo_t di;
	  
	  buff_ptr = buff_get_line(line, 512, buff_ptr);
	  
	  if(!parseDiskLine(line, &di))
	       continue;
	  diskInfo_t *di2ptr = findDiskInfo(arr, *size, di.name);
	  if(!di2ptr) continue;
	  
	  *di2ptr = di;
//	  printf("Got dev %s %u %u\n", di.name, di.readSectors, di.writeSectors);
	  devFound++;
     } while(buff_ptr);

     return devFound;
}


int calcDiskIORate(diskInfo_t *prevArr, diskInfo_t *currArr, float timePassed)
{

     //printf("Prev %u Curr %u\n" ,currArr->readSectors, prevArr->readSectors);
     float sectorsRead = currArr->readSectors - prevArr->readSectors; 
     //printf("TMP %f\n", tmp);
     if(sectorsRead > 0.0) {
	  currArr->totalRead = (sectorsRead * (float)SECTOR_SIZE) /(1024*1024);   
	  currArr->readMBSec = currArr->totalRead / timePassed ;
	  //printf("CALC %f\n", currArr->readMBSec);
     } else {
	  currArr->totalRead = 0;
     	  currArr->readMBSec = 0;
     }

     float sectorsWrite = currArr->writeSectors - prevArr->writeSectors; 
     if(sectorsWrite > 0.0) {

	  currArr->totalWrite = (sectorsWrite * (float)SECTOR_SIZE) /(1024*1024);   
	  //printf("RRRR %f\n", currArr->totalWrite);
	  currArr->writeMBSec = currArr->totalWrite / timePassed;
	  //printf("CALC %f\n", tmp * SECTOR_SIZE /(1024*1024)
	  
     } else {
	  currArr->totalWrite = 0;
	  currArr->writeMBSec = 0;
     }
      
     return 1;
}

