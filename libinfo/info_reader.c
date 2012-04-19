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
 * Author(s): Peer Ilan
 *
 *****************************************************************************/


/*****************************************************************************
 *
 * File: info_reader.c, used to read variable values from a buffer 
 *
 ****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <info_reader.h>
#include <msx_error.h>
#include <info_iter.h>
#include <ctype.h>
#include <stdlib.h>

#include <msx_debug.h>
#include <msx_error.h>

#include <ModuleLogger.h>

#define  XML_TAG_BASE        "base"
#define  XML_TAG_EXTRA       "extra"
#define  XML_TAG_VLEN        "vlen"
#define  XML_TAG_EXTERNAL    "external"
#define  INC_SIZE  (32)
#define  SMALL_BUFF_SZ (256)

/****************************************************************************
 * Local functions
 ***************************************************************************/

/*
 * Used for the sorting of the var_t array in mappings
 * and for binary search
 */
static int
var_cmp( const void *a, const void *b ) {

	var_t *n1 = (var_t*)a;
	var_t *n2 = (var_t*)b;
	return strcmp( n1->name, n2->name );
}

/****************************************************************************
 * Remove leading white spaces
 ***************************************************************************/
static char*
remove_ws( char *str ) {
	for( ; str && *str != '\0' && isspace( *str ) ; str++ );
	return str; 
}

/****************************************************************************
 * Return the position of the end of line character
 ***************************************************************************/
static char*
get_endof_line( char *str ) {
	for( ; str && *str != '\0' && *str != '\n' ; str++ );

	if( (*str == '\n') || (*str == '\0') )
		return str;
	return NULL; 
}

/****************************************************************************
 * Get the root of the information
 ***************************************************************************/
static char*
get_root_tag( char *str ) {

	char *ptr = NULL, *end = NULL;
	if( !( ptr = remove_ws( str ) ) )
		goto failed;

	if( *ptr != '<' )
		goto failed;

	ptr++;
	if( !( ptr = remove_ws( ptr ) ) )
		goto failed;

	for( end = ptr; end && ( *end != '\0' ) && !( isspace( *end )) &&
		     *end != '>' ; end++ );
	
	if( isspace(*end) || *end == '>' ) {
		*end = '\0';
		return strdup( ptr );
	}

 failed:
	debug_r( "Error: invalid root tag\n" );
	return NULL;
}

/****************************************************************************
 * Validate the xml root 
 ***************************************************************************/
static char*
get_xml_root( char *str ) {

	char *ptr = NULL;
	ptr = remove_ws( str );

	if( strncmp( XML_ROOT_TAG, str, strlen( XML_ROOT_TAG )) != 0 ) {
		debug_r( "Error: invalid root element\n" );
		return NULL;
	}
	
	ptr += strlen( XML_ROOT_TAG );
	return ptr;
}

/****************************************************************************
 * Test that the line is a closing tag of the root 
 ***************************************************************************/
static int
test_closing_tag( char *str, char* root ) {

	if( *str != '/' )
		return 0;
	str++;

	if( strncmp( str, root, strlen( root )) != 0 )
		return 0;
	str += strlen( root );

	if( !( str = remove_ws( str )))
		return 0;

	if( *str != '>' )
		return 0;

	return 1;
}

/****************************************************************************
 * Validate the name of the tag. Should be XML_TAG_BASE or XML_TAG_EXTRA
 ***************************************************************************/
static char*
validate_tag_name(var_t *cur,  char *str ) {

	char *ptr = NULL;
	ptr = remove_ws( str );

	if( strncmp( XML_TAG_BASE, str, strlen( XML_TAG_BASE )) == 0 ) {
                strcpy(cur->class_type, XML_TAG_BASE);
                ptr += strlen( XML_TAG_BASE );
		return ptr;
	}
	else if( strncmp( XML_TAG_EXTRA, str, strlen( XML_TAG_EXTRA )) == 0 ) {
                strcpy(cur->class_type, XML_TAG_EXTRA);
		ptr += strlen( XML_TAG_EXTRA );
		return ptr;
	}
        else if( strncmp( XML_TAG_VLEN, str, strlen( XML_TAG_VLEN )) == 0 ) {
                strcpy(cur->class_type, XML_TAG_VLEN);
		ptr += strlen( XML_TAG_VLEN );
		return ptr;
	}
        else if( strncmp( XML_TAG_EXTERNAL, str, strlen( XML_TAG_EXTERNAL )) == 0 ) {
                strcpy(cur->class_type, XML_TAG_EXTERNAL);
		ptr += strlen( XML_TAG_EXTERNAL );
		return ptr;
	}
        
	return NULL;
}

