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
#include <sys/types.h>
#include <unistd.h>

#include <readproc.h>

unsigned long getMyMemSize() {
        char          buff[256];
        int           pid = getpid();
        proc_entry_t  pEnt;


        sprintf(buff, "/proc/%d/", pid);
        if(!read_process_stat(buff, &pEnt))
                return 0;

        return pEnt.virt_mem;
        
}


unsigned long getMyRSS() {
        char          buff[256];
        int           pid = getpid();
        proc_entry_t  pEnt;


        sprintf(buff, "/proc/%d/", pid);
        if(!read_process_stat(buff, &pEnt))
                return 0;

        return pEnt.rss_sz;
        
}
