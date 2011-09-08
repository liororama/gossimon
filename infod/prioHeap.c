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
 * File: prio_heap.c. Implementation of a priority heap.
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <prioHeap.h>

#define PARENT(i)          ((i)/2)
#define LEFT(i)            ((i)<<1)
#define RIGHT(i)           (((i)<<1) + 1)
#define HDATA(h,i)         ((h)->h_data[(i)].data)
#define HDATALEN(h,i)      ((h)->h_data[(i)].datalen)
#define HKEY(h,i)          ((h)->h_data[(i)].prio)

/* A single entry in the priority queue */
typedef struct prio_queue_data_t {
	heap_key_t    prio;           // priority 
	int           datalen;        // the length of the data 
	void          *data;          // buffer for the data
} heap_data_t;


/****************************************************************************
 * The heapify routine 
 ***************************************************************************/
void
heapify( heap_t *h, int i ) {
	int left = 0, right = 0, largest = 0;
	unsigned char done = 0;
  
	largest = i;
	while( !done ) {
		left = LEFT(i);
		right = RIGHT(i);
	  
		if( (left <= h->h_size) && (HKEY(h,left) < HKEY(h,i)))
			largest = left;
		else
			largest = i;
		if( (right <= h->h_size ) &&
		    ( HKEY(h, right) < HKEY(h,largest)))
			largest = right;

		
		/* Perform the swap */
		if( largest != i) {
			heap_data_t tmp_data;
			tmp_data = h->h_data[i];
			h->h_data[i] = h->h_data[largest];
			h->h_data[largest] = tmp_data;
			i=largest;
		}
		else
			done = 1 ;
	}
}

/****************************************************************************
 * Build the heap from an unsorted array
 ***************************************************************************/
void
build_heap(heap_t *h) {
	int i;
	for( i = h->h_size/2 ; i >= 1  ; i-- ) 
		heapify(h,i);
}

/****************************************************************************
 * Initialize the heap. Note that the memory for heap_t must be already
 * be allocated. 
 ***************************************************************************/
int
heap_init( heap_t *h, int size ) {
	int i;
	
	if( !h )
		return 0;

	if( !(h->h_data = malloc( size * sizeof(heap_data_t))))
		return 0;
	
	h->h_length = size;
	h->h_size = 0;
	
	for( i = 0 ; i < h->h_length ; i++ ) {
		h->h_data[ i ].prio = 0;
		h->h_data[ i ].datalen = 0;
		h->h_data[ i ].data = NULL;
	}
	
	return 1;
}

/****************************************************************************
 * Free the heap.
 ***************************************************************************/
int
heap_free( heap_t *h ) {
	if( !h || !h->h_data  )
		return 0;
        free( h->h_data );
	return 1;
}

/****************************************************************************
 * Reset the heap.
 ***************************************************************************/
int
heap_reset( heap_t *h ) {

	if( !h )
		return 0;

	h->h_size = 0;
	return 1;
}

/****************************************************************************
 * Returns the maximal size of the heap
 ***************************************************************************/
int
heap_size( heap_t *h ) {
	return ( h == NULL )?0:h->h_size; 
}

/****************************************************************************
 * Returns 1 if the heap is empty 
 ***************************************************************************/
int
heap_is_empty( heap_t *h ) {
	return ( ( h == NULL ) || ( h->h_size <= 0 )) ? 1 : 0;
}

/****************************************************************************
 * Returns 1 if the heap is full
 ***************************************************************************/
int
heap_is_full( heap_t *h ) {
	return ( ( h == NULL ) || ( h->h_size == h->h_length ))? 1:0;
}

/****************************************************************************
 * Get the maximal heap value
 ***************************************************************************/
heap_key_t
heap_get_max_key( heap_t *h, void **data, int *datalen ) {
	if(h->h_size > 0) {
		*data = HDATA(h,1);
		*datalen = HDATALEN(h,1);
		return HKEY(h,1);
	}
	else
		return -1;
}

