/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include <parse_helper.h>
#include <msx_error.h>
#include <msx_debug.h>
#include <infoVec.h>
#include <infod.h>
#include <easy_args.h>
#include <Mapper.h>
#include <ConfFileReader.h>
#include <ModuleLogger.h>
#include <gossimon_config.h>

#define CONF_FILE_FLAG "conf"

infod_cmdline_t *OPTS = NULL;
/*****************************************************************************
 * Command line argumant handling(using the easy_args library)
 ****************************************************************************/
// General
int usage(void *);
int show_copyright(void *);

int set_conf_file( void *void_str ){
     OPTS->opt_confFile = strdup((char*)void_str );
     return 0;
}

int set_debug( void *void_int)     { OPTS->opt_debug = 1; return 0;}

int set_debug_mode(void *void_char) {
  char *dbgModeStr = (char *)void_char;

  
  char *debugItems[51];
  int   maxItems = 50, items = 0;
  
  items = split_str(dbgModeStr, debugItems, maxItems, ",");
  if(items < 1 ) 
    return 1;

  printf("Handling debug modes:\n");

  for(int i=0; i<items ; i++) {
    printf("Got debug mode [%s]\n", debugItems[i]);
    mlog_setModuleLevelFromStr(debugItems[i], LOG_DEBUG);
    int flag = get_debug_from_short(debugItems[i]);
    if(flag) {
      msx_add_debug(flag);
    }
    
  }
  return 0;
}

int set_clear( void *void_int)     { OPTS->opt_clear = 1; return 0;}
int set_my_ip( void *void_char ){
     char *ipStr = (char *)void_char;
     if(!inet_aton(ipStr, &OPTS->opt_myIP)) {
          fprintf(stderr, "my IP is not in a leagal IPV4 format\n");
          usage(NULL);
     }
     // The IP was set from the outside so we will not try to look for IP
     // changes during the run
     OPTS->opt_forceIP = 1;
     
     return 0;
}
int set_port( void *void_int)      { OPTS->opt_infodPort = 1; return 0;}


// Provider
int set_provider( void *void_str ){
     char *provider_name = (char *)void_str;
     if(strcmp(provider_name, "linux") == 0) {
          OPTS->opt_providerType = INFOD_LP_LINUX;
     } else if (strcmp(provider_name, "mosix") == 0) {
          OPTS->opt_providerType = INFOD_LP_MOSIX;
     }
     else if (strcmp(provider_name, "external") == 0) {
          OPTS->opt_providerType = INFOD_EXTERNAL_PROVIDER;
     }
     else {
          usage(NULL);
     }
     return 0;
}
int set_watch_fs(void *void_str) {
     OPTS->opt_providerWatchFs = strdup((char*) void_str);
     return 0;
}

int set_watch_net(void *void_str) {
     OPTS->opt_providerWatchNet = strdup((char*) void_str);
     return 0;
}


int set_info_file(void *void_str) {
     OPTS->opt_providerInfoFile = strdup((char*) void_str);
     return 0;
}

int set_proc_file(void *void_str) {
     OPTS->opt_providerProcFile = strdup((char*) void_str);
     return 0;
}

int set_eco_file(void *void_str) {
     OPTS->opt_providerEcoFile = strdup((char*) void_str);
     return 0;
}

int set_jmig_file(void *void_str) {
     OPTS->opt_providerJMigFile = strdup((char*) void_str);
     return 0;
}



int set_topology( void *void_int ){
     OPTS->opt_mosixTopology = *((int*) void_int);
     return 0;
}


