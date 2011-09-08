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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "host_list.h"

char strBuff[1024];

int mh_init(mon_hosts_t *mh)
{
	if(!mh)
		return 0;

	mh->mh_nhost = 0;
	mh->mh_curr = 0;
	mh->mh_list = NULL;

	return 1;
}

int mh_free(mon_hosts_t *mh)
{
	int i;
	
	if(!mh)
		return 1;
	
	for(i=0 ; i< mh->mh_nhost ; i++)
		if(mh->mh_list[i])
			free(mh->mh_list[i]);
	
	mh->mh_nhost = 0;
	mh->mh_curr = 0;
	mh->mh_list = NULL;
	
	return 1;
}


void mh_print(mon_hosts_t *mh)
{
	int i;

	if(!mh || mh->mh_nhost <= 0)
	{
		printf("Empty list\n");
		return;
	}

	for(i=0 ; i< mh->mh_nhost ; i++)
		printf("Host %d: %s\n", i, mh->mh_list[i]);
	
}
int mh_size(mon_hosts_t *mh)
{
	return mh->mh_nhost;
}

char *mh_current(mon_hosts_t *mh)
{
	if(!mh)
		return NULL;
	return mh->mh_list[mh->mh_curr];
}
char *mh_next(mon_hosts_t *mh)
{
	char *host;
	
	if(!mh || mh->mh_nhost <= 0)
		return NULL;

	mh->mh_curr = (mh->mh_curr + 1) % mh->mh_nhost;
	host = mh->mh_list[mh->mh_curr];
	return host;
}

int is_range(char *str)
{
	if(strstr(str, "..") ||
	   strchr(str, ','))
		return 1;
	return 0; 
}

int is_number(char *str)
{
	int l = strlen(str);
	int i;
	
	for(i=0; i<l; i++)
		if(!isdigit(str[i]))
			return 0;
	return 1;
}

int parse_range(char *r, char **prefix, int *start, int *end)
{
	char *range;
	char *first_part;
	char *ptr;
	char *end_str;
	char *start_str;
	int i,l;
	
	// Working on a separate copy
	range = strdup(r);
	
	if(!(ptr = strstr(range, "..")))
	{
		*prefix = range;
		*start = -1;
		return 1;
	}

	first_part = range;
	*ptr = '\0';
	
	// Finding the prefix and the start number
	l = strlen(first_part);
	for(i=0; i<l; i++)
		if(isdigit(first_part[i]))
			break;
	
	if(i == l)
	{
		fprintf(stderr, "First part of range does not have digits\n");
		goto bad_range;
	}
	
	start_str = &first_part[i];
	if(!is_number(start_str))
	{
		fprintf(stderr, "Start of range is not a number\n");
		goto bad_range;
	}
	*start = atoi(start_str);
	*start_str = '\0';
	*prefix = strdup(first_part);

	// Finding the end number
	ptr++; ptr++;
	if(!ptr)
	{
		fprintf(stderr, "Error bad range");
		goto bad_range;
	}

	end_str = ptr;
	if(!is_number(end_str))
	{
		fprintf(stderr, "End of range is not a number\n");
		goto bad_range;
	}
	*end = atoi(end_str);

	if(*end <= *start)
		goto bad_range;
	
	//printf("Range prefix: %s start: %d end: %d\n", *prefix, *start, *end);
	
	return 1;
	
 bad_range:
	free(range);
	return 0;
	
}

typedef struct range__
{
	char *prefix;
	int  start;
	int  end;
} range_t;


int count_range(range_t *r, int sz)
{
	int i;
	int num = 0;
	for(i=0 ; i<sz ; i++)
	{
		if(r[i].start == -1)
			num++;
		else {
			num += r[i].end - r[i].start + 1;
		}
	}
	return num;
}

void build_range(range_t *r, int r_sz, char **l)
{
	int i,j;
	int pos=0;
	char name[100];

	for(i=0 ; i<r_sz ; i++)
	{
		if(r[i].start == -1)
			l[pos++] = strdup(r[i].prefix);
		else {
			for(j = r[i].start ; j<= r[i].end ; j++)
			{
				sprintf(name,"%s%d", r[i].prefix, j);
				l[pos++] = strdup(name);
			}
		}
	}
}


