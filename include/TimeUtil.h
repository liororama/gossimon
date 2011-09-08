/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __CL_TIME_UTIL
#define __CL_TIME_UTIL

#include <sys/time.h>
#include <time.h>
// Returns the time difference in second betwenn start and end.
// The returned value is a floating number
float timeDiffFloat(struct timeval *start, struct timeval *end);



#endif