/****************************************************************************
 * Insert a new value to the heap. Note that *data should not be freed,
 * unless it was first removed from the heap. 
 ***************************************************************************/
int
heap_insert( heap_t *h, heap_key_t key, void *data, int datalen ) {
	int i;

	/* no place left, return 0 */
	if( h->h_size == (h->h_length -1) )
		return 0;

	/* put the new node at the bottom of the heap */ 
	h->h_size++;
	i = h->h_size;
	
	while( i>1 && HKEY(h, PARENT(i)) > key ) {
		HKEY(h,i)     =  HKEY(h, PARENT(i));
		HDATA(h,i)    =  HDATA(h, PARENT(i));
		HDATALEN(h,i) = HDATALEN(h, PARENT(i));
		i = PARENT(i);
	}
	
	HKEY(h,i) = key;
	HDATA(h,i) = data;
	HDATALEN(h,i) = datalen;
	
	return 1;
}

/****************************************************************************
 * Remove the entry at he head of the heap and return it.
 ***************************************************************************/
heap_key_t
heap_extract_max( heap_t *h, void **data, int *datalen ) {
	heap_key_t max;

	/* Heap is empty */ 
	if( h->h_size == 0 ) 
		return -1;
	
	max = HKEY(h,1);
	*data = HDATA(h,1);
	*datalen = HDATALEN(h,1);
	
	HKEY(h,1) = HKEY(h, h->h_size);
	HDATA(h,1) = HDATA(h, h->h_size);
	HDATALEN(h,1) = HDATALEN(h, h->h_size);
	
	HKEY(h, h->h_size) = -1;
	h->h_size--;
	
	heapify(h,1);
	return max;
}

/****************************************************************************
 * Delete an entry from the heap
 ***************************************************************************/
heap_key_t
heap_delete( heap_t *h, int i, void **data, int *datalen ) {
	heap_key_t key;
	
	if( (h->h_size == 0) || (i < 1  || i > h->h_size ))
		return -1;

	key = HKEY(h,i);
	*data = HDATA(h,i);
	*datalen = HDATALEN(h,i);
	
	HKEY(h,i) = HKEY(h, h->h_size);
	HKEY(h, h->h_size) = -1;
	HDATA(h,i) = HDATA(h, h->h_size);
	HDATA(h, h->h_size) = NULL;
	HDATALEN(h,i) = HDATALEN(h, h->h_size);
	HDATALEN(h, h->h_size) = -1;
	
	h->h_size--;
	
	heapify(h,i);
	return key;
}

/****************************************************************************
 * Print the heap, according to the order
 ***************************************************************************/
void
print_heap(heap_t *h) {
	int i;
	 
	printf("\n-------------------------------\n");
	for(i=0 ; i <= h->h_size + 2 ; i++ ) {
		if(i == h->h_size + 1)
			printf("-------- size ---------\n");
		printf("pos %5d \t%5ld  %s\n", i,HKEY(h,i),
		       (char *)HDATA(h,i));
	}
}


#ifdef A
int main()
{
	heap_t hh;
	char *str1, *str2, *str3, *str4, *str5;
	
	str1 = strdup("String 1");
	str2 = strdup("String 2");
	str3 = strdup("String 3");
	str4 = strdup("String 4");
	str5 = strdup("String 5");
	
	heap_init(&hh,30);

	heap_insert(&hh, 14, str1, strlen(str1)+1);
	heap_insert(&hh, 3, str2, strlen(str2)+1);
	heap_insert(&hh, 5, str3, strlen(str3)+1);
	heap_insert(&hh, 1, str4, strlen(str4)+1);
	print_heap(&hh);

	printf("\n\n");
	while(!heap_is_empty(&hh))
	{
		char *ptr;
		int  len;
		int  prio;

		prio = heap_extract_max(&hh, (void **) &ptr, &len);

		printf("Got max %5d %s\n", prio, ptr);
	}
	heap_insert(&hh, 1, str5, strlen(str5)+1);
	print_heap(&hh);
	return 0;
}
#endif /* A */