/****************************************************************************
 * Get value
 ***************************************************************************/
static char*
get_value_end( char* str ) {

	if( *str != '=' )
		return NULL;
	str++;

	if( *str != '"' )
		return NULL;
	
	for( str++ ;
	     str && *str != '\0' && *str != '>' && *str != '"' ; str++  );
	
	if( *str != '"' )
		return NULL;
	
	return str;
}

/****************************************************************************
 * Create an entry for variable map
 ***************************************************************************/
static int
create_var_entry( var_t *cur, char *str ) {

	char *end = NULL;
	bzero( cur, VMAP_ENTRY_SZ );

	if( !( str = validate_tag_name( cur, str ) ))
		return 0;

	while( 1 ) {
		if( !( str = remove_ws( str )))
			return 0;

		if( *str == '/' )
			break;
		
		if( strncmp( NAME_TAG, str, strlen( NAME_TAG )) == 0 ){
			str += strlen( NAME_TAG ); 
			if( !( end = get_value_end( str )))
				return 0;
			
			str += 2;
			*end = '\0';
			strncpy( cur->name, str, STR_LEN - 1 );
		}
		
		else if( strncmp( TYPE_TAG, str, strlen( TYPE_TAG )) == 0 ){
			str += strlen( TYPE_TAG );
			if( !( end = get_value_end( str )))
				return 0;

			str += 2;
			*end = '\0';
			strncpy( cur->type, str, STR_LEN - 1 );
		}
		
		else if( strncmp( DEFVAL_TAG, str, strlen( DEFVAL_TAG )) == 0){
			str += strlen( DEFVAL_TAG );
			if( !( end = get_value_end( str )))
				return 0;

			str += 2;
			*end = '\0';
			cur->def_val = atoi(str);
		}

		else if( strncmp( UNIT_TAG, str, strlen( UNIT_TAG )) == 0 ){
			str += strlen( UNIT_TAG );
			if( !( end = get_value_end( str )))
				return 0;

			str += 2;
			*end = '\0';
			strncpy( cur->unit, str, STR_LEN - 1 );
		}
		
		else if( strncmp( SIZE_TAG, str, strlen( SIZE_TAG )) == 0 ){
			str += strlen( SIZE_TAG );
			if( !(end = get_value_end( str )))
				return 0;
			str += 2;
			*end = '\0';
			cur->size = atoi( str );
		}
		
		else {
			debug_r( "Error: tag name (%s) not valid\n", str );
			return 0;
		}
		str = end + 1;
	}

	if( ( strlen( cur->name ) == 0 ) || ( strlen( cur->type ) == 0 ))
		return 0;
	return 1;
}

/****************************************************************************
 * Interface functions
 ***************************************************************************/

/****************************************************************************
 * Given a string holding the description of the information provided by
 * the kernel, returns a sorted mapping of variable names to their
 * description and offset in the structure
 ***************************************************************************/


int nameInList(char *name, char **list) {
     int i = 0;
     while(list[i] != NULL) {
	  if(strcmp(list[i], name) == 0)
	       return 1;
	  i++;
     }
     return 0;
}

