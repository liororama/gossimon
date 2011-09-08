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
 * Author(s): Peer Ilan, Amar Lior
 *
 *****************************************************************************/

/******************************************************************************
 * File: infod.h. infod definitions and declarations.
 *****************************************************************************/

#ifndef _MOSIX_INFOD
#define _MOSIX_INFOD

#include <sys/time.h>
#include <info.h>
#include <gossimon_config.h>

/****************************************************************************
 * Constants 
 ***************************************************************************/
// General 
#define MSX_INFOD_DEF_CONF_FILE       GOSSIMON_CONFIG_DIR"/infod.conf"
#define MSX_INFOD_DEF_BIND_GIVEUP     (65)     // seconds to bind
#define MSX_INFOD_MAX_ENTRY_SIZE      (8192)
#define MSX_INFOD_LOCK_FILE           "/proc/self/lock"
#define MSX_INFOD_KCOMM_SOCK_NAME     "/var/..itm_"
#define MSX_INFOD_KCOMM_MAX_SEND      (100)
#define MSX_INFOD_DEF_COMM_TIMEOUT    (DEF_TIMEOUT)
#define MSX_INFOD_COMM_DELAY_FACTOR   (10000)
#define INFOD_CWD                     "/tmp"
#define INFOD_NATIVE_CORE_FILE        "core"
#define INFOD_CORE_FILE_PREFIX        "infod-core"
#define INFOD_EASY_ARGS_PREFIX        "INFOD"

#define INFOD_MAX_CRASH_NUM           (50)
#define SMALL_BUFF_SIZE               (256)

#define PIM_DEF_PERIOD                (5)

#ifndef INFOD2
#define INFOD_DEF_PID_FILE         "/var/run/infod.pid"
#else
#define INFOD_DEF_PID_FILE         "/var/run/infod2.pid"
#endif
//--- Provider ----

// The currently supported provider types
#define INFOD_LP_LINUX                (1)
#define INFOD_LP_MOSIX                (2)
#define INFOD_EXTERNAL_PROVIDER       (4)

#define MSX_INFOD_DEF_TOPOLOGY        (1)

//---- MAP ----
// Map types definitions
#define INFOD_USERVIEW_MAP            (1)
#define INFOD_MOSIX_MAP               (2)
#define INFOD_MOSIX_BINARY_MAP        (3)

#define MSX_INFOD_DEF_MAPFILE         GOSSIMON_CONFIG_DIR"/infod.map"
#define MSX_INFOD_DEF_MAPCMD          "cat "GOSSIMON_CONFIG_DIR"/infod.map"

#define MSX_INFOD_RELOAD_MAP_TIME     (60)     // seconds to reload map file
#define MSX_INFOD_PERIODIC_ADMIN_TIME (10)     // seconds to performs various
                                               // admin tasks

//---- Gossip ----
#define INFOD_DEF_TIME_STEP           (500)    // Default time step in milli-seconds
#define INFOD_DEF_WINSIZE             (20)
#define INFOD_DEF_WINAGE              (4)
#define INFOD_DEF_WIN_PARAM           (8)
#define INFOD_DEF_GINFOD_PARAM        (32)
#define INFOD_SEND_TO_ANY_CYCLE       (5)

// Gossip Algo
#include <infoVec.h>

/* Gossip Action: who to send a message and what type */
struct gossipAction {
     struct in_addr   randIP;
     int              msgType;
     int              msgLen;
     void            *msgData;
     int              keepConn;
};

/* Function for the selected gossip protocole */
typedef int    (*gossip_init_func_t)    (void **gossip_data);
typedef int    (*gossip_step_func_t)    (ivec_t vec, void *gossip_data, struct gossipAction *a);

struct infodGossipAlgo {
     char                *algoName;
     gossip_init_func_t   initFunc;
     gossip_step_func_t   stepFunc;
};

