/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


/*****************************************************************************
 *    File: infod_data_iter.h
 *****************************************************************************/

#ifndef _INFOD_DATA_ITER_H
#define _INFOD_DATA_ITER_H

#include <info.h>
#include <sys/time.h>
#include <msx_error.h>

/****************************************************************************
 * Iterator used to run over 
 ***************************************************************************/

typedef struct idata_iter {
	idata_t       *data;
	idata_entry_t *cur;
	int index;
} idata_iter_t ; 

#define IDATA_ITER_SZ    (sizeof(idata_iter_t))

/****************************************************************************
 * Interface functions 
 ***************************************************************************/

/* Create an iterator  */
idata_iter_t *idata_iter_init( idata_t *data );

/* Get the current position of the iterator */
idata_entry_t*  idata_iter_cur( idata_iter_t *iter );

/* Get the current position and move to thenext one */
idata_entry_t* idata_iter_next( idata_iter_t *iter );

/* Get the next position */ 
idata_entry_t* idata_iter_get_next( idata_iter_t *iter );

/* Get the current position and move to the previous one */ 
idata_entry_t* idata_iter_get_prev( idata_iter_t *iter );

/* Rewind the iterator */
idata_entry_t* idata_iter_rewind( idata_iter_t *iter );

/* FastForward the iterator (move to the end position) */
idata_entry_t* idata_iter_fast_forward( idata_iter_t *iter );

/* Delete the current position, copy back  */
idata_entry_t* idata_iter_delete_cur( idata_iter_t *iter );

/* Get the value in current position by XML key */
int idata_iter_get_value_by_key( idata_iter_t *iter,
				 void *map, char* key, void* out );

/* Set the value in current position by XML key */
int idata_iter_set_value_by_key( idata_iter_t *iter,
				 void *map, char* key, void* in);

/* Stop iterating */
void idata_iter_done( idata_iter_t *iter );

#endif
				
/***************************************************************************
                                E O F
****************************************************************************/