int get_range_list(char *str, char ***list, int *size)
{
	char *plus_part = NULL,  *minus_part = NULL;
	range_t plus_range[100], minus_range[100];
	char **plus_list, **minus_list;
	char **final_list;
	int plus_range_sz, minus_range_sz;
	int plus_list_sz, minus_list_sz;
	
	char *ptr, *range;
	char *comma_delim = ",";
	int  i,j;
	
	char *range_prefix;
	int range_start, range_end;
		
	
	*size = 0;
	//printf("Getting Range\n");

	plus_part = str;
	if((ptr = strchr(str, '^')))
	{
		minus_part = ptr+1;
		*ptr = '\0';
	}
	
	//printf("Range: %s : %s \n", plus_part, minus_part);

	// Building the plus range
	plus_range_sz = 0;
	while((range = strsep(&plus_part, comma_delim)))
	{
		//printf("Plus range %s\n", range);
		if(!parse_range(range, &range_prefix, &range_start, &range_end))
		{
			//fprintf(stderr, "Bad range");
			return 0;
		}
		plus_range[plus_range_sz].prefix = range_prefix;
		plus_range[plus_range_sz].start = range_start;
		plus_range[plus_range_sz].end = range_end;

		// Way too much ranges
		if(++plus_range_sz == 100) 
			break;
	}
	

	// Building the minus list;
	minus_range_sz = 0;
	while((range = strsep(&minus_part, comma_delim)))
	{
		//printf("Minus range %s\n", range);
		if(!parse_range(range, &range_prefix, &range_start, &range_end))
		{
			//fprintf(stderr, "Bad range");
			return 0;
		}
		minus_range[minus_range_sz].prefix = range_prefix;
		minus_range[minus_range_sz].start = range_start;
		minus_range[minus_range_sz].end = range_end;

		// Way too much ranges
		if(++minus_range_sz == 100) 
			break;
	}
	
	// Building the final list (not a range list)
	// We first count the plus.
	plus_list_sz = count_range(plus_range, plus_range_sz);
	//printf("Size of plus: %d %d\n", plus_list_sz, plus_range_sz);
	plus_list = malloc(sizeof(char *) * plus_list_sz);
	build_range(plus_range, plus_range_sz, plus_list);

	minus_list_sz = count_range(minus_range, minus_range_sz);
	//printf("Size of minus: %d %d\n", minus_list_sz, minus_range_sz);
	if(minus_list_sz > 0) {
		minus_list = malloc(sizeof(char *) * minus_list_sz);
		build_range(minus_range, minus_range_sz, minus_list);
	}

	// deleting the unwanted nodes
	for(i=0 ; i<minus_list_sz ; i++)
	{
		//printf("i %d\n", i);
		for(j=0 ; j<plus_list_sz ; j++)
		{
			//printf("j %d\n", j);
			if(!plus_list[j])
				continue;
			if(!strcmp(minus_list[i], plus_list[j]))
			{
				free(plus_list[j]);
				plus_list[j] = NULL;
			}
		}
	}
	// counting the nodes left after deletion
	for(i=0, j=0 ; i<plus_list_sz ; i++)
		if(plus_list[i] != NULL)
			j++;

	// Allocating the final list and copy the nodes to it
	*size = j;
	final_list = malloc(sizeof(char *) * (*size));
	for(i=0, j=0 ; i<plus_list_sz ; i++)
		if(plus_list[i] != NULL)
			final_list[j++] = strdup(plus_list[i]);

	*list = final_list;
	// cleaning the temporary lists
	for(i=0 ; i<plus_list_sz ; i++)
		free(plus_list[i]);
	free(plus_list);

	if(minus_list_sz > 0) {
		for(i=0 ; i<minus_list_sz ; i++)
			free(minus_list[i]);
		free(minus_list);
	}
	return 1;
}



