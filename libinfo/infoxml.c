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
 *    File: infoxml.c
 *****************************************************************************/

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <msx_error.h>

#include <msx_debug.h>
#include <msx_error.h>

#include <infolib_internal.h>

#include <infolib.h>
#include <infoxml.h>
#include <info_iter.h>
#include <info_reader.h>

#define  BASIC_SZ             (4096)

static char           *glob_desc    = NULL;
static int            glob_desc_len = 0;
static variable_map_t *glob_vmap    = NULL;

/****************************************************************************
 * Create the variale map from the description 
 ***************************************************************************/
variable_map_t*
infoxml_get_vmap( char *server, unsigned short portnum, char **itemList ) {
     
     /* get the information and create the mapping */
     if( glob_desc == NULL ) {
	  if( !( glob_desc = infolib_info_description(server,portnum))) {
	       debug_r( "Error: retrieving desc\n" );
	       return NULL;
	  }
     }
     glob_desc_len = strlen( glob_desc );
     
     if(itemList) {
	  if( !( glob_vmap = create_selected_info_mapping( glob_desc, itemList ) )) {
	       debug_r( "Error: creating mapping\n" );
	       return NULL;
	  }
     }
     else {
	  if( glob_vmap == NULL ) {
	       if( !( glob_vmap = create_info_mapping( glob_desc ) )) {
		    debug_r( "Error: creating mapping\n" );
		    return NULL;
	       }
	  }
     }
     variable_map_print(glob_vmap);
     return glob_vmap;
}

/****************************************************************************
 * Vriable length items which are also string are printed here
 ***************************************************************************/
void print_vlen_string(char *str, var_t *var, node_info_t *node)
{
        int   size;
        char *data;

	int isXML = strcmp(var->unit, "xml") == 0;

        //print_vlen_items(stderr, node);
        data = get_vlen_info(node, var->name, &size);
        //fprintf(stderr, "Printing a vlen item: %s\n", data);
	
        if(!IS_VLEN_INFO(node)) {
                if(isXML) {
                        sprintf(str, "%s<%s>0</%s>\n",
				XML_INFO_ITEM_IDENT_STR, var->name, var->name);
		}
                else
                        sprintf(str, "0");
                return;
        }


        if(data) {
                if(isXML) {
                        sprintf(str, "%s%s\n", XML_INFO_ITEM_IDENT_STR, data);
		}
                else
                        sprintf(str, "%s", data);
        } else {
                if(isXML) {
                        sprintf(str, "%s<%s>error</%s>\n",
				XML_INFO_ITEM_IDENT_STR, var->name, var->name);
		}
                else
                        sprintf(str, "error");
        }
}

/****************************************************************************
 * Add the representation of a single node
 ***************************************************************************/
