/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/



/*
 * Author(s): Amar Lior
 */

// Implementing a host list for mmon -h argument.
//
// If there is a single name (no .. and node , or ^ ) then
// the name is treated as a dns name we try to get the list of addresses
// for that name and build a host list from those addresses.
//
// If we have a range we parse it and build a list from it.
//


#ifndef __MOSIX_MMON_HOST_LIST
#define __MOSIX_MMON_HOST_LIST

typedef struct mon_host_list {
	int   mh_nhost;
	int   mh_curr;
	char **mh_list;
} mon_hosts_t;

int mh_init(mon_hosts_t *mh);
int mh_free(mon_hosts_t *mh);
void mh_print(mon_hosts_t *mh);
// Set the host names from a string. The names in the current list are lost
int mh_set(mon_hosts_t *mh, char *hosts_str);
// Add the hosts in the string to the current list
int mh_add(mon_hosts_t *mh, char *hosts_str);

// Getting the next host in the list
char *mh_current(mon_hosts_t *mh);
char *mh_next(mon_hosts_t *mh);	
int mh_size(mon_hosts_t *mh);

#endif /* __MOSIX_MMON_HOST_LIST */