struct infodGossipAlgo  *getGossipAlgoByName(char *name);
struct infodGossipAlgo  *getGossipAlgoByIndex(int index);


// Gossip distance
#define GOSSIP_DIST_CLUSTER      (1)
#define GOSSIP_DIST_GRID         (2)
// Window types
#define INFOD_WIN_FIXED           (1)
#define INFOD_WIN_UPTOAGE         (2)

typedef struct __infod_cmdline {
     // General
     int             opt_debug;
     int             opt_clear;
     int             opt_forceIP;
     struct in_addr  opt_myIP;
     int             opt_myPE;
     int             opt_infodPort;
     char *          opt_confFile;
	
     // Provider
     int             opt_providerType;
     char           *opt_providerWatchFs;
     int             opt_mosixTopology;
     char           *opt_providerInfoFile;
     char           *opt_providerProcFile;
     char           *opt_providerEcoFile;
     char           *opt_providerJMigFile;
     char           *opt_providerWatchNet;
     // Map
     int             opt_mapType;
     int             opt_mapSourceType;
     char           *opt_mapBuff;
     int             opt_mapBuffSize;
     int             stat_mapOk;  // Should be 1 if map is ok
	int             opt_allowNoMap;	
     // Gossip
     unsigned int    opt_timeStep;
     struct infodGossipAlgo  *opt_gossipAlgo;
     int             opt_gossipDistance;
     int             opt_maxAge;
     int             opt_winType;
     int             opt_winParam;

     int             opt_winAutoCalc;
     double          opt_desiredAvgAge;
     double          opt_desiredAvgMax;
     int             opt_desiredUptoEntries;
     double          opt_desiredUptoAge;
     
     // Measurments
     int             opt_measureAvgAge;
     int             opt_measureEntriesUptoage;
     int             opt_measureWinSize;
     int             opt_measureInfoMsgsPerSec;
     int             opt_measureSendTime;
	
} infod_cmdline_t;


struct infod_runtime_info {
     /* Infod uptime handling */
     struct timeval    startTime;

     /* Number of child forked so far (meaning number of craches) */
     int               childNum;
     // Info library requests (activity of clients)
     unsigned int      reqNum;

     // Information dessimination messages (will indicate the activity of infod);
     struct timeval    infoMsgsStartTime;
     int               infoMsgsSamples;
     int               infoMsgsMaxSamples;
     double            infoMsgsPerSec;
     int               infoMsgs;
	
     unsigned int      desiminationNum;

     // Measuring the time it takes to compleate sending an information message.
     // We assume here (for simplicity) that there will be no dead nodes so
     // each send will be finished before the next one starts. This is a
     // reasonable assumption if the time step is large (1 sec).
     struct timeval    sendStartTime;
     struct timeval    sendFinishTime;
     struct timeval    sendAccumulatedTime;
     int               sentMsgs;
     int               sentMsgsSamples;
     int               sentMsgsMaxSamples;

     int               timeSteps;
     double            totalTime;
};

extern struct infod_runtime_info  runTimeInfo;

// Global variables
extern int             glob_daemonMode;
extern pid_t           glob_working_pid;
extern pid_t           glob_parent_pid;


// Misc functions (in infod_misc.c)
inline void infod_critical_error( char *fmt, ... );
void infod_exit();
void infod_log( int priority, char *fmt, ... );
void log_init();
int createPidFile();
int removePidFile();
int pidFileExists();


// Command line and init defaults (infod_cmd_line.c)
int initOptsDefaults(infod_cmdline_t *opts);
int parseInfodCmdLine(int argc, char **argv, infod_cmdline_t *opts);
int  resolve_my_ip(struct in_addr *myIP);


// Infod Parent functions (infod_parent.c)
void infod_parent_exit(int status);
void infod_parent_critical_error( char *msg );
void sig_parent_hndl( int sig );
int infod_parent_signals();
int infod_parent();






#endif

/****************************************************************************
 *                              E O F 
 ***************************************************************************/
