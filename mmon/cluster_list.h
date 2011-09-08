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
 * Copyright (c) 2000, 2001, 2002, 2003, 2004 Amnon BARAK (amnon@MOSIX.org).
 * All rights reserved.
 * This software is subject to the MOSIX SOFTWARE LICENSE AGREEMENT.
 *
 *	MOSIX $Id: cluster_list.h,v 1.2 2006-10-29 09:30:22 alexam02 Exp $
 *
 * THIS SOFTWARE IS PROVIDED IN ITS "AS IS" CONDITION, WITH NO WARRANTY
 * WHATSOEVER. NO LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE USE OF THIS SOFTWARE WILL BE ACCEPTED.
 */
/*
 * Author(s): Amar Lior
 */
#ifndef __MOSIX_MMON_CLUSTER_LIST
#define __MOSIX_MMON_CLUSTER_LIST

#define MAX_CLUSTER_NAME (64)

#include "host_list.h"

typedef struct cluster_entry
{
	char name[MAX_CLUSTER_NAME+1];
	mon_hosts_t host_list;
} cluster_entry_t;

typedef struct mon_cluster_list
{
	int cl_ncluster;
	int cl_curr;
	cluster_entry_t *cl_list;
} clusters_list_t;

int cl_init(clusters_list_t *cl);
int cl_free(clusters_list_t *cl);
void cl_print(clusters_list_t *cl);

// Setting the cluster list from a string. The string can be a list
// of hosts which can be a dns name each (or only one host)
// The cluster name is the name of the node (if it is a dns name than the
// dns name itself (e.g,. bmos)
int cl_set_from_str(clusters_list_t *cl, char *str);

// Setting the cluster list from a file. The file can contain ranges of machine
// per cluster and also the cluster name which can not be given in the string
// format. 
int cl_set_from_file(clusters_list_t *cl, char *fn);


// Adding a single cluster based on its range string
int cl_set_single_cluster(clusters_list_t *cl, char *cluster_str);

// Return the next cluster in the list. if we reach the end of the list we
// circulate to the begining
cluster_entry_t *cl_next(clusters_list_t *cl);

// Retrun the previous cluster in the list
cluster_entry_t *cl_prev(clusters_list_t *cl);

// Retrun the current cluster in the list
cluster_entry_t *cl_curr(clusters_list_t *cl);

// Return size of cluster list
int cl_size(clusters_list_t *cl);
	

#endif /* __MOSIX_MMON_CLUSTER_LIST */