// Map
int set_map_type(void *void_str) {
     char *mapTypeStr = (char *)void_str;
     if(strcmp(mapTypeStr, "userview") == 0) {
          OPTS->opt_mapType = INFOD_USERVIEW_MAP;
     } else if (strcmp(mapTypeStr, "mosix") == 0) {
          OPTS->opt_mapType = INFOD_MOSIX_MAP;
     }
     else if (strcmp(mapTypeStr, "mosix-binary") == 0) {
          OPTS->opt_mapType = INFOD_MOSIX_BINARY_MAP;
     }
     else {
          usage(NULL);
     }
     return 0;
}
int set_map_file( void *void_str ){
     OPTS->opt_mapSourceType = INPUT_FILE;
     OPTS->opt_mapBuff = strdup((char*)void_str );
     OPTS->opt_mapBuffSize = strlen(OPTS->opt_mapBuff);
     return 0;
}
int set_map_cmd( void *void_str ){
     OPTS->opt_mapSourceType = INPUT_CMD;
     OPTS->opt_mapBuff = strdup((char*)void_str );
     OPTS->opt_mapBuffSize = strlen(OPTS->opt_mapBuff);
     return 0;
}

int set_allow_no_map( void *void_int) {
	OPTS->opt_allowNoMap = 1;
	return 0;
}


// Gossip
int set_time_step( void *void_int ){
     OPTS->opt_timeStep = *((int*)void_int);
     return 0;
}
int set_gossip_algo(void *void_str) {
     char  *gossipAlgoStr = (char *)void_str;

     struct infodGossipAlgo *iga;
     iga = getGossipAlgoByName(gossipAlgoStr);
     if(!iga)
          usage(NULL);
     OPTS->opt_gossipAlgo = iga;
     return 0;
}
int set_gossip_dist( void *void_str ){
     char c = *((char *)void_str);
     switch(c) {
         case 'c': OPTS->opt_gossipDistance = GOSSIP_DIST_CLUSTER ; break;
         case 'g': OPTS->opt_gossipDistance = GOSSIP_DIST_GRID ; break;
         default:
              usage(NULL);
     }
	
     return 0;
}
int set_max_age( void *void_int ){

  OPTS->opt_maxAge = *((int*)void_int);
  printf("Got max age %d\n", OPTS->opt_maxAge);
  return 0;
}
int set_win_type( void *void_str ){
     char *winTypeStr = (char *)void_str;
     if(strcmp(winTypeStr, "fix") == 0) {
          OPTS->opt_winType = INFOVEC_WIN_FIXED;
     } else if (strcmp(winTypeStr, "upto") == 0) {
          OPTS->opt_winType = INFOVEC_WIN_UPTOAGE;
     }
     else {
          usage(NULL);
     }

     return 0;
}
int set_win_param( void *void_int ){
     OPTS->opt_winParam= *((int*)void_int);
     return 0;
}


int set_avgage( void *void_double ){
     OPTS->opt_winAutoCalc = 1;
     OPTS->opt_desiredAvgAge = *((double *)void_double);
     return 0;
}

int set_avgmax( void *void_double ){
     OPTS->opt_winAutoCalc = 1;
     OPTS->opt_desiredAvgMax = *((double *)void_double);
     return 0;
}

int set_uptoentries(void *void_str ){
     char  *str = (char *) void_str;

     int     e;
     float   age;

     if(sscanf(str, "%d,%f", &e, &age) != 2) {
          fprintf(stderr, "--upto-entries ENT,AGE should be given\n");
          return 1;
     }
     
     OPTS->opt_winAutoCalc = 1;
     OPTS->opt_desiredUptoEntries = e;
     OPTS->opt_desiredUptoAge = age;
     return 0;
}

int show_copyright(void *void_str) {
     printf("infod version " GOSSIMON_VERSION_STR "\n" GOSSIMON_COPYRIGHT_STR "\n\n");
     exit(0);
}

