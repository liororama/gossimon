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
#include <string.h>

struct mem_cache_struct
{
	int              size;       // Maximal number of elements 
	void             **ptrArr;   // Array of pointers to proc entry
	char             *allocArr;  // Indicate if memory is allocated or not
	int              nextAlloc;  // The position of the next ptr to alloc
	int              entrySize;  // Size of data entry 
	void             *dataArr;   // The data array
};

#include "mem_cache.h"



mem_cache_t mem_cache_create(int size, int entrySize)
{
	mem_cache_t ptr;
	int i;
	
	// Allocating the struct itself
	if(!(ptr = malloc(sizeof(struct mem_cache_struct))))
	   return NULL;

	ptr->size = size;
	ptr->entrySize = entrySize;
	ptr->nextAlloc = size - 1;

	// Allocating the arrays
	if(!(ptr->ptrArr = malloc(sizeof(void *) * size)) ||
	   !(ptr->dataArr = malloc(entrySize * size))     ||
	   !(ptr->allocArr = malloc(sizeof(char) * size)))
	{
		free(ptr->ptrArr);
		free(ptr->dataArr);
		free(ptr);
	}

	bzero(ptr->dataArr, size*entrySize);
	
	// All memory is allocated, setting the pointers
	for(i=0 ; i<size ; i++)
	{
		ptr->ptrArr[i] = ((char *)ptr->dataArr) + i*entrySize;
		ptr->allocArr[i] = 0;
	}
	
	return ptr;
}

// Very dangerous, the data is freed but there might be references to it
// outside
int mem_cache_destroy(mem_cache_t cache)
{
	if(!cache)
		return 0;
	free(cache->ptrArr);
	free(cache->dataArr);
	free(cache->allocArr);
	free(cache);
	return 1;
}


void *mem_cache_alloc(mem_cache_t cache)
{
	void *ptr;
	int n;
	
	if(!cache || cache->nextAlloc < 0)
		return NULL;

	// We have more item to allocate
	ptr = cache->ptrArr[cache->nextAlloc--];
	n = (ptr - cache->dataArr) / cache->entrySize;
	cache->allocArr[n] = 1;
	return ptr;
}

// Returning the position of a leagal pointer
static int get_ptr_pos(mem_cache_t c, void *ptr)
{
	int n;
	n = (char*)ptr - (char *)c->dataArr;
	return n/c->entrySize;
}


static int is_ptr_leagal(mem_cache_t cache, void *ptr)
{
	int n;
	
	// If the pointer is from our pool
	if((char *)ptr < (char*)cache->dataArr ||
	   (char *)ptr > ((char *)cache->dataArr + cache->size*cache->entrySize))
	{
		//fprintf(stderr, "Error pointer is not from this pool\n");
		return 0;
	}
	
	// Checking allignment
	n = (char*)ptr - (char *)cache->dataArr;
	if( n % cache->entrySize)
	{
		//fprintf(stderr, "Error: pointer is not in right allignment\n");
		return 0;
	}

	return 1;
}

// ptr should be from our pool
int mem_cache_free(mem_cache_t cache, void *ptr)
{
	if(!cache)
		return 0;
	
	if(!is_ptr_leagal(cache, ptr))
		return 0;

	
	// If the area is not taken
	if(!cache->allocArr[get_ptr_pos(cache,ptr)])
	{
		//fprintf(stderr, "Error: trying to free a not taken area\n");
		return 0;
	}
	
	cache->allocArr[get_ptr_pos(cache,ptr)] = 0;
	cache->nextAlloc++;
	cache->ptrArr[cache->nextAlloc] = ptr;
	
	// Zeroing the memory
	bzero(ptr, cache->entrySize);
	
	return 1;
}

int mem_cache_size(mem_cache_t cache)
{
	if(!cache)
		return 0;
	return cache->size;
}
int mem_cache_free_size(mem_cache_t cache)
{
	if(!cache)
		return 0;
	return cache->nextAlloc + 1;
}
int mem_cache_taken_size(mem_cache_t cache)
{
	if(!cache)
		return 0;
	return cache->size - cache->nextAlloc - 1;
}

int mem_cache_resize(mem_cache_t cache, int new_size)
{
	if(!cache)
		return 0;

	return 1;

}



int mem_cache_revalidate(mem_cache_t cache)
{
	int i;
	int numAlloc;

	int *numPointed;
	
	if(!cache)
		return 0;

	
	// First check that the allcArr is according to the allocNext
	for(i=0, numAlloc=0 ; i < cache->size ; i++)
		if(cache->allocArr[i])
			numAlloc++;
	if(numAlloc != (cache->size - cache->nextAlloc - 1))
		return 0;
	
	// Checking that all the pointers which are currently in the cache
	// points to leagal locations And that no two pointers point to the
	// same location

	if(!(numPointed = malloc(sizeof(int) * cache->size)))
		return 0;
	bzero(numPointed, sizeof(int) * cache->size);
	
	for(i=0 ; i<= cache->nextAlloc ; i++)
	{
		int ptrPos;
		
		if(!is_ptr_leagal(cache, cache->ptrArr[i]))
			return 0;
		ptrPos = get_ptr_pos(cache, cache->ptrArr[i]);
		if(numPointed[ptrPos])
			return 0;

		numPointed[ptrPos] = 1;
	}
	return 1;
}
