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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#include "mem_cache.h"

typedef struct dd {
	int i;
	int j;
	char name[10];
} dd_t;


int test_simple_alloc_free(int size)
{
	mem_cache_t c;
	dd_t **dp;
	int i;
       
	if(!(dp = (dd_t **) malloc(size * sizeof(dd_t *))))
	{
		printf("Error: allocating array of pointers\n");
		return 0;
	}
	
	// First going all over the list
	c = mem_cache_create(size, sizeof(dd_t));
	if(!c) {
		printf("Error allocating cache\n");
		return 0;;
	}
	
	for(i=0 ; i< size ; i++)
	{
		dp[i] = mem_cache_alloc(c);
		if(dp[i]) {
			//printf("Alloc ok\n");
			dp[i]->i = 1110;
		}
		else {
			printf("Alloc BAD %d\n", i);
			return 0;
		}
	}
	
	for(i=0 ; i< size ; i++)
	{
		if(!mem_cache_free(c, (void *)dp[i]))
		{
			printf("Free BAD\n");
			return 0;
		}
	}
	
	
	return 1;
}

int test_boundry_alloc_free(int size)
{
	mem_cache_t c;
	dd_t **dp;
	dd_t *ptr;
	int i;
 
	if(!(dp = (dd_t **) malloc(size * sizeof(dd_t *))))
	{
		printf("Error: allocating array of pointers\n");
		return 0;
	}
	
	c = mem_cache_create(size, sizeof(dd_t));
	if(!c) {
		printf("Error allocating cache\n");
		return 0;
	}
	
	for(i=0 ; i< size ; i++)
	{
		dp[i] = mem_cache_alloc(c);
		if(dp[i]) {
			//printf("Alloc ok\n");
			dp[i]->i = 1110;
		}
		else {
			printf("Alloc BAD %d\n", i);
			return 0;
		}
	}

	// Allocating one more
	if((ptr = mem_cache_alloc(c)))
	{
		printf("Error: Allocating more then size\n");
		return 0;
	}

	
	// Trying to free outside array
	if(mem_cache_free(c, ((char *)dp[0] + 2*sizeof(dd_t))))
	{
		printf("Error: Free outside boundries\n");
		return 0;
	}

	// Tring to free non alligned pointer
	if(mem_cache_free(c, ((char *)dp[0] + 1)))
	{
		printf("Error: Free unaligned pointer \n");
		return 0;
	}
	// Performing free
	for(i=0 ; i< size ; i++)
	{
		if(!mem_cache_free(c, (void *)dp[i]))
		{
			printf("Free BAD\n");
			return 0;
		}
	}

	// Performing a scond free
	if(mem_cache_free(c, dp[0]))
	{
		printf("Error: Freed twice\n");
		return 0;
	}
	
	
	return 1;
}


// Perform massive actions with the library and home that something bad will
// happen
int test_random_alloc_free(int size, int num)
{
	mem_cache_t c;
	dd_t **dp;
	dd_t *ptr;
	int curr_size;
	int i,j;
	int action;
		
		
	if(!(dp = (dd_t **) malloc( (size + 10) * sizeof(dd_t *))))
	{
		printf("Error: allocating array of pointers\n");
		return 0;
	}
	
	c = mem_cache_create(size, sizeof(dd_t));
	if(!c) {
		printf("Error allocating cache\n");
		return 0;
	}	

	srandom(getpid());
	curr_size = 0;
	for(i=0 ; i < num ; i++) {
		action = random() % 2;
		// Performing an allocation
		if(action == 0)
		{
			if( (ptr = mem_cache_alloc(c)) )
			{
				dp[curr_size++] = ptr;
			}
		}
		// Performing a free
		else {
			if(!curr_size)
				continue;
			
			j = random() % curr_size;
			if(mem_cache_free(c, dp[j])) {
				memmove(&dp[j], &dp[j+1], curr_size - j -1);
				curr_size--;
			}
		}
	}
	return mem_cache_revalidate(c);
}

int test_create_distroy()
{
	mem_cache_t c;
	int i;
	
	for(i=0 ; i < 1000 ;  i++)
	{
		c = mem_cache_create(500, sizeof(dd_t));
		if(!c) {
			printf("Error allocating cache\n");
			return 0;
		}
		
		if(! mem_cache_destroy(c))
		{
			printf("Error distroying cache\n");
			return 0;	
		}
	}
	return 1;
}

/*
int test_revalidate(int size)
{
	mem_cache_t c;
	dd_t **dp;
	dd_t *ptr;
	int curr_size;
	int i,j;
	int action;
		
		
	if(!(dp = (dd_t **) malloc( (size + 10) * sizeof(dd_t *))))
	{
		printf("Error: allocating array of pointers\n");
		return 0;
	}
	
	c = mem_cache_create(size, sizeof(dd_t));
	if(!c) {
		printf("Error allocating cache\n");
		return 0;
	}

	for(i=0 ; i< size - 10 ; i++)
	{
		dp[i] = mem_cache_alloc(c);
		if(dp[i]) {
			//printf("Alloc ok\n");
			dp[i]->i = 1110;
		}
		else {
			printf("Alloc BAD %d\n", i);
			return 0;
		}
	}


	// We change the cache and check if the revalidate work
	c->nextAlloc--;
	if(!mem_cache_revalidate(c))
		return 0;
	
	c->nextAlloc++;

	c->ptrArr[c->nextAlloc - 1] = 	c->ptrArr[c->nextAlloc - 5];  
	if(!mem_cache_revalidate(c))
		return 0;
	return 1;
	
}

*/

void print_ok()  { printf("[OK]\n") ;}
void print_bad() { printf("[BAD]\n") ;} 

int main()
{


	fprintf(stderr, "Testing create distroy ...... ");
	if(!test_create_distroy())
	{
		print_bad();
		exit(1);
	}
	else
		print_ok();
	

	fprintf(stderr, "Testing simple alloc free ...... ");
	if(!test_simple_alloc_free(500))
	{
		print_bad();
		exit(1);
	}
	else
		print_ok();
	
	fprintf(stderr, "Testing boundry alloc free ...... ");
	if(!test_boundry_alloc_free(500))
	{
		print_bad();
		exit(1);
	}
	else
		print_ok();
	
	fprintf(stderr, "Testing random alloc free ...... ");
	if(!test_random_alloc_free(500, 100))
	{
		print_bad();
		exit(1);
	}
	else
		print_ok();
	
	/*
	fprintf(stderr, "Testing revalidate ...... ");
	if(!test_revalidate(500))
	{
		print_bad();
		exit(1);
	}
	else
		print_ok();
	*/
	
	
	return 0;
}
