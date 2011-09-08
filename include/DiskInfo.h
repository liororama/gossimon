/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __DISK_INFO
#define __DISK_INFO

#define MAX_DISK_DEVICE_NAME   (128)
#define SECTOR_SIZE            (512)
#define DISK_PROC_FILE         "/proc/diskstats"



typedef struct {
	char           name[MAX_DISK_DEVICE_NAME];
	int            major;
	int            minor;
	unsigned int   readSectors;
	unsigned int   readOps;
	unsigned int   writeSectors;
	unsigned int   writeOps;

	float          readMBSec;
	float          writeMBSec;

	float          totalRead;
	float          totalWrite;
	
} diskInfo_t;

int getDiskIOInfo(diskInfo_t *diskInfo, int *size);
int parseDiskIOInfo(char *buff, diskInfo_t *diskInfo, int *size);

int calcDiskIORate(diskInfo_t *prevArr, diskInfo_t *currArr, float timePassed);
#endif