int usage(void *void_str)
{
     
     printf(
          "infod version " GOSSIMON_VERSION_STR "\n"
          "Usage: \n"
          "\n"
          " infod [OPTIONS]\n"
          "\n"
          "An information dessimination service for a cluster of LINUX nodes\n"
          "INFOD uses a gossip algorithm to dessiminate information among the cluster node.\n"
          "Each node collects local information (such as load, memory...) and send it to\n"
          "a randomly chosen other node (with other informatio received no other nodes).\n"
          "This way each nodes collect informatio about all the cluster nodes during time\n"
          "\n"
          "\n"
          "Options:\n"
          "Provider parameters:\n"
          "--------------------\n"
          "--provider=mosix,linux,external\n"
          "                            Set the information provider to use for\n"
          "                            the built in MOSIX information module\n"
          "                            an internal mosix or linux provider\n"    
          "--watch-fs=<path>           A path to the file system the provider\n"
          "                            should watch (reporting disk capacity) on.\n"
          "                            Can be a comma seperated list of files like\n"
          "                            dir1,dir2\n"
          "\n"
          "--info-file=<file-name>     Tells the provider to add the content of the\n"
          "                            given file to the local information entry.\n"
          "--proc-file=<file-name>     Tells the provider to watch after processes\n"
          "                            given in file-name. The file should contain\n"
          "                            a line for each procesess with the name of \n"
          "                            the process and its pid-file\n"
//          "--eco-file=<file-name>      Provides the economy status of the current \n"
//          "                            node\n"
//          "--jmig-file=<file-name>     Provides the jmigrate status on the current\n"
//          "                            node\n"  
          "--watch-net=<nic1,nic2,..>  A comma separated list of network interfces\n"
          "                            to monitor. Use ifconfig -a to view all\n"
	  "                            available network interfaces\n"
	  "\n"
          "Map parameters:\n"
          "---------------\n"
          "--maptype=<userview,mosix,mosixbinary>\n"
          "                            The map type to use for node configuration\n"
          "--mapfile=file              Build map from the given file\n"
          "--mapcmd=command            Build map from the output of command\n"
	  "--allow-no-map              Allow infod to start without map\n"
	  "\n"
          "\n"
          "Information dissimination parameters:\n"
          "-------------------------------------\n"
          "--timestep=<mili-seconds>   The time step to use for sending gossip\n"
          "                            information to other nodes.\n"
          "--gossip-algo=<reg,mindead,pushpull>\n"
          "                            Type of random node selection when selecting\n"
          "                            the next node to send information to.\n"
//              "     --gossip-dist=<c|g>         Level of gossip to use [c,g]\n"
//              "     --vectype=[0,1]             \n"
//              "     --vectype-param=n           \n" 
          "--wintype=<fix|upto>        Set the information window sent to be of type\n"
          "                            \"fix\" which means that the window is of fixed\n"
          "                            length. Or of type \"upto\" which means the entries\n"
          "                            in a window will be up to a given age (see the\n"
          "                            --winparam argument)\n"
          "--winparam=n                Set the size of the window if a \"fix\" window is\n"
          "                            used. Or set the maximal age of an entry in the \n"
          "                            window if the \"upto\" window type was used\n"
          "--avgage=AGE                Set the desired average age of the vector to AGE.\n"
          "                            This option imply using a \"fix\" window type and\n"
          "                            gossip algorithm:  mindead or reg\n"
          "--avgmax=AGE                Same as above, but setting the desired average\n"
          "                            maximal age of the information vector\n"
          "--upto-entries ENT,AGE      The window size is calculated in such a way that\n"
          "                            the vector will contain ENT entries with age upto\n"
          "                            age AGE\n"
//              "     --global-info               \n"
          "\n"
          "General parameters:\n"
          "-------------------\n"
	  "--conf=<conf-file>          An alternate configuration file to use\n"
          "--myip=ip                   This infod ip, in case there are more than\n"
          "                            one IP for this node and the one to use is\n"
          "                            not the first returend by resolving of the\n"
          "                            local host name\n"
          "--port=n                    Alternative port number to use\n"  
          "--clear                     Clear screen\n"
          "--debug                     Use debug mode, no daemon\n"
          "--debug-mode mod1,mod2,..   Specify debug modes to use upon start\n"
          "--copyright                 Displaly copyright message\n"
          "--help                      This help message\n"
	  "\n\n"
	     );
     exit(EXIT_SUCCESS);
}

