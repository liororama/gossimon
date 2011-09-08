/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __MOSIX2_SITE_INFO
#define __MOSIX2_SITE_INFO

#include <site_protection.h>

// This File should contain only information definitions

struct valid_ip valid_ips[] =
{
	{HIDE_IP(IP(132,65,0,0)), CLASS_B_SIZE},
	{HIDE_IP(IP(132,64,161,40)), 10},
};

// The size of the above valid_ips array
#define VALID_IPS_ENTRIES    (2)

#endif