variable_map_t*
_create_info_mapping( char* desc, char **itemList ) {

	variable_map_t *mapping = NULL;
	char *str  = NULL, *ptr  = NULL, *end = NULL;
	char *root = NULL;
	
	int cur_entries = 0, cur_offset = 0, i = 0;
        int included_entries = 0;
        int includeCurr = 0;

	if( !desc ) {
		debug_r( "Error: args, create mapping\n" );
		return NULL;
	}

	if( !( str = malloc( strlen( desc ) + 1 ))) {
		debug_r( "Error: malloc, mapping\n" );
		return NULL;
	}

	memcpy( str, desc, strlen( desc ));
	
	/* Get the xml root element */
	if( !( ptr = get_xml_root( str ) ) )
		goto failed;

	/* get the root tag */
	if( !( end = get_endof_line( ptr )))
		goto failed;

	if( !( root = get_root_tag( ptr ))) 
		goto failed;

	/* Allocate the data structures */ 
	if( !( mapping = (variable_map_t*) malloc ( VMAP_SZ ))) 
		goto failed;

	mapping->num = INC_SIZE;
        mapping->sorted_vars = NULL;
	if( !( mapping->vars = (var_t*) malloc( mapping->num*VMAP_ENTRY_SZ)))
		goto failed;
        
        mlog_bn_dy("inforeader", "In create_info_mapping\n");

	/* Parse the variable description */ 
	for( ptr = end + 1 ;  ; ptr = end + 1 ) {

		var_t  cur;
		
		if( ( ptr = remove_ws( ptr )) == NULL )
			break;

		if( *ptr == '<' )
			ptr++;
		else
			goto failed;
		
		if( !( end = get_endof_line( ptr )) || ( *end == '\0'))
			break;
		
		*end = '\0';
		if( strlen( ptr ) == 0 )
			continue;

		bzero( &cur, VMAP_ENTRY_SZ );

		if( create_var_entry( &cur, ptr ) == 0 ) {
			if( test_closing_tag( ptr, root ))
				break;
			else
				goto failed;
		}

		// Skeeping a variable if it is not in the list 
                includeCurr = 1;
		if(itemList && !nameInList(cur.name, itemList)) {
                  includeCurr = 0;
		}

		if( cur_entries == mapping->num ) {
			mapping->num += INC_SIZE ;
			if( !( mapping->vars = realloc( mapping->vars,
							mapping->num *
							VMAP_ENTRY_SZ ))) {
				debug_r( "Error: malloc failed\n" );
				goto failed;
			}
		}
                
		cur.offset = cur_offset;
		if( cur.size == 0 ) {
			if( ( strcmp( cur.type, "int" )) == 0)
				cur.size = sizeof(int);
			else if( ( strcmp( cur.type, "unsigned char" )) == 0 )
				cur.size = sizeof(unsigned char);
			else if( ( strcmp( cur.type, "unsigned short" )) == 0 )
				cur.size = sizeof(unsigned short);
			else if( ( strcmp( cur.type, "unsigned long" )) == 0) 
				cur.size = sizeof(unsigned long);
			else if( ( strcmp( cur.type, "unsigned int" )) == 0)
				cur.size = sizeof(unsigned int);

                        // A string is currently allowed only in the external
                        // tag which should be at the end (for now)
                        else if( ( strcmp( cur.type, "string" )) == 0) {
                                if(strcmp( cur.class_type, XML_TAG_EXTERNAL) == 0 ||
                                   strcmp( cur.class_type, XML_TAG_VLEN) == 0)
                                {
                                        cur.size = 0;
                                }
                        }
                        else
				goto failed;
		}
                if(includeCurr) {
                  mapping->vars[ included_entries++ ] = cur;
                }
		cur_offset += cur.size;
		cur_entries++;
	}
	
	mapping->num = included_entries;
	mapping->entry_sz = cur_offset;

	if( !( mapping->sorted_vars =
	       malloc( mapping->num * VMAP_ENTRY_SZ ))) {
		debug_r( "Error: malloc failed\n" );
		goto failed;
	}

	for( i = 0; i < mapping->num ; i++ ) {
		mapping->sorted_vars[i] = mapping->vars[i]; 
                mlog_bn_dy("inforeader", "%25s %4d %4d\n", 
                           mapping->vars[i].name, mapping->vars[i].offset, mapping->vars[i].size);

        }
	qsort( mapping->sorted_vars, mapping->num, VMAP_ENTRY_SZ, var_cmp );

        

	if( str )
		free( str );
	if( root )
		free( root );
	
	return mapping ;
    	
 failed:
	if( str )
		free( str );
	if( root )
		free( root );
	
	destroy_info_mapping( mapping );
    	return NULL;
}

