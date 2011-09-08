/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/



#ifndef __MSX_PROC__
#define __MSX_PROC__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/utsname.h>


int msx_readval(char *path, int *val);
int msx_readval2(char *path, int *val1, int *val2);
int msx_write(char *path, int val);
int msx_write2(char *path, int val1, int val2);
int msx_readnode(int node, char *item);
long long msx_readnodemem(int node, char *item);
int msx_readproc(int pid, char *item);
int msx_read(char *path);
int msx_writeproc(int pid, char *item, int val);
int msx_readdata(char *fn, void *into, int max, int size);
int msx_writedata(char *fn, char *from, int size);
int msx_replace(char *fn, int val);
int msx_count_ints(char *fn);
int msx_fill_ints(char *fn, int *, int);

int msx_readstr(char *fn, char *buff, int *size);

#endif