void get_dns_list(char *str, char ***list, int *size)
{
	struct hostent *he, *he2;
	//struct in_addr *addr;
	char **final_list;
	int i,j;
	
	*size = 0;
	//printf("Getting DNS\n");

	if(!(he = gethostbyname(str)))
	   return;

	// first counting the names
	for(i=0; he->h_addr_list[i] != 0 ; i++);
	*size = i;
	
	final_list = malloc(sizeof(char *) * (*size));
	// Now building the list
	for(i=0, j=0; he->h_addr_list[i] != 0 ; i++)
	{
		//addr = (struct in_addr *) he->h_addr_list[i];
		//printf("Address %s  ", inet_ntoa(*addr));
		if(!(he2 = gethostbyaddr(he->h_addr_list[i], he->h_length, AF_INET)))
			continue;
		final_list[j++] = strdup(he2->h_name);
		//printf("Name %s\n", he2->h_name);
	}
	*size = j;
	*list = final_list;
	
}

int mh_set(mon_hosts_t *mh, char *hosts_str)
{
	// first we check if we have a range or a single machine
	// If we get a single machine we try to find if it is a dns name
	// and if yes we build a list.
	int size;
	char **list;

	if(!mh || !hosts_str)
		return 0;

	strncpy(strBuff, hosts_str, 1023);
	strBuff[1023] = '\0';
	
	if(is_range(hosts_str))
		get_range_list(hosts_str, &list, &size);
	else
		get_dns_list(hosts_str, &list, &size);

	if(!size)
		return 0;

	mh->mh_list = list;
	mh->mh_nhost = size;
	return 1;
}


int mh_add(mon_hosts_t *mh, char *hosts_str)
{
	mon_hosts_t   newList;
	int           oldSize,i;
	char        **oldList;
	
	if(!mh || !hosts_str)
		return 0;

	strncpy(strBuff, hosts_str, 1023);
	strBuff[1023] = '\0';
	
	// Addition is done in a simple way
	mh_init(&newList);
	if(!mh_set(&newList, strBuff))
		return 0;
	
	oldSize = mh->mh_nhost;
	oldList = mh->mh_list;
	// Allocating extra space
	mh->mh_list = realloc(mh->mh_list, (mh->mh_nhost + newList.mh_nhost)*sizeof(char*));
	if(!mh->mh_list)
		return 0;

	// Copying the new list to the extra space in the old list
	for(i=0 ; i < newList.mh_nhost ; i++)
		mh->mh_list[mh->mh_nhost + i] = strdup(newList.mh_list[i]);
	
	mh->mh_nhost += newList.mh_nhost;
	mh->mh_curr = 0;
	// Deallocating the list
	mh_free(&newList);
	return 1;
}


// Setting a default list of nodes for debugging
int mh_set_fixed(mon_hosts_t *mh)
{
	int i;
	char *list[] = {"mos1", "mos2", "mos3", "bmos-14", "cmos-01"};
	
	mh->mh_nhost = 5;
	mh->mh_curr = 0;
	mh->mh_list = malloc(mh->mh_nhost * sizeof(char *));
	for(i=0 ; i < mh->mh_nhost ; i++)
		mh->mh_list[i] = strdup(list[i]);
	
	return 1;
}

/*

int main(int argc, char **argv)
{
	mon_hosts_t list;
	int i;
	
	mh_init(&list);
	mh_set_fixed(&list);
	mh_print(&list);
	
	printf("Checking mh_next:\n");
	for(i=0 ; i< 2*mh_size(&list) ; i++)
		printf("Next %s\n", mh_next(&list));


	if(argc > 1)
	{
		printf("Checking mh_set from argv[1]: %s\n", argv[1]);
		mh_free(&list);
		mh_init(&list);
		mh_set(&list, argv[1]);
		mh_print(&list);
	}
	printf("Checking mh_next:\n");
	for(i=0 ; i< 2*mh_size(&list) ; i++)
		printf("Next %s\n", mh_next(&list));

}
*/