static int
infoxml_create_node( variable_map_t *map, idata_entry_t *cur, char *res ) {

	int i = 0;
	char tmp[BASIC_SZ];
	node_info_t *node = NULL;
	int status = 0;
	double age = 0.0;
	
	if( cur->valid == 0 )
		return 0;

	node = cur->data;
	status = node->hdr.status & INFOD_ALIVE;
	age = ((double)(node->hdr.time.tv_sec)*(double)(MILLI)+
	       (double)(node->hdr.time.tv_usec))/(double)(MILLI);
	/* add the openeing tag and the name */
	sprintf( tmp, "\t<%s %s=\"%.2f\">\n%s<%s>%s</%s>\n",
		 XML_NODE_ELEMENT, XML_AGE_ATTR, age,
		 XML_INFO_ITEM_IDENT_STR,
		 XML_NAME_ELEMENT, cur->name, XML_NAME_ELEMENT );

	strcat( res, tmp );
	// Adding the pe stuff which is only in the header
	sprintf(tmp, "%s<pe>%d</pe>\n", XML_INFO_ITEM_IDENT_STR, node->hdr.pe);
	strcat( res, tmp );
	
	// Adding the infod status stuff
	sprintf(tmp, "%s<infod_status>%d</infod_status>\n",
		XML_INFO_ITEM_IDENT_STR, status);
	strcat( res, tmp );
	sprintf(tmp, "%s<infod_dead_str>%s</infod_dead_str>\n",
		XML_INFO_ITEM_IDENT_STR, infoStatusToStr(node->hdr.status));
	strcat( res, tmp );
	
        // Adding the external_status
        sprintf(tmp, "%s<external_status>%s</external_status>\n",
		XML_INFO_ITEM_IDENT_STR,
		infoExternalStatusToStr(node->hdr.external_status));
	strcat( res, tmp );
	
        
	/* add the information */ 
	for( i = 0; i < map->num ; i++ ) {
		
		char   tmp2[BASIC_SZ];
		var_t  *var = &(map->vars[i]);
		
                // Printing the name of the item unless it is of unit xml and
                // type string
                tmp[0] = '\0';
                tmp2[0] = '\0';
                if(strcmp(var->type, "string") != 0 ||
                   strcmp(var->unit, "xml") != 0 )
                {
                        if( strlen( var->unit ) != 0 )
                                sprintf( tmp, "%s<%s unit=\"%s\">",
					 XML_INFO_ITEM_IDENT_STR,
					 var->name, var->unit );
                        else
                                sprintf( tmp, "%s<%s>",
					 XML_INFO_ITEM_IDENT_STR, var->name );
                }
                
		/* if( !( strcmp( var->name, "status" ))) { */
/* 			sprintf( tmp2, "%d", node->hdr.status ); */
/* 		} */


                // Handling each type of supported variable
                if( strcmp( var->type, "string") == 0 &&
                    is_var_vlen( var ))
                {
                        print_vlen_string(tmp2, var, node);
		}
		else if( !( strcmp( var->type, "int"))) {
			int val = status?
				*((int*)((void*)(node->data)+var->offset)):0;
			sprintf( tmp2, "%d",val );
		}
		
		else if( !( strcmp( map->vars[i].type, "unsigned short"))) {
			unsigned short val = status?
				*((unsigned short*)
				  ( (void*)(node->data)+var->offset )) : 0;
			sprintf( tmp2, "%d", val );
		}
		else if( !( strcmp( map->vars[i].type, "unsigned char"))) {
			unsigned char val = status?
				*((unsigned char*)
				  ( (void*)(node->data)+var->offset )) : 0;
			sprintf( tmp2, "%u", val );
		}
		
		
		else if( !( strcmp( map->vars[i].type, "unsigned int" ))) {
			unsigned int val = status?
				*((unsigned int*)
				  ( (void*)(node->data)+var->offset )) : 0;
			sprintf( tmp2, "%u", val );
		}
		
		else if( !( strcmp( map->vars[i].type, "struct sockaddr" ))) {
			struct sockaddr_in addr = 
				*((struct sockaddr_in*)
				  ( (void*)(node->data)+var->offset ));
			sprintf( tmp2, "%s", inet_ntoa(addr.sin_addr));
		}
		
		else {
			unsigned long val = status?
				*((unsigned long*)
				  ( (void*)(node->data)+var->offset )) : 0;
			sprintf( tmp2, "%lu", val);
		}
		
		strcat( tmp, tmp2 );
                if(strcmp(var->type, "string") != 0 ||
                   strcmp(var->unit, "xml") !=0)
                {
                        sprintf(tmp2, "</%s>\n", var->name );
                        strcat(tmp, tmp2 );
                }
		strcat( res, tmp);
	}

	/* Add the closing tag */ 
	sprintf( tmp, "\t</%s>\n", XML_NODE_ELEMENT );
	strcat( res, tmp );
	return strlen( res );
}

/****************************************************************************
 * Creates a string that holds the information in xml representation
 ***************************************************************************/
