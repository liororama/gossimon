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
 *    File: infoxml.h
 *****************************************************************************/

#ifndef _INFOXML_H
#define _INFOXML_H

#include <info.h>
#include <infolib.h>
#include <info_reader.h>

#define  XML_ROOT_ELEMENT         "cluster_info"
#define  XML_ERROR_TAG            "error"
#define  XML_NODE_ELEMENT         "node"
#define  XML_AGE_ATTR             "age"
#define  XML_NAME_ELEMENT         "name"
#define  XML_STATS_ELEMENT        "stats"

#define  XML_CONT_ELEMENT         "cont"
#define  XML_SUBST_ELEMENT        "subst"
#define  XML_NAMES_ELEMENT        "names"
#define  XML_AGE_ELEMENT          "age"
#define  XML_ARG_ELEMENT          "arg"
#define  XML_NUM_ELEMENT          "total_num"
#define  XML_ALIVE_ELEMENT        "alive"
#define  XML_TAG_NCPUS            "ncpus"
#define  XML_TAG_SSPEED           "sspeed"
#define  XML_TAG_TMEM             "tmem"
#define  XML_TAG_AVGLOAD          "avgload"
#define  XML_TAG_AVGAGE           "avgage"
#define  XML_TAG_MAXAGE           "maxage"


#define XML_INFO_ITEM_IDENT_STR  "\t\t"
/****************************************************************************
 * Interface functions, that retrieve the information in xml format
 ***************************************************************************/

/*
 * Creates a string that holds the information in xml representation
 * from the given data
 */                                  
char* infoxml_create( idata_t *data, variable_map_t *vmap );
char* infoxml_create_stats( infod_stats_t *stats );

/* get the load information of all of the machines from server */ 
char* infoxml_all( char *server, unsigned short portnum, char **itemList ) ;

/* get the load information of all of the machines from server */ 
char* infoxml_window( char *server, unsigned short portnum ) ;

/* get the load information of a continues subset of machines */
char* infoxml_cont_pes( char *server,  unsigned short portnum,
			cont_pes_args_t *args);

/* get the load information of a subset of machines */ 
char* infoxml_subset_pes( char *server,   unsigned short portnum,
			  subst_pes_args_t *args );

/* get the loads of the machines according to their names */
char* infoxml_subset_name( char *server,   unsigned short portnum,
			   char *args );

/* get the loads of machines upto a given age */
char* infoxml_up2age( char *server, unsigned short portnum,
		      unsigned long age ); 

/* get all the statistics about the vector */
char* infoxml_stats( char* server, unsigned short portnum );

#endif

/****************************************************************************
 *                              E O F 
 ***************************************************************************/