variable_map_t*
create_info_mapping( char* desc ) {
	return _create_info_mapping(desc, NULL);
}

variable_map_t*
create_selected_info_mapping( char* desc, char **itemList ) {
	return _create_info_mapping(desc, itemList);
}


/****************************************************************************
 * frees memory allocated by create_info_mapping()
 ***************************************************************************/
void
destroy_info_mapping( variable_map_t* mapping ) {
	if( mapping ){
		if( mapping->vars )
			free( mapping->vars );
		if( mapping->sorted_vars )
			free( mapping->sorted_vars );

		free( mapping );
	}
}

/****************************************************************************
 * Search the varibale. Return the structure describing the variable
 * in case that the variable was found, else return NULL
 ***************************************************************************/
var_t*
get_var_desc( variable_map_t *mapping, char *key ) {
        var_t *v;
	if( !mapping || !mapping->vars || !key ){
		debug_r( "Error: args in get_var_desc\n" );
		return NULL;
	}

        
        //printf("SIZE SIZE %d num %d\n", VMAP_ENTRY_SZ, mapping->num);

        var_t keyVar;
        strcpy(keyVar.name, key);
	v =(var_t*)bsearch( &keyVar, mapping->sorted_vars, mapping->num,
                            VMAP_ENTRY_SZ, var_cmp );
        return v;
}

/****************************************************************************
 * Printing functions
 ***************************************************************************/
void
var_print( var_t *var ) {
	debug_b( "Name=%-16sC=%-5sType=%-15sDef=%d\tUnit=%-12sOff=%d\tSz=%d\n",
		 var->name, var->class_type, var->type, var->def_val,
                 var->unit, var->offset, var->size );
}

void
variable_map_print( variable_map_t *mapping ){

	int i = 0;
	
	if( !mapping )
		debug_r( "Error: args variable_map_print\n" );
	
	debug_g( "===============  N(%d) S(%d) ================\n",
		 mapping->num, mapping->entry_sz );
    
	for( i = 0 ; i < mapping->num ; i++ )
		var_print( &(mapping->sorted_vars[i]) );
}

int is_var_vlen(var_t *v)
{
        if(!v)
                return 0;
        if(strcmp(v->class_type, XML_TAG_VLEN) == 0)
                return 1;
        return 0;
}


/****************************************************************************
 * Variable Length Index management
 ***************************************************************************/
// The structure is
// int    number-of-items
// short  item-len, name-len
// char   item-name
// char   item-data


int setVlenInfo(char *ptr, char *data_name, short nameLen, char *data, short size)
{
        char   *infoDataPtr;
        char   *infoNamePtr;
        short  *infoLenPtr, *infoNameLenPtr;

        infoLenPtr     = (short *) ptr;
        infoNameLenPtr = infoLenPtr + 1; //pointer arithmetic with shorts
        infoNamePtr    = ptr + 2*sizeof(short);
        infoDataPtr    = infoNamePtr + nameLen;
        
        *infoLenPtr = size;
        *infoNameLenPtr = nameLen;
        strcpy(infoNamePtr, data_name);
        memcpy(infoDataPtr, data, size);

        return size + nameLen + 2*sizeof(short);
}
// Advance to next vlen item
char *nextVlenInfo(char *ptr)
{
        short  *infoLenPtr, *infoNameLenPtr;
        // Adding to an existing vlen info

        if(!ptr)
                return NULL;
        
        infoLenPtr     = (short *)ptr;
        infoNameLenPtr = (short *)(ptr + sizeof(short));

        // Some problem with the data negative or zero length
        if(*infoLenPtr <= 0 || *infoNameLenPtr <= 0)
                return NULL;
        
        return ptr + 2*sizeof(short) + *infoLenPtr + *infoNameLenPtr;
}

inline int getVlenDataSize(char *ptr) {
        return *((short *) ptr);
}

inline char *getVlenName(char *ptr) {
        if(!ptr)
                return NULL;
        return ptr + 2*sizeof(short);
}

