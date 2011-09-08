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

#ifndef _MOSIX_INFO_VEC_INTERNAL
#define _MOSIX_INFO_VEC_INTERNAL


#define INITIAL_VEC_SIZE (32)
#define IPV              (4)
#define MSG_BUFF_SZ      (262144)
		

/****************************************************************************
 * Data structure 
 ***************************************************************************/

/****************************************************************************
 * The window object/structure the vector holds inside
 ***************************************************************************/
/* A single entry in the window */ 
typedef struct info_win_entry {
	
     int             index;   /* The index in the vector. -1 is an empty entry*/
     int             priority; // Priority of information   
	
} info_win_entry_t;

/* The function prototype which is used to calculate how many entries compose
   the window when this is requested. Since we can support Fixed window and
   Uptoage */
typedef int    (*calc_win_size_func_t)    (ivec_t vec);

/* The window */ 
typedef struct info_win {
     int               size;
     info_win_entry_t *data;
     int               type; // Fix or upto-age
	
     int              *aux;
     int               sendSize;   // The number of entries to send. 
     int               fixWinSize; // Size of fixed window
     int               uptoAge;    // Maximal age to send.
     calc_win_size_func_t calcWinSizeFunc;
} info_win_t;

/****************************************************************************
 * The information message, which is sent from infod to infod. The infoVec
 * creates this message buffer and knows how to unpack it and update the
 * vector according to the new window recieved
 ***************************************************************************/

/* An entry in the information msg */
typedef struct info_msg_entry {
	
     int   size;
     int   priority;
     node_info_t    data[0];
	
} info_msg_entry_t; 
  
/* Holds the window message */
typedef struct info_msg {
	
     unsigned long signature;
     unsigned int  num;
     unsigned int  tsize;
     info_msg_entry_t  data[0];
	
} info_msg_t ;

/****************************************************************************
 * Continuous mapping
 ***************************************************************************/

/* Describes a continuous part of the cluster (according to pe) */ 
typedef struct cont_vec_ips_ent {
     int               index;
     int               size;
     struct in_addr    ip;  //In host order
} cont_vec_ips_ent_t ;

typedef struct cont_vec {
     cont_vec_ips_ent_t  *data;     
     int                 size;
} cont_vec_ips_t;


/****************************************************************************
 * Vector
 ***************************************************************************/
struct ivec {

     //node_t              local_pe;
     int                 localIndex;
     struct in_addr      localIP;
     ivec_entry_t       *vec;
     int                 vsize;

     info_win_t          win;           /* the window              */
     cont_vec_ips_t      contIPs;       /* To speed up searches    */

     unsigned long       max_age;       /* maximal age of an entry */
     struct timeval      init_time;     /* intitiation time        */

     unsigned int        localPrio;     // Our one prio
     unsigned int        deadStartPrio; // Max priority (for dead nodes) */
	
     unsigned int        numAlive;
     struct in_addr      lastDeadIP;
     unsigned long       signature;
	
     void               *msg_buff;
     unsigned int        msg_buff_size;

     struct timeval      currTime;
     struct timeval      prev_time;

     int                 resolveHosts;

     ivec_age_measure_t       ageMeasure; 
     ivec_win_size_measure_t  winSizeMeasure;
     ivec_death_log_t         deathLog;
     ivec_entries_uptoage_measure_t entriesUptoageMeasure;
};

#define  INFO_WIN_ENTRY_SIZE     (sizeof(info_win_entry_t))
#define  INFO_WIN_SZ             (sizeof(info_win_t))
#define  CONT_IP_ENTRY_SIZE      (sizeof(cont_vec_ips_ent_t))
#define  INFO_VEC_SIZE           (sizeof(struct ivec))
#define  INFO_MSG_ENTRY_SIZE     (sizeof(info_msg_entry_t))
#define  INFO_MSG_SIZE           (sizeof(info_msg_t))


#endif

/****************************************************************************
 *                              E O F 
 ***************************************************************************/
