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

#include <sys/time.h>

float timeDiffFloat(struct timeval *start, struct timeval *end) {
     float secDiff = end->tv_sec - start->tv_sec;
     float usecDiff = end->tv_usec - start->tv_usec;
     if(usecDiff < 0) {
	  secDiff -= 1;
	  usecDiff += 1000000;
     }
     return secDiff + usecDiff/1000000;
}
