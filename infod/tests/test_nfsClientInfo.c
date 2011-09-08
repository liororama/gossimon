/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

#include <provider.h>


int main() {
	nfs_client_info_t nci, nci_old;
	nfs_client_info_t nci_rates;
	
	struct timeval t_now, t_old;
	

	while(1) {
		bzero(&nci, sizeof(nfs_client_info_t));
		bzero(&nci_rates, sizeof(nfs_client_info_t));
		get_nfs_client_info(&nci);
		gettimeofday(&t_now, NULL);
		calc_nfs_client_rates(&nci_old, &nci,
				      &t_old, &t_now,
				      &nci_rates);
		

		printf("\n");
		printf("rpc-ops      %ld\n", nci_rates.rpc_ops);
		printf("read-ops     %ld\n", nci_rates.read_ops);
		printf("write-ops    %ld\n", nci_rates.write_ops);
		printf("getattr-ops  %ld\n", nci_rates.getattr_ops);
		printf("other-ops    %ld\n", nci_rates.other_ops);

		nci_old = nci;
		t_old = t_now;
		
		sleep(1);
	}
	
	return 0;
	
}
