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
 *
 * File: info_reader.h, used to read variable values from a buffer 
 *
 ****************************************************************************/

#ifndef _INFO_READER_H
#define _INFO_READER_H

#include <info.h>

/****************************************************************************
 * Interface used to parse the descirption
 ***************************************************************************/

#define STR_LEN (32)
#define ROOT_NAME       "local_info"
#define BASE_TAG        "base"
#define EXTRA_TAG       "extra"
#define VLEN_TAG        "vlen"
#define EXTERNAL_TAG    "external"


#define NAME_TAG        "name"
#define TYPE_TAG        "type"
#define DEFVAL_TAG      "defval"
#define UNIT_TAG        "unit"
#define SIZE_TAG        "size"

typedef struct var {
        char class[ STR_LEN ];  // The base, extra, vlen ...>
        char name[ STR_LEN ];   // The name of the item
        char type[ STR_LEN ];   // the type int, short, string...
        char unit[ STR_LEN ];   // Unit of item MB Kb/sec..
        int  def_val;           // default value that should be used
        unsigned short  offset; // Offset from the start of the item
        unsigned short  size;   // size (in bytes) of info item
} var_t;

typedef struct variable_map {
    int num;
    int entry_sz;
    var_t *sorted_vars;
    var_t *vars;
} variable_map_t ;

#define  VMAP_ENTRY_SZ  (sizeof(var_t))
#define  VMAP_SZ        (sizeof(variable_map_t))

/*
 * This function creates a mapping between the names of the variables
 * in the node_info_t structure and their offsets 
 */
variable_map_t* create_info_mapping( char* desc );

/*
 * frees memory allocated by create_info_mapping()
 */
void
destroy_info_mapping( variable_map_t* mapping );

/*
 * Get the structure describing the variable
 */
var_t* get_var_desc( variable_map_t *mapping, char *key );

/*
 * For variable length items we should use this function to get the correct
 * offset of a variable length item
 */
int is_var_vlen(var_t *v);
/*
 * Printing functions for debugging
 */
void var_print( var_t *var );

void variable_map_print( variable_map_t *mapping );

#endif

/****************************************************************************
 *                      E O F 
 ***************************************************************************/