struct argument args[] = {
     // Provider
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "provider",  set_provider},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "watch-fs",  set_watch_fs},
     { ARGUMENT_NUMERICAL | ARGUMENT_FULL, 0, "topology",  set_topology},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "info-file", set_info_file},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "proc-file", set_proc_file},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "eco-file",  set_eco_file},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "jmig-file", set_jmig_file},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "watch-net", set_watch_net},
        
     // Map 
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "maptype",   set_map_type},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "mapcmd",   set_map_cmd},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "mapfile",  set_map_file},
     { ARGUMENT_FLAG      | ARGUMENT_FULL, 0, "allow-no-map", set_allow_no_map},
     
     
     // Gossip
     { ARGUMENT_NUMERICAL | ARGUMENT_FULL, 0, "timestep",    set_time_step},	
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "gossip-algo", set_gossip_algo},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "gossip-dist", set_gossip_dist},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "wintype",     set_win_type},	
     { ARGUMENT_NUMERICAL | ARGUMENT_FULL, 0, "winparam",    set_win_param},
     { ARGUMENT_NUMERICAL | ARGUMENT_FULL, 0, "maxage",      set_max_age},
     { ARGUMENT_DOUBLE    | ARGUMENT_FULL, 0, "avgage",      set_avgage},
     { ARGUMENT_DOUBLE    | ARGUMENT_FULL, 0, "avgmax",      set_avgmax},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "uptoage",     set_uptoentries},

        
     // General
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, CONF_FILE_FLAG,    set_conf_file},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "myip",       set_my_ip},
     { ARGUMENT_FLAG      | ARGUMENT_FULL, 0, "debug",      set_debug},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "debug-mode", set_debug_mode},
     { ARGUMENT_STRING    | ARGUMENT_FULL, 0, "port",       set_port},
     { ARGUMENT_FLAG      | ARGUMENT_FULL, 0, "help",       usage},
     { ARGUMENT_FLAG      | ARGUMENT_FULL, 0, "clear",      set_clear},            
     { ARGUMENT_FLAG      | ARGUMENT_FULL, 0, "copyright",  show_copyright},            
     { 0,0,0,0},
};


int  resolve_my_ip(struct in_addr *myIP) {
     char              hostname[MAXHOSTNAMELEN+1];
     struct hostent   *hostent;
     struct in_addr    myaddr;

     hostname[MAXHOSTNAMELEN] = '\0';
     if (gethostname(hostname, MAXHOSTNAMELEN) < 0) {
          debug_lr(MAP_DEBUG, "gethostname() failed\n");
          return 0;
     }
	
     if ((hostent = gethostbyname(hostname)) == NULL)
     {
          if (inet_aton(hostname, &myaddr) == -1) {
               debug_lr(MAP_DEBUG, "gethostbyname() faild\n");
               return 0;
          }
     }
     else  {
          bcopy(hostent->h_addr, &myaddr.s_addr, hostent->h_length);
          //myaddr = ntohl(myaddr);
     }
     *myIP = myaddr;
     return 1;
}


