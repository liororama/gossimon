/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// Setting the checkpoint file from withing the process 
// This can also be done via the -C argument to mosrun
int setCheckpointFile(char *file) {
     int fd;
     
     fd = open("/proc/self/checkpointfile", 1|O_CREAT, file);
     if (fd == -1) {
	  return 0;
     }
     return 1;

}

// Triggering a checkpoint from whithin the process
int triggerCheckpoint() {
     int fd;
     fd = open("/proc/self/checkpoint", 1|O_CREAT, 1);
     if(fd == -1) {
	  fprintf(stderr, "Error doing self checkpoint \n");
	  return 0;
     }
     printf("Checkpoint was done successfuly\n");
     return 1;
}

int main(int argc, char **argv) {
     int    j, unit, t;
     char *checkpointFileName;
     int   checkpointUnit = 0;
     
     if(argc < 3) {
	  fprintf(stderr, "Usage %s <checkpoint-file> <unit>\n", argv[0]);
	  exit(1);
     }
     
     checkpointFileName = strdup(argv[1]);
     checkpointUnit = atoi(argv[2]);
     if(checkpointUnit < 1 || checkpointUnit > 100) {
	  fprintf(stderr, "Checkpoint unit should be > 0 and < 100\n");
	  exit(1);
     }

     printf("Checkpoint file: %s\n", checkpointFileName);
     printf("Checkpoint unit: %d\n", checkpointUnit);
     
     // Setting the checkpoint file from within the process (also can be done using
     // the -C argument of mosrun
     if(!setCheckpointFile(checkpointFileName)) {
	  fprintf(stderr, "Error setting the checkpoint filename from within the process\n");
	  fprintf(stderr, "Make sure you are running this program via mosrun\n");
	  return 0;
     }
     
     // Main loop ... running for 100 units. checnge this loop if you wish
     // the program to run do more loops
     for( unit = 0; unit < 100 ; unit++ )   {
	  // Consuming some cpu time (simulating the run of the application)
	  // Change the number below to cause each loop to consume more (or) less
	  // time                ||      ||  
	  //                     \/      \/
	  for( t=0, j = 0; j < 1000000 * 500; j++ )  {
	       t = j+unit*2;
	  }
	  printf("Unit %d done\n", unit);

	  // Trigerring a checkpoint request from within the process 
	  if(unit == checkpointUnit) {
	       if(!triggerCheckpoint())
		    return 0;
	  }
     }
     return 1;
}
