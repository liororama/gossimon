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
 * File: InfoVec.h Containes the information vector interface
 *****************************************************************************/

#ifndef _MOSIX_INFO_VEC
#define _MOSIX_INFO_VEC

#include <sys/time.h>
#include <info.h>
#include <Mapper.h>

/****************************************************************************
 * Data structures
 ***************************************************************************/

typedef struct ivec_entry {
	
     char           name[ MACHINE_NAME_SZ ];
     node_info_t    *info;
     unsigned int   isdead;
     unsigned int   reserved;
     
} ivec_entry_t;

typedef struct ivec *ivec_t;

#define      VENTRY_SZ     (sizeof(ivec_entry_t))

// Window types
#define INFOVEC_WIN_FIXED           (1)
#define INFOVEC_WIN_UPTOAGE         (2)

/****************************************************************************
 * Interface functions
 ***************************************************************************/

/* initiate the vector */ 
ivec_t infoVecInit( mapper_t map, 
                    unsigned long maxage,
                    int winType, 
                    int winTypeParam, 
                    char* description, 
                    int resolvHosts
                    );

/* free the ivec_t data structure */
void infoVecFree( ivec_t vec );

/* Find an entry in the vector */
ivec_entry_t* infoVecFindByPe( ivec_t vec, node_t pe, int *index );
ivec_entry_t* infoVecFindByIP( ivec_t vec, struct in_addr *ip, int *index);

/* update the vector */
int infoVecUpdate( ivec_t vec, node_info_t* update, int size,
		 unsigned int priority );

/* punish a node - that did not accept a connection */
int infoVecPunish( ivec_t vec, struct in_addr *ip, unsigned int cause ) ;

char *
infoVecPackQueryReplay( ivec_entry_t** ivecptr, int size, char *buff, int *buffSize);

/* return the information about all the nodes */
ivec_entry_t** infoVecGetAllEntries( ivec_t vec ) ;

/* return the window */
ivec_entry_t** infoVecGetWindowEntries( ivec_t vec, int *winSize ) ;

/* Return information about the given list of ips  */
ivec_entry_t** infoVecGetEntriesByIP( ivec_t vec, struct in_addr* ips, int size );

/* Requesting for a continues range of ips starting at baseIP */
ivec_entry_t**
infoVecGetContIPEntries( ivec_t vec, struct in_addr *baseIP, int size );

/* Requesting all the nodes which are up to max_age seconds old */
ivec_entry_t**
infoVecGetEntriesByAge( ivec_t vec, unsigned long max_age, int* size );

/* Prepare a window message to be sent to another infod */
void*    infoVecGetWindow( ivec_t vec, int *size, int size_flag  );

/* Handle an information message from another infod */
int      infoVecUseRemoteWindow( ivec_t vec, void *buff, int size );

/* /\* Conversion between time to age and from age to time *\/ */
/* void ivec_time2age( node_info_t *node, struct timeval *cur ); */
/* void ivec_age2time( node_info_t *node, struct timeval *cur ); */

/*
 * Get a random node in the cluster.
 * only_alive == 0 - any node
 * only_alive == 1 - must be an active node
 */
int   infoVecRandomNode( ivec_t vec, int only_alive, struct in_addr *ip );

/*
 * Get the oldest alive node
 */
int   infoVecOldestNode( ivec_t vec, struct in_addr *ip );


/* Access funcs */
ivec_entry_t*     infoVecGetVec( ivec_t vec ) ;
int               infoVecGetSize( ivec_t vec );
int               infoVecNumAlive( ivec_t vec );
int               infoVecNumDead( ivec_t vec );
int               infoVecStats( ivec_t vec, infod_stats_t *stats );
int               infoVecGetWinSize( ivec_t vec );

/****************************************************************************
 * Statistics & measurments structure
 ****************************************************************************/

/*
 * Describes a list of ips and times of death of nodes, this will be used for
 * checking that the event of death is propagated quicly (log(n)).
 */
#define INFOVEC_DEATH_LOG_SIZE   (10)
typedef struct _ivec_death_log {
	int               size;
	struct in_addr    ips[INFOVEC_DEATH_LOG_SIZE];
	// Death Propagation time
	float             deathPropagationTime[INFOVEC_DEATH_LOG_SIZE];
} ivec_death_log_t;

void  infoVecClearDeathLog(ivec_t vec);
void  infoVecGetDeathLog(ivec_t vec, ivec_death_log_t *dm); 

// ============ Mesuring age properties (avg, avg-max, max max)
#define INFOVEC_AGE_MEASURE_AUTO_CLEAR   (50)
typedef struct _ivec_age_measure {
     int                im_maxSamples;
     int                im_sampleNum;
     double             im_avgAge;       // Vector average age
     double             im_avgAgedSaved; 
     // Average maximal age during the whole measurment period
     double             im_avgMaxAge;    
     // Maximal age sampled over all the measurment period
     double             im_maxMaxAge;
     // Measuring the time of death of nodes
     
} ivec_age_measure_t;

// Measurements functions
void  infoVecClearAgeMeasure(ivec_t vec);
void  infoVecSetAgeMeasureSamplesNum(ivec_t vec, int num);
void  infoVecDoAgeMeasure(ivec_t vec);
void  infoVecGetAgeMeasure(ivec_t vec, ivec_age_measure_t *m); 


// ============== Measuring the number of entries upto age ============
#define INFOVEC_ENTRIES_UPTO_SIZE  (10)
typedef struct _ivec_entries_uptoage_measure {
     int               im_maxSamples;
     int               im_sampleNum;
     int               im_size;
     double            im_entriesUpto[INFOVEC_ENTRIES_UPTO_SIZE];
     float             im_ageArr[INFOVEC_ENTRIES_UPTO_SIZE];
     
} ivec_entries_uptoage_measure_t;

void infoVecClearEntriesUptoageMeasure(ivec_t vec);
void infoVecSetEntriesUptoageMeasure(ivec_t vec, int samples, float *ageArr, int size);
void infoVecDoEntriesUptoageMeasure(ivec_t vec);
void infoVecGetEntriesUptoageMeasure(ivec_t vec, ivec_entries_uptoage_measure_t *m);

// =========== Measuring average window size (for upto age window) ==========
typedef struct _ivec_win_size_measure {
	int                im_maxSamples;
	int                im_sampleNum;
	double             im_avgWinSize;
} ivec_win_size_measure_t;

void  infoVecClearWinSizeMeasure(ivec_t vec);
void  infoVecSetWinSizeMeasureSamplesNum(ivec_t vec, int num);
//void  infoVecDoWinSizeMeasure(ivec_t vec);
void  infoVecGetWinSizeMeasure(ivec_t vec, ivec_win_size_measure_t *m); 


/*
 * print the contents of a vec_info_t obj
 * flag == 0 - print vec
 * flag == 1 - print cont_map
 * flag == 2 - print win
 */

void infoVecPrint(FILE *f, ivec_t vec, int flag );

#endif

/****************************************************************************
 *                              E O F 
 ***************************************************************************/
