/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __MOSIX2_SITE_PROTECTION
#define __MOSIX2_SITE_PROTECTION


#define CLASS_B_SIZE     (65536)
#define CLASS_C_SIZE     (256)


#define IP(i1,i2,i3,i4)        \
        ((((int)(i1)) << 24) | \
         (((int)(i2)) << 16) | \
         (((int)(i3)) << 8)  | \
         ((int)(i4)))

/*
 * Hiding function:
 * We break the ip to 4 part and every part to upper (U) and lower (L)
 * Then we permutate the parts and receive the hide function
 * i1L, i3U, i2L, i3L, i1U, i4U, i2U, i4L
 *
 */

#define HIDE_IP(ip) \
	 (((((ip) & 0xf0000000) >> 28) << 12 ) |       \
          ((((ip) & 0x0f000000) >> 24) << 28 ) |       \
	  ((((ip) & 0x00f00000) >> 20) << 4  ) |       \
	  ((((ip) & 0x000f0000) >> 16) << 20 ) |       \
	  ((((ip) & 0x0000f000) >> 12) << 24 ) |       \
	  ((((ip) & 0x00000f00) >> 8 ) << 16 ) |       \
	  ((((ip) & 0x000000f0) >> 4 ) << 8  ) |       \
	  ((((ip) & 0x0000000f)      )))             


/*
 * Unhiding an ip according to the above hiding function
 *
 */
#define UNHIDE_IP(hip) \
	IP( ((hip & 0xf0000000) >> 28)  | ((hip & 0x0000f000) >> 8 ), \
	    ((hip & 0x00f00000) >> 20)  | ((hip & 0x000000f0)      ), \
            ((hip & 0x000f0000) >> 16)  | ((hip & 0x0f000000) >> 20), \
    	    ((hip & 0x0000000f)      )  | ((hip & 0x00000f00) >> 4 ))
	


struct valid_ip {
	unsigned int ip;
	int          cnt;
};

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