inline char *getVlenData(char *ptr) {
        short *nameLenPtr;
        if(!ptr)
                return NULL;

        nameLenPtr = (short *)(ptr + sizeof(short));
        // Not allowing negative(zero) length
        if(*nameLenPtr <=0)
                return NULL;
        return ptr + 2*sizeof(short) + *nameLenPtr;
}

void printVlenInfo(FILE *fh, char *ptr)
{
        char   *infoNamePtr;
        short  *infoLenPtr, *infoNameLenPtr;
        
        infoLenPtr     = (short *) ptr;
        infoNameLenPtr = infoLenPtr + 1; //pointer arithmetic with shorts
        infoNamePtr    = ptr + 2*sizeof(short);
        
        fprintf(fh, "%s (len %d) data-len: %d\n",
                infoNamePtr, *infoNameLenPtr, *infoLenPtr);
        
}

int  add_vlen_info(node_info_t *ninfo, char *data_name, char *data, int size)
{
        int    *vlenItemsPtr;
        char   *ptr;
        int     nameLen = strlen(data_name) +1;
        int     vlenSize;
        
        if(!ninfo || !data_name || !data || size <= 0)
                return 0;

        vlenItemsPtr =(int*)( (char *)ninfo + ninfo->hdr.psize);

        ptr = (char*)vlenItemsPtr + sizeof(int);
        // There is vlen info items already
        if(IS_VLEN_INFO(ninfo)) {
                int i;
                for(i=0; i< *vlenItemsPtr ; i++) 
                        ptr = nextVlenInfo(ptr);
                
                vlenSize = setVlenInfo(ptr, data_name, nameLen, data, size);
                (*vlenItemsPtr) ++;
        }
        // First time addition
        else {
                *vlenItemsPtr = 1;
                vlenSize = setVlenInfo((char *)(vlenItemsPtr+1), data_name, nameLen, data, size);
                vlenSize += sizeof(int);
                
        }
        ninfo->hdr.fsize += vlenSize; 
        return 1;
}

void  *get_vlen_info(node_info_t *ninfo, char *info_name, int *size) {
        int    *vlenItemsPtr;
        char   *ptr;
        
        if(!ninfo || !info_name)
                return 0;

        // There is no vlen info
        if(!IS_VLEN_INFO(ninfo)) 
                return NULL;

        vlenItemsPtr =(int*)( (char *)ninfo + ninfo->hdr.psize);
        // VlenItem number is wrong
        if(*vlenItemsPtr <=0)
                return NULL;
        
        int i;
        char *currName;
        
        ptr = (char*)vlenItemsPtr + sizeof(int);
        currName = getVlenName(ptr);
        i = 0;
        while((i < *vlenItemsPtr) && strcmp(info_name, currName) != 0) {
          
                ptr = nextVlenInfo(ptr);
                currName = getVlenName(ptr);
                i++;
        }
        // Could not find the requested item
        if(i == *vlenItemsPtr)
                return NULL;
        // The requested item found
        *size = getVlenDataSize(ptr);
        return getVlenData(ptr);
}

// Printing the vlen info (sizes + name) for debugging
void print_vlen_items(FILE *fh, node_info_t *ninfo)
{
        int    *vlenItemsPtr;
        char   *ptr;
        int     i;

        if(!ninfo) {
                fprintf(fh, "Error: Ninfo is NULL\n");
                return;
        }

        // There is no vlen info
        if(!IS_VLEN_INFO(ninfo)) {
                fprintf(fh, "Error: There is no vlen info\n");
                return;
        }

        vlenItemsPtr =(int*)( (char *)ninfo + ninfo->hdr.psize);
        // VlenItem number is wrong
        if(*vlenItemsPtr <=0) {
                fprintf(fh, "Error:Vlen items number <=0 %d\n", *vlenItemsPtr);
                return;
        }
        fprintf(stderr, "Vlen info: %d\n", *vlenItemsPtr);
        ptr = (char*)vlenItemsPtr + sizeof(int);
        for(i = 0; i < *vlenItemsPtr ; i++) {
                printVlenInfo(fh,ptr);
                ptr = nextVlenInfo(ptr);
        }
}


