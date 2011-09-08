/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __MEM_CACHE_
#define __MEM_CACHE_

/*
  A cache of memory. We allocate it once and then perform get and free
  All the entries have the same size.
*/

typedef struct mem_cache_struct *mem_cache_t;

/****************************************************************************
 * Description:
 *    Initialzing a cache object
 *
 * Arguments:
 *    cache     : The object to initialize
 *    size      : The number of data elements to create
 *    entrySize : The size of individual data element
 *
 * Return value:
 *         True  : On succes
 *         False : On Error
 ****************************************************************************/
mem_cache_t mem_cache_create(int size, int entrySize);

/****************************************************************************
 * Description:
 *    Destroying a cache object
 *
 * Arguments:
 *    cache     : The object to destroy
 *
 * Return value:
 *         True  : On succes
 *         False : On Error
 ****************************************************************************/
int mem_cache_destroy(mem_cache_t cache);

/****************************************************************************
 * Description:
 *    Allocating an entry from the cache
 *
 * Arguments:
 *    cache     : The object to allocate from
 *
 * Return value:
 *         True  : On succes
 *         False : On Error (no more free entries or object is bad
 ****************************************************************************/
void *mem_cache_alloc(mem_cache_t cache);

/****************************************************************************
 * Description:
 *    Returning an entry to the cache
 *
 * Arguments:
 *    cache     : The object to return the entry to
 *    ptr       : Pointer to the entry
 *  
 * Return value:
 *         True  : On succes
 *         False : On Error 
 ****************************************************************************/
int mem_cache_free(mem_cache_t cache, void *ptr);

/****************************************************************************
 * Description:
 *    Returning the total size, free size or taken size of the cache object
 *
 * Arguments:
 *    cache     : The object to query
 *  
 * Return value:
 *         size >=0 : The requested size
 *         -1       : On Error (cache object is not ok) 
 ****************************************************************************/
int mem_cache_size(mem_cache_t cache);
int mem_cache_free_size(mem_cache_t cache);
int mem_cache_taken_size(mem_cache_t cache);


/****************************************************************************
 * Description:
 *    Performing a resize of the cache object. 
 *
 * Arguments:
 *    cache      : The object to resize
 *    new_size   : The new size
 *
 * Return value:
 *         True  : On succes
 *         False : On Error
 ****************************************************************************/

int mem_cache_resize(mem_cache_t cache, int new_size);



/****************************************************************************
 * Description:
 *    Revalidate the cache structure
 *
 * Arguments:
 *    cache      : The object to revalidate
 *
 * Return value:
 *         True  : On succes
 *         False : On Error
 ****************************************************************************/

int mem_cache_revalidate(mem_cache_t cache);

#endif
