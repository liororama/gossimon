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
 * Author(s): Amar Lior
 *
 *****************************************************************************/

/******************************************************************************
 *
 * File: A module for obtaining information about the current node.
 * This information can be retrieved from 
 *
 *****************************************************************************/

#ifndef _PROVIDER_INTERFACE_H
#define _PROVIDER_INTERFACE_H

#define MAX_KCOMM_SOCK_NAME  (256)
#define KCOMM_AUTH_CHAR      'm'

#include <sys/socket.h>
#include <infod.h>
#include <msx_common.h>
#include <info.h>
#include <infoVec.h>
/**************************************************************************
 General kcomm functions used to manage the communication channel with
 the provider. The provider is mainly an internal provider collecting 
 information from local host but it is also possible to create an external
 provider from a loadable module
**************************************************************************/

/* Initilizing the information provider module
   kcomm_sock_name == NULL means do not use a provider but get data
   by yourself
*/
int kcomm_init( char *kcomm_module,
		char *kcomm_sock_name, int max_send,
	        int module_argc, char **module_argv);

int kcomm_init_as_client( char *kcomm_sock_name, int max_send,
			  char *specific_kcomm_module);

int kcomm_set_vector(void *vector);

// Stopping ctl subsystem
int kcomm_stop();
int kcomm_reset();

// Depends on our status insert the main socket or the current ctl socket.
int kcomm_get_fdset(fd_set *rfds, fd_set *wfds, fd_set *efds);
// Returens the maximum sock number
int kcomm_get_max_sock(int curr_max);
int kcomm_is_new_connection(fd_set *rfds);
int kcomm_setup_connection();
int kcomm_is_req_arrived(fd_set *rfds);
int kcomm_is_ready_to_send(fd_set *wfds);
int kcomm_exception(fd_set *efds);

// read the message from the socket and parese it. Returning the action that
// should be performed with its data. Should be called after an accept 
int kcomm_recv(int *magic, void *user_buff, int *user_len);

// Checking if the channel was alredy authenticated (relate only to server )
int kcomm_is_auth();
int kcomm_is_up();

// Receiving auth message
int kcomm_recv_auth();

// send a generic message on the channel
int kcomm_send(void *user_buff, int user_len);

// send a speed change message (should be generic to all kernels)
int kcomm_send_speed_change(unsigned long new_speed);

// send a cost changs message (should be generic to all kernels, those
// who doesnot support cost will simply ignore
int kcomm_send_cost_change(unsigned long ntopology, unsigned long *costs);

// Compleating a partial send
int kcomm_finish_send();

/***************************************************************************
 * Provider specific functions. The following functions call the module
 * supplied functions.
 ***************************************************************************/
// Init function for the module
int kcomm_module_init();

// Setting the kcomm pe (if it need so)
void kcomm_set_pe( unsigned long pe );

// Setting the map change handler
typedef void   (*map_change_hndl_t) (void);
void kcomm_set_map_change_hndl(map_change_hndl_t hndl);

// Printing the received message for debugging
void kcomm_print_msg( void *msg, int len );

// Handling of received message (and responding to it if necessary)
int kcomm_handle_msg( unsigned long magic, char *req_buff, int req_len,
		      ivec_t vec );

// Periodic admin of kernel frequency request
int kcomm_periodic_admin( ivec_t vec );

//  get the statistics from the infod vector
int kcomm_get_stats( ivec_t vec, infod_stats_t *stats );

// Getting the info description
char  *kcomm_get_infodesc();
// Getting info size
int kcomm_get_infosize();

// Getting local info from kcomm
int kcomm_get_info(void *info_buff, int size); 


/****************************************************************************
 * Some utility functions and structures for providers. Such as finding the
 * Disk io done and the network usage
 ***************************************************************************/
#include <providerUtil.h>

/************************************************************************
 Module functions. An information provider module should supply those
Functions.
*************************************************************************/
#define KCOMM_SF_INIT_FUNC             "kcomm_sf_init"
#define KCOMM_SF_RESET_FUNC            "kcomm_sf_reset"
#define KCOMM_SF_PRINT_MSG_FUNC        "kcomm_sf_print_msg"
#define KCOMM_SF_PARSE_MSG_FUNC        "kcomm_sf_parse_msg"
#define KCOMM_SF_HANDLE_MSG_FUNC       "kcomm_sf_handle_msg"
#define KCOMM_SF_PERIODIC_ADMIN_FUNC   "kcomm_sf_periodic_admin"
#define KCOMM_SF_GET_STATS_FUNC        "kcomm_sf_get_stats"
#define KCOMM_SF_GET_INFODESC_FUNC     "kcomm_sf_get_infodesc"
#define KCOMM_SF_GET_INFOSIZE_FUNC     "kcomm_sf_get_infosize"
#define KCOMM_SF_GET_INFO_FUNC         "kcomm_sf_get_info"
#define KCOMM_SF_SET_PE_FUNC           "kcomm_sf_set_pe"
#define KCOMM_SF_SET_MAP_CHANGE_HNDL_FUNC "kcomm_sf_set_map_change_hndl"

/****************************************************************************
 * The protototypes of the above functions
 ***************************************************************************/

/* Init function for the module */
typedef int    (*kcomm_init_module_func_t)    (int, int, char **);

/* Reset function for the module */
typedef int    (*kcomm_reset_module_func_t)   (void);

/* changing pe in kcomm specific module */
typedef void   (*kcomm_set_pe_func_t)         (unsigned long);

/* Setting the map change handler */
typedef void   (*kcomm_set_map_change_hndl_func_t) (map_change_hndl_t);

/* Printing a message received from the kernel: buff + len */
typedef void   (*kcomm_print_msg_func_t)      (void*, int);

/* Handling of a message: magic + buff + len */
typedef int    (*kcomm_handle_msg_func_t)     (unsigned long, char *, int,
					       ivec_t);

/* Priodic admin, should be called in the infod time handler */
typedef int    (*kcomm_periodic_admin_func_t) (ivec_t);

/* get the statistcs of the information */
typedef int    (*kcomm_get_stats_t)(ivec_t, infod_stats_t*);

/* Getting information description */
typedef char*  (*kcomm_get_infodesc_func_t)   (void);

/* Getting information size */
typedef int    (*kcomm_get_infosize_func_t)   (void);

/* Getting information */
typedef int    (*kcomm_get_info_func_t)       (void*, int);

/*
 * Parsing a message and telling if it is leagal or not and if not how much
 * is steel needed to be read and if a full messages is resceived then
 * how much extra bytes there are.
 */
typedef int    (*kcomm_parse_msg_func_t)      (void *, int, int *);

/****************************************************************************
 * The structure holding that aggregates all the above functions
 ***************************************************************************/

typedef struct _kcomm_module_func {
	kcomm_init_module_func_t    init;
	kcomm_reset_module_func_t   reset;
	kcomm_print_msg_func_t      print_msg;
	kcomm_parse_msg_func_t      parse_msg;
	kcomm_handle_msg_func_t     handle_msg;
	kcomm_periodic_admin_func_t periodic_admin;
	kcomm_get_stats_t           get_stats;
	kcomm_get_infodesc_func_t   get_infodesc;
	kcomm_get_infosize_func_t   get_infosize;
	kcomm_get_info_func_t       get_info;
	kcomm_set_pe_func_t         set_pe;
	kcomm_set_map_change_hndl_func_t set_map_change_hndl;  
} kcomm_module_func_t;

/* Default variables associated with the default modes of the infod */
extern kcomm_module_func_t mosix_provider_funcs;







#endif /* _PROVIDER_INTERFACE_H */