int initOptsDefaults(infod_cmdline_t *opts) {
     // General

     memset(opts, 0, sizeof(infod_cmdline_t));

     opts->opt_confFile = MSX_INFOD_DEF_CONF_FILE;
     opts->opt_debug = 0;
     opts->opt_clear = 0;
     if(!resolve_my_ip(&opts->opt_myIP))
          infod_critical_error("Can not find my IP\n");
     opts->opt_forceIP = 0;
     
     opts->opt_infodPort = MSX_INFOD_DEF_PORT;

     //Provider
     opts->opt_providerType      = INFOD_LP_LINUX;
     opts->opt_providerWatchFs   = NULL;
     opts->opt_providerWatchNet  = NULL;
     opts->opt_providerInfoFile  = NULL;
     opts->opt_providerProcFile  = NULL;
     opts->opt_providerEcoFile   = NULL;
     opts->opt_providerJMigFile  = NULL;
     opts->opt_mosixTopology     = MSX_INFOD_DEF_TOPOLOGY;

     // Map
     opts->opt_mapType           = INFOD_USERVIEW_MAP;
     opts->opt_mapSourceType     = INPUT_FILE;
     opts->opt_mapBuff           = MSX_INFOD_DEF_MAPFILE;
     opts->opt_mapBuffSize       = strlen(opts->opt_mapBuff);
     opts->stat_mapOk            = 0;
     opts->opt_allowNoMap        = 0;
     
     //Gossip
     opts->opt_timeStep          = INFOD_DEF_TIME_STEP;
     opts->opt_gossipAlgo        = getGossipAlgoByName("mindead");
     if(!opts->opt_gossipAlgo)
          infod_critical_error("Default gossip algo mindead is not found\n");
     opts->opt_gossipDistance    = GOSSIP_DIST_CLUSTER;
     opts->opt_maxAge            = 0;
     opts->opt_winType           = INFOD_WIN_FIXED;
     opts->opt_winParam          = INFOD_DEF_WINSIZE;
     opts->opt_winAutoCalc       = 0;
     
     // Measurments
     opts->opt_measureAvgAge     = 0;
     opts->opt_measureEntriesUptoage = 0;
     opts->opt_measureWinSize    = 0;
     opts->opt_measureInfoMsgsPerSec = 0;
     opts->opt_measureSendTime   = 0;
	
     return 1;
}

static int getInt(char *str, int *val) {
     errno = 0;
     *val = strtol(str, (char **) NULL, 10);
     if(errno) {
          return 0;
     }
     return 1;
}

static char        *validVarNames[] =
{ "provider",
  "myip",
  "port",
  "maptype",
  "mapfile",
  "mapcmd",
  "timestep",
  "gossip-algo",
  "wintype",
  "winparam",
  "avgage",
  "avgmax",
  "upto-entries",
  "watch-fs",
  "watch-net",
  "info-file",
  "proc-file",
  "eco-file",
  "jmig-file",
  "debug-mode",
  "net-watch",
  NULL};

