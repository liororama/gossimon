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
 * Author(s): Amar Lior, Peer Ilan
 *
 *****************************************************************************/

/*****************************************************************************
 *    File: infod_data_iter.c
 *****************************************************************************/
#include <msx_debug.h>
#include <msx_error.h>

#include <info_iter.h>
#include <stdlib.h>
#include <string.h>
#include <info_reader.h>

/****************************************************************************
 * Create an iterator
 ***************************************************************************/
idata_iter_t*
idata_iter_init( idata_t *data ) {

	idata_iter_t *iter = NULL;
    
	if( !data ) {
		debug_r( "Error: illgal args, infod_data_iter_init\n" );
		return NULL;
	}

	if( !( iter = (idata_iter_t*) malloc( IDATA_ITER_SZ ))) {
		debug_r( "Error: malloc failed\n" );
		return NULL;
	}

	iter->data = data;
	iter->index = 0;
    
	if( data->num == 0 )
		iter->cur = NULL;
	else
		iter->cur = iter->data->data;
    	return iter;
}

/****************************************************************************
 * Get the current position of the iterator
 ***************************************************************************/
idata_entry_t*
idata_iter_cur( idata_iter_t *iter ) {
	if( iter )
		return iter->cur;
	return NULL;
}

/****************************************************************************
 * Get the current position and move to the next one
 ***************************************************************************/ 
idata_entry_t*
idata_iter_next( idata_iter_t *iter ){

	idata_entry_t *cur = NULL;
	
	if( !iter || !iter->data )
		return NULL;

	cur = iter->cur ;
	if( iter->index < iter->data->num - 1 ){
		void *ptr = ((void*)(cur)) + cur->size;
		iter->cur = (idata_entry_t*)ptr; 
	}
	else
		iter->cur = NULL;

	if( iter->index != iter->data->num )
		iter->index++;
    
	return cur;
}

/****************************************************************************
 * Get the next position
 ***************************************************************************/ 
idata_entry_t*
idata_iter_get_next( idata_iter_t *iter ) {

	idata_entry_t *cur = NULL;

	if( !iter || !iter->data )
		return NULL;
	
	if( iter->index < (iter->data->num - 1) ) {
		void *ptr = ((void*)(iter->cur)) + iter->cur->size;
		iter->cur = (idata_entry_t*)ptr; 
		cur = iter->cur ;
		iter->index++;
	}

	return cur;
}

/****************************************************************************
 * Get the previous position
 ***************************************************************************/ 
idata_entry_t*
idata_iter_get_prev( idata_iter_t *iter ) {

	idata_entry_t *cur = NULL;

	if( !iter || !iter->data )
		return NULL;

	if( iter->index > 0 ) {
		void *ptr = ((void*)(iter->cur)) - iter->cur->size;
		iter->cur = (idata_entry_t*)ptr; 
		cur = iter->cur ;
		iter->index--;
	}
	return cur;
}

/****************************************************************************
 * Rewind the iterator
 ***************************************************************************/
idata_entry_t*
idata_iter_rewind( idata_iter_t *iter ) {

	if( !iter )
		return NULL;

	iter->cur = iter->data->data;
	iter->index = 0;
	return iter->cur;
}

/****************************************************************************
 * FastForward the iterator (return the last entry of the information 
 ***************************************************************************/
idata_entry_t*
idata_iter_fast_forward( idata_iter_t *iter ) {

	if( !iter )
		return NULL;

	for( ; iter->index < iter->data->num ; iter->index++ ) {
		void *ptr = ((void*)(iter->cur)) + iter->cur->size;
		iter->cur = (idata_entry_t*)ptr; 
	}
	return iter->cur;
}

/****************************************************************************
 * Delete the current position, copy back 
 ***************************************************************************/
idata_entry_t*
idata_iter_delete_cur( idata_iter_t *iter ) {
	
	return NULL;
}

/****************************************************************************
 * Get the value in current position by XML key
 ***************************************************************************/
int
idata_iter_get_value_by_key( idata_iter_t *iter,
			     void *m, char* key, void* out ) {
	var_t* var;
	variable_map_t *map = m;

	if( !( var = get_var_desc( map, key )))
		return -1;

	memcpy( out, iter->cur + var->offset, var->size );
	return 0;
}

/****************************************************************************
 * Set the value in current position by XML key
 ***************************************************************************/
int
idata_iter_set_value_by_key( idata_iter_t *iter,
				  void *m, char* key, void* in ) {
	var_t* var;
	variable_map_t *map = m;

	if( !( var = get_var_desc( map, key )))
		return -1;

	memcpy( iter->cur + var->offset, in, var->size );
	return 0;
}

/****************************************************************************
 * Stop iterating
 ***************************************************************************/
void
idata_iter_done( idata_iter_t *iter ) {

	if( !iter )
		return;
    
	iter->data = NULL;
	iter->cur  = NULL;
	iter->index = 0 ;
	free( iter );
}

/****************************************************************************
 *                               E O F 
 ***************************************************************************/