char*
infoxml_create( idata_t *data, variable_map_t *vmap ) {

	idata_iter_t  *iter = NULL;
	idata_entry_t *cur  = NULL;
	char          *res  = NULL;
	int           res_sz = 0;
	char          tmp[BASIC_SZ];

	/*  create the iterator */ 
	if( !( iter = idata_iter_init( data ))) {
		debug_r( "Error: creating iterator\n" );
		goto failed;
	}

	res_sz = (data->num * glob_desc_len) + BASIC_SZ;
	if( !( res = malloc( res_sz ))) {
		debug_r( "Error: malloc\n" );
		goto failed;
	}
	bzero( res, res_sz );
	
	/* add the xml header and the root element */
	strcpy( res, XML_ROOT_TAG );
	sprintf( tmp, "<%s size=\"%d\" unit=\"%d\">\n",
		 XML_ROOT_ELEMENT, data->num, MEM_UNIT_SIZE );
	strcat( res, tmp );

	while(( cur = idata_iter_next( iter ))) {

		if( ( infoxml_create_node( vmap, cur, res )) == -1 ) {
			free( res );
			res = NULL;
			goto failed;
		}
	}

	/* Add the cluster info tag */ 
	sprintf( tmp, "</%s>\n", XML_ROOT_ELEMENT);
        strcat( res, tmp );

	if( iter )
		idata_iter_done( iter );
	
	return res;
	
 failed:
	if( glob_desc ) {
		free( glob_desc );
		glob_desc = NULL;
	}
	
 	if( vmap ) {
	 	destroy_info_mapping( vmap );
		vmap = NULL;
	}
	
	if( iter )
		idata_iter_done( iter );
	
	return res;
}


/****************************************************************************
 * Create an XML representation to the infod stats
 ***************************************************************************/
char*
infoxml_create_stats( infod_stats_t *stats ) {

	int res_sz = BASIC_SZ;
	char *res = NULL;
	
	if( !( res = malloc( res_sz ))) {
		debug_r( "Error: malloc\n" );
		return NULL;
	}
	bzero( res, res_sz );
	
	/* add the xml header and the root element */
	sprintf( res, "%s<%s>"
		 "\t<%s>%d</%s>\n"
		 "\t<%s>%d</%s>\n"
		 "\t<%s>%d</%s>\n"
		 "\t<%s>%d</%s>\n"
		 "\t<%s unit=\"%d\">%d</%s>\n"
		 "\t<%s>%.2f</%s>\n"
		 "\t<%s>%.2f</%s>\n"
		 "\t<%s>%.2f</%s>\n"
		 "</%s>\n", 
 		 XML_ROOT_TAG,
		 XML_STATS_ELEMENT,
		 XML_NUM_ELEMENT, stats->total_num, XML_NUM_ELEMENT,
		 XML_ALIVE_ELEMENT, stats->num_alive, XML_ALIVE_ELEMENT,
		 XML_TAG_NCPUS,stats->ncpus, XML_TAG_NCPUS,
		 XML_TAG_SSPEED,stats->sspeed, XML_TAG_SSPEED,
		 XML_TAG_TMEM, MEM_UNIT_SIZE, stats->tmem, XML_TAG_TMEM,
		 XML_TAG_AVGLOAD,stats->avgload, XML_TAG_AVGLOAD,
		 XML_TAG_AVGAGE,stats->avgage, XML_TAG_AVGAGE,
		 XML_TAG_MAXAGE, stats->maxage, XML_TAG_MAXAGE,
		 XML_STATS_ELEMENT );

	return res;
}

/****************************************************************************
 * Get infrmation about all the nodes in xml format
 ***************************************************************************/
char*
infoxml_all( char *server, unsigned short portnum, char **itemList ) {

	idata_t        *data = NULL;
	char           *res  = NULL;
	variable_map_t *vmap = NULL;
	
	if( !( data = infolib_all( server, portnum ))) {
		debug_r( "Error: retrieving data\n" );
		goto failed;
	}

	if(itemList) {
	     // Creating xml representation of the item list
	     if( !( vmap = infoxml_get_vmap( server, portnum, itemList )))
		  goto failed;
	}
	else if( !( vmap = infoxml_get_vmap( server, portnum, NULL ))) {
	     /* create the xml representation of the data */
	     goto failed;
	}
	res = infoxml_create( data, vmap );
	
 failed:
 	if( data )
	 	free( data );
	
	return res;
}


