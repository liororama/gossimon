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
 *    File: infolib.h
 *****************************************************************************/

#ifndef _INFOLIB_H
#define _INFOLIB_H

#include <info.h>
#include <sys/time.h>
#include <msx_error.h>

/* The default time to wait for receiving data */
#define DEF_WAIT_SEC   (1)

/****************************************************************************
 *    Routines avaliable to clients
 ****************************************************************************/

/* get the load information of all of the machines from server */ 
idata_t* infolib_all( char *server, unsigned short portnum ) ;

/* get the load information of a continues subset of machines */
idata_t* infolib_cont_pes( char *server,  unsigned short portnum,
			   cont_pes_args_t *args );

/* get the load information of a subset of machines */ 
idata_t* infolib_subset_pes( char *server,   unsigned short portnum,
			     subst_pes_args_t *args );

/* get the loads of the machines according to their names */
idata_t* infolib_subset_name( char *server,   unsigned short portnum,
			      char *machines_names );

/* get the loads of machines upto a given age */
idata_t* infolib_up2age( char *server, unsigned short portnum,
			 unsigned long age ); 

/* get all the statistics about the vector */
int infolib_get_stats( char* server,   unsigned short portnum,
		       infod_stats_t *stats );

/* get the names of all the machines */
char* infolib_all_names( char* server, unsigned short portnum );

/* Returns XML file holding info description */
char* infolib_info_description( char *server, unsigned short portnum);

/* get the load information of all of the machines from server */ 
idata_t* infolib_window( char *server, unsigned short portnum );

#endif
				
/***************************************************************************
                                E O F
****************************************************************************/
