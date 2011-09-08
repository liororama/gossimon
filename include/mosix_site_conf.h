/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __MOSIX2_SITE_CONF
#define __MOSIX2_SITE_CONF

// This variable state if we encountered a bad ip in one of our checks
// If so we can do whatever we want

extern int badip;

// Checking if a given ip is valid. If ues badip is cleared if not
// badip is set to indicate bad ip state

int is_ip_valid(unsigned int ip);



// Temporary trap disable for the amos cluster so we wont need to change
// kernel. So setpe will work on older version of mosix until we upgrade
// the amos cluster to the new kernel

extern int setpe_use_only_new_version;

#endif
