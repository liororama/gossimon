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
 * Author(s): Peer Ilan, Amar Lior
 *
 *****************************************************************************/

/******************************************************************************
 *
 * File: prio_heap.h. Defines a priority heap.
 *
 *****************************************************************************/

#ifndef __MSX_INFOD_PRIO_HEAP__
#define __MSX_INFOD_PRIO_HEAP__

typedef unsigned long heap_key_t;

typedef struct _prio_heap {
	unsigned int             h_size;    // The actual number of elements
	unsigned int             h_length;  // The max number of elements
	struct prio_queue_data_t *h_data;
} heap_t;

/****************************************************************************
 * Functions for manipulating the heap.
 ***************************************************************************/

void         heapify( heap_t *h, int i );
void         build_heap( heap_t *h );
int          heap_init(heap_t *h, int size);
int          heap_free(heap_t *h);
int          heap_reset(heap_t *h);
int          heap_size(heap_t *h);
int          heap_is_empty(heap_t *h);
int          heap_is_full(heap_t *h);
heap_key_t   heap_get_max_key(heap_t *h, void **data, int *datalen);
int          heap_insert(heap_t *h, heap_key_t key, void *data, int datalen);
heap_key_t   heap_extract_max(heap_t *h, void **data, int *datalen);
heap_key_t   heap_delete(heap_t *h, int i, void **data, int *datalen);

#endif /* __MSX_INFOD_PRIO_HEAP__ */

/****************************************************************************
 *                                 E O F 
 ***************************************************************************/