char*
infoxml_window( char *server, unsigned short portnum ) {

	idata_t        *data = NULL;
	char           *res  = NULL;
	variable_map_t *vmap = NULL;
	
	if( !( data = infolib_window( server, portnum ))) {
		debug_r( "Error: retrieving data\n" );
		goto failed;
	}

	
	/* create the xml representation of the data */
	if( !( vmap = infoxml_get_vmap( server, portnum, NULL )))
		goto failed;
        
	res = infoxml_create( data, vmap );

 failed:
 	if( data )
	 	free( data );
	
	return res;
}

/****************************************************************************
 * Return an xml string of the given pes 
 ***************************************************************************/
char*
infoxml_cont_pes( char *server, unsigned short portnum,
		  cont_pes_args_t *args ) {
	
	idata_t        *data = NULL;
	char           *res  = NULL;
	variable_map_t *vmap = NULL;
	
	if( !( data = infolib_cont_pes( server, portnum, args ))) {
		debug_r( "Error: retrieving data\n" );
		goto failed;
	}

	/* create the xml representation of the data */
	if( !( vmap = infoxml_get_vmap( server, portnum, NULL )))
		goto failed;
	
	res = infoxml_create( data, vmap );
	
 failed:
 	if( data )
	 	free( data );
	return res;
}

/****************************************************************************
 * Return the information about a subset of pes in xml format
 ***************************************************************************/
char*
infoxml_subset_pes( char *server, unsigned short portnum,
		    subst_pes_args_t *args ) {

	idata_t        *data = NULL;
	char           *res  = NULL;
	variable_map_t *vmap = NULL;
	
	if( !( data = infolib_subset_pes( server, portnum, args ))) {
		debug_r( "Error: retrieving data\n" );
		goto failed;
	}

	/* create the xml representation of the data */
	if( !( vmap = infoxml_get_vmap( server, portnum, NULL )))
		goto failed;
	
	res = infoxml_create( data, vmap );

 failed:
 	if( data )
	 	free( data );
	return res;
}

/****************************************************************************
 * Retrieve a subset of machines according to their names
 ***************************************************************************/
char*
infoxml_subset_name( char *server, unsigned short portnum, char *args ) {

	idata_t        *data  = NULL;
	char           *res   = NULL;
	variable_map_t *vmap = NULL;

	if( !( data = infolib_subset_name( server, portnum, args ))) {
		debug_r( "Error: retrieving data\n" );
		goto failed;
	}
	
	/* create the xml representation of the data */
	if( !( vmap = infoxml_get_vmap( server, portnum, NULL )))
		goto failed;
	
	res = infoxml_create( data, vmap );

 failed:
 	if( data )
	 	free( data );

	return res;
}

/****************************************************************************
 * Create the XML representation for the data that holds only entries that
 * are not older than 'age'.
 ***************************************************************************/
char*
infoxml_up2age( char *server, unsigned short portnum, unsigned long age ) {

	idata_t        *data  = NULL;
	char           *res   = NULL;
	variable_map_t *vmap = NULL;
	
	if( !( data = infolib_up2age( server, portnum, age ))) {
		debug_r( "Error: retrieving data\n" );
		goto failed;
	}

	/* create the xml representation of the data */
	if( !( vmap = infoxml_get_vmap( server, portnum, NULL )))
		goto failed;
	
	res = infoxml_create( data, vmap );

 failed:
 	if( data )
	 	free( data );

	return res;
}

/****************************************************************************
 * Create the XML representation for the statistics
 ***************************************************************************/
char*
infoxml_stats( char* server, unsigned short portnum ) {

	infod_stats_t  stats;
	char           *res  = NULL;
	
	if( ( infolib_get_stats( server, portnum, &stats )) != 1 ) {
		debug_r( "Error: retrieving data\n" );
		goto failed;
	}

	/* create the xml representation of the data */
	res = infoxml_create_stats( &stats );

 failed:
	return res;
}

/****************************************************************************
 *                            E O F
 ***************************************************************************/