// We should not override the options specified in the command line
int parseInfodConfFile(infod_cmdline_t *opts) {

     conf_file_t cf;


     if(!(cf = cf_new(validVarNames))) {
	  fprintf(stderr, "Error generating conf file object\n");
	  return 0;
     }
     printf("Parsing configuration file [%s]\n", opts->opt_confFile);
     if(access(opts->opt_confFile, R_OK) != 0) {
	  fprintf(stderr, "Error: configuration files [%s] does not exists or is not readable\n",
		  opts->opt_confFile);
	  return 0;
     }
     
     if(!cf_parseFile(cf, opts->opt_confFile)) {
	  fprintf(stderr, "Error parsing configuration file\n");
	  return 0;
     }

     conf_var_t ent;
     int        intVal;
     if(cf_getVar(cf, "provider", &ent)) 
	     set_provider(ent.varValue);
     if(cf_getVar(cf, "myip", &ent)) 
	     set_my_ip(ent.varValue);
     if(cf_getVar(cf, "port", &ent)) {
          if(!getInt(ent.varValue, &intVal)) {
               fprintf(stderr, "Error: parameter port is not an integer\n");
               return 0;
          }
          set_port(&intVal);
     }
     if(cf_getVar(cf, "maptype", &ent)) 
	     set_map_type(ent.varValue);
     if(cf_getVar(cf, "mapfile", &ent)) 
	     set_map_file(ent.varValue);
     if(cf_getVar(cf, "mapcmd", &ent)) 
	     set_map_cmd(ent.varValue);
     if(cf_getVar(cf, "timestep", &ent)) {
          if(!getInt(ent.varValue, &intVal)) {
               fprintf(stderr, "Error: parameter timestep is not an integer\n");
               return 0;
          }
          set_time_step(&intVal);
     }
     if(cf_getVar(cf, "gossip-algo", &ent)) 
	     set_gossip_algo(ent.varValue);
     if(cf_getVar(cf, "wintype", &ent)) 
	     set_win_type(ent.varValue);
     if(cf_getVar(cf, "winparam", &ent)) 
	     set_win_param(ent.varValue);

     if(cf_getVar(cf, "avgage", &ent)) {
          if(!getInt(ent.varValue, &intVal)) {
               fprintf(stderr, "Error: parameter avgage is not an integer\n");
               return 0;
          }
          set_avgage(&intVal);
     }
     
     if(cf_getVar(cf, "avgmax", &ent)) {
          if(!getInt(ent.varValue, &intVal)) {
               fprintf(stderr, "Error: parameter avgmax is not an integer\n");
               return 0;
          }
          set_avgmax(&intVal);
     }
     
     if(cf_getVar(cf, "upto-entries", &ent)) {
          if(!getInt(ent.varValue, &intVal)) {
               fprintf(stderr, "Error: parameter upto-entries is not an integer\n");
               return 0;
          }
          set_uptoentries(&intVal);
     }

     if(cf_getVar(cf, "watch-fs", &ent)) 
	     set_watch_fs(ent.varValue);
     if(cf_getVar(cf, "watch-net", &ent)) 
	     set_watch_net(ent.varValue);
     if(cf_getVar(cf, "info-file", &ent)) 
	     set_info_file(ent.varValue);
     if(cf_getVar(cf, "eco-file", &ent)) 
	     set_eco_file(ent.varValue);
     if(cf_getVar(cf, "proc-file", &ent)) 
	     set_proc_file(ent.varValue);
     if(cf_getVar(cf, "jmig-file", &ent)) 
	  set_jmig_file(ent.varValue);
     

     cf_free(cf);
	
	

     return 1;
}

int getConfFileCmdLine(infod_cmdline_t *opts, int argc, char **argv) {
     
     for(int i=0 ; i < argc ; i++) {
	  if(strcmp(argv[i], "--" CONF_FILE_FLAG) == 0) {
	       printf("found conf file: %s\n", argv[i]);
	       if(i == argc - 1) {
	       need_value:
		    fprintf(stderr, "--%s argument requires a value\n", CONF_FILE_FLAG);
		    return 0;
	       }
	       if(strncmp(argv[i+1], "-", 1) == 0) {
		    goto need_value;
	       }
	       opts->opt_confFile = strdup(argv[i+1]);
	       return 1;
	  }
     }
     return 0;
}

void checkHelpCmdLine(infod_cmdline_t *opts, int argc, char **argv) {
     
     for(int i=0 ; i < argc ; i++) {
          if(strcmp(argv[i], "--help") == 0) {
               usage(NULL);
          }
          if(strcmp(argv[i], "--copyright") == 0) {
               show_copyright(NULL);
          }
     }
}


int parseInfodCmdLine(int argc, char **argv, infod_cmdline_t *opts)
{
     if(!opts)
          return 0;
     // Setting the global opts struct so the functions will update it
     OPTS = opts;
     /* test command line argumets */ 
     checkHelpCmdLine(opts, argc, argv);

     if(getConfFileCmdLine(opts, argc, argv)) {
	  printf("Detected conf file flag\n");
     }
     
//     if(opts->opt_confFile) {
     if(!parseInfodConfFile(opts)) {
	  fprintf(stderr, "Errors parsing configuration file %s\n",
		  opts->opt_confFile);
	  exit(1);
     }
	     //   }
     
     easy_args_env_prefix = INFOD_EASY_ARGS_PREFIX ;
     parse_command_line( argc, argv, args );

     return 1;
}
