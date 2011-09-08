/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>    
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>


// A simple copy of files
int copyFile(char *src, char *dest) {
     int src_fd, dest_fd;
     char *buff[1024*64];
     int buff_size = 1024*64;
     int size;

     if((src_fd = open(src, O_RDONLY)) == -1) {
	  fprintf(stderr, "Error opening %s for reading\n", src);
	  perror("");
	  return 0;
     }
     if((dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR |S_IRUSR | S_IRGRP  )) == -1) {
	  fprintf(stderr, "Error opening %s for writing\n", dest);
	  perror("");
	  return 0;
     }

     while(1) {
	  size = read(src_fd, buff, buff_size);
	  if(size > 0) {
	       write(dest_fd, buff, size);
	  }
	  else if(size == -1) {
	       fprintf(stderr, "Error reading source file\n");
	       return 0;
	  } else if(size ==0) {
	       close(src_fd);
	       close(dest_fd);
	       return 1;
	  }
     }
     return 1;
}

// Processing a file... scanning it until the end and adding a line at the end
int processFile(char *filename) {
     int fd;
     char buff[64*1024];
     int buff_size = 64*1024;
     int size;
     int tmp = 0;
     static iter = 0;

     if((fd = open(filename, O_RDWR)) == -1) {
	  fprintf(stderr, "Error opening file %s\n", filename);
	  return 0;
     }
     
     while((size = read(fd, buff, buff_size)) > 0) {
	  tmp += size;
     }
     if(size == -1) {
	  fprintf(stderr, "Error reading from file\n");
	  close(fd);
	  return 0;
     }
     sprintf(buff, "Iter %d Adding a line at the end of file %s\n", iter++, filename);
     write(fd, buff, strlen(buff));
     close(fd);
     return 1;
}

int main(int argc, char **argv) {
     
     int    j, unit, t;
     char **mem;
     char *ptfDirName;
     char *externalFileName;
     char *ptfFileName;
     
     if(argc < 3) {
	  fprintf(stderr, "Usage %s <ptf-dir> <external-file>\n", argv[0]);
	  exit(1);
     }
     
     ptfDirName = strdup(argv[1]);
     externalFileName = strdup(argv[2]);

     sprintf(ptfFileName, "%s/%s", ptfDirName, basename(externalFileName));

     printf("PTF Dir:         %s\n", ptfDirName);
     printf("External File:   %s\n", externalFileName);
     printf("Ptf File:        %s\n", ptfFileName);

     // Copying the file to the ptf area
     copyFile(externalFileName, ptfFileName);

     // Main loop ... running for 100 units
     for( unit = 0; unit < 20 ; unit++ )   {
	  int fd;
	  
	  // Consuming some cpu time (simulating the run of the application)
	  for( t=0, j = 0; j < 1000000 * 200; j++ )  {
	       t = j+unit*2;
	  }
	  processFile(ptfFileName);
	  printf("Unit %d done\n", unit);
	  
     }
     
     // Copying the ptf file out of the ptf dir once the main loop finish
     copyFile(ptfFileName, externalFileName);
     return 1;
}
