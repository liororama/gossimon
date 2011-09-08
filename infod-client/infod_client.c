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

/****************************************************************************
 *
 * File: infod_client.c
 *
 ****************************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>

#include <msx_debug.h>
#include <msx_error.h>
#include <info.h>
#include <infolib.h>
#include <infoxml.h>
#include <easy_args.h>
#include <info_iter.h>
#include <info_reader.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <parse_helper.h>
#include <ModuleLogger.h>

#include <gossimon_config.h>

typedef enum {
	OPT_ALL_DATA = 0,
	OPT_CONT_DATA,
	OPT_SUBSET_DATA,
	OPT_SUBSET_NAME_DATA,
	OPT_SUBSET_AGE_DATE,
	OPT_ALL_NAMES,
	OPT_STATS,
	OPT_DESCRIPTION,
	
	OPT_PS   = 100,
	OPT_DEAD,
	OPT_DEAD_LIST,
	OPT_LIVE_LIST,
	OPT_ALL_LIST,
	OPT_AGE_HISTOGRAM,
	OPT_AGE_CDF,
	OPT_WINDOW,
	OPT_ERROR,
}  clientOptsT;

/****************************************************************************
 * Command line handling and global variables
 ***************************************************************************/
typedef struct _prog_param {
     char           *server;
     unsigned short  port;

     char           *queryStr;
     int             queryId;
     char           *queryArgsStr;
     char           *infoItemsStr;
     char           *infoItemsList[50];
     int             xml;
     int             interactiveMode;
     int             repeat;
     int             timeout;
     int             verbose;
     int             debug;
} prog_param_t;

prog_param_t *glob_pp;
// Functions declarations
int getAgeInfo(prog_param_t *pp, int type);
int getNodesList(prog_param_t *pp, int type);
int getInfoWindow(prog_param_t *pp);
void handleInteractive(prog_param_t *pp);


int parseQuery( prog_param_t *pp, char *query_name ) {

	if (strcmp(query_name,"all-data") == 0)
		pp->queryId = OPT_ALL_DATA;
	else if (strcmp(query_name,"all") == 0)
		pp->queryId = OPT_ALL_DATA;

	// Status
	else if (strcmp(query_name,"stats") == 0)
		pp->queryId = OPT_STATS;
	
	else if (strcmp(query_name,"description") == 0)
		pp->queryId = OPT_DESCRIPTION;
	else if (strcmp(query_name,"desc") == 0)
		pp->queryId = OPT_DESCRIPTION;
	
	else if (strcmp(query_name,"ps") == 0)
		pp->queryId = OPT_PS;
	else if (strcmp(query_name,"dead") == 0)
		pp->queryId = OPT_DEAD_LIST;
	else if (strcmp(query_name,"dead-list") == 0)
		pp->queryId = OPT_DEAD_LIST;
	else if (strcmp(query_name,"live-list") == 0)
		pp->queryId = OPT_LIVE_LIST;
	else if (strcmp(query_name,"all-list") == 0)
		pp->queryId = OPT_ALL_LIST;
	else if (strcmp(query_name,"age-histo") == 0)
		pp->queryId = OPT_AGE_HISTOGRAM;
	else if (strcmp(query_name,"age-cdf") == 0)
		pp->queryId = OPT_AGE_CDF;
	else if (strcmp(query_name,"window") == 0)
		pp->queryId = OPT_WINDOW;
	else {
		int val;
		char *endptr;
		char *str = (char*)query_name;
		
		val = strtol(str, &endptr, 10);
		if(val == 0 && *endptr != '\0')
			pp->queryId = OPT_ERROR;
		else if(val < 0)
			pp->queryId = OPT_ERROR;
		else
			pp->queryId = val;
	}
	return pp->queryId != OPT_ERROR ? 1 : 0;
}


void execPS(const char* server);

char usageStr[] =
"infod-client version " GOSSIMON_VERSION_STR "\n"
"\n"
"Usage: infod-client [-p|--port=port] [-h|--server=host]\n"
"                    [-q|--query=OPT]\n"
"\n"
"Connect to a running infod and retreive information about the\n"
"cluster.\n"
"\n"
"OPTIONS:\n"
" -h, --server <host>  Name of the machine to contact.\n" 
"                      If not specified, use the local machine\n" 
" -p, --port <port>    Port number\n" 
" -i, --interactive    infod-client will run in interactive mode\n"
" -q, --query <name>   Query to perform (see below)\n" 
"     --args ARG       Arguments for selected query\n"
"     --info item1,item2,...\n"
"                      Selecting specific information items form each node\n"
"                      returned by the query. This is relevant only for some\n"
"                      queries\n"
"                      name. Multiple items should be seperated by commas\n"   
" -r, --repeat SEC     Work in continouse mode, repeating every SEC seconds\n"
" -t, --timeout <sec>  Wait for sec seconds before exiting with error.\n"
"                      Usually infod-client will terminats before this.\n"
"\n"
" -v, --verbose        Print verbose information messages\n"
"     --copyright      Show copyright message\n"
"     --help           This message\n"
"\n"
"The --query flag options:\n"
"\n"
"   General queries:\n"
"   ----------------\n"
"   all         Get the data about all the nodes (DEFAULT)\n"
"   stats       Get the cluster statistics\n" 
"   desc        Get the description of the information\n" 
"\n"
"   Nodes lists queries:\n"
"   --------------------\n"
"   dead        Get list of dead nodes\n"
"   dead-list   Get comma separated list of dead nodes\n"
"   live-list   Get comma separated list of live nodes\n"
"   all-list    Get comma separated list of all cluster nodes\n"
"\n"
"   Age properties queries:\n"
"   -----------------------\n"
"   window      Get the node information window\n"
"   age-histo   Get age histogram of information vector\n"
"   age-cdf     Get age CDF of information vector\n"
"\n"
"\n"
"Selecting specific information:\n"
" --info  item1,item2,item3\n"
"\n"
" For example: load,ncpus\n"
"\n\n"
"For bug report and additional support contact www.clusterlogic.net \n"
"Also try www.sourceforge.net/projects/gossimon\n"
"\n";

void usage(){
  fprintf(stderr, "%s", usageStr);
     exit( EXIT_FAILURE ) ;
}

int show_copyright() {
     printf("infod-client version " GOSSIMON_VERSION_STR "\n" GOSSIMON_COPYRIGHT_STR "\n\n");
     exit(0);
}


static void set_defaults(prog_param_t *pp) {
     memset(pp, 0, sizeof(prog_param_t));
     pp->port = MSX_INFOD_LIB_DEF_PORT ; 
}

static void
parse_cmdline(int argc, char **argv, prog_param_t *pp ) {

	int c;
		
	while( 1 ) {
				
		//int this_option_optind = optind ? optind : 1;
		int option_index = 0;

		static struct option long_options[] = {
			{"query",       1, 0, 'q'},
			{"server",      1, 0, 'h'},
			{"port",        1, 0, 'p'},
			{"arg",         1, 0, 'a'},
			{"info",        1, 0, 0  },
			{"repeat",      1, 0, 'r'},
			{"timeout",     1, 0, 't'},
			{"interactive", 0, 0, 'i'},
			{"help",        0, 0, 0},
                        {"debug",       1, 0, 0},
                        {"copyright",   0, 0, 0}, 
			{"verbose",     0, 0, 'v'},
			{0, 0, 0, 0}
		};
	
		c = getopt_long (argc, argv, "q:s:p:a:r:t:h:iv",
				 long_options, &option_index);
		if (c == -1)
			break;
	
		switch (c) {
		    case 0:
			 if(strcmp(long_options[option_index].name,
				   "info") == 0) {
			      pp->infoItemsStr = strdup(optarg);
			      int n = split_str(pp->infoItemsStr, pp->infoItemsList, 50, ",");
			      if(n>0) {
				   pp->infoItemsList[n] = NULL;
			      }
			      else {
				   fprintf(stderr, "Error parsing info items\n");
				   exit(1);
			      }
			 }
			 else if(strcmp(long_options[option_index].name,
					"help") == 0) {
			      usage();
			      exit(0);
			 }
                         else if(strcmp(long_options[option_index].name,
                                        "debug") == 0) {
                              char *debugItems[51];
                              int   maxItems = 50, items = 0;
                              
                              items = split_str(optarg, debugItems, maxItems, ",");
                              if(items >= 1 )  {
                                   printf("Handling debug modes:\n");
                                   for(int i=0; i<items ; i++) {
                                        printf("Got debug mode: [%s]\n", debugItems[i]);
                                        mlog_setModuleLevelFromStr(debugItems[i], LOG_DEBUG);
                                   }
                              }
			 }

                         else if(strcmp(long_options[option_index].name,
					"copyright") == 0) {
			      show_copyright();
			      exit(0);
			 }
			 break;

			 
		    case 'q':
			 pp->queryStr = strdup(optarg);
			 if(!parseQuery(pp, pp->queryStr)) {
			      fprintf(stderr, "Error, query is not supported [%s]\n", pp->queryStr);
			      exit(1);
			 }
			 break;
			 
			 
		case 'h':
		     pp->server = strdup(optarg);
		     break;
		     
		case 'p':
		     pp->port = atoi(optarg);
		     break;
		     
		case 'a':
		     pp->queryArgsStr = strdup(optarg);
		     break;
		     
		case 'r':
		     pp->repeat = atoi(optarg);
		     break;
		     
		case 't':
		     pp->timeout = atoi(optarg);
		     break;
		case 'i':
		     pp->interactiveMode = 1;
		     break;
		case 'v':
		     pp->verbose = 1;
		     break;
		     
		default:
		     printf ("Getopt returned character code 0%o??\n",
			     c);
		}
	}
    
	if( optind < argc ) {
		printf ("non-option ARGV-elements: ");
		while( optind < argc )
			printf ("%s ", argv[optind++]);
		printf ("\n");
		usage();
		exit( 0 );
	}
}


/****************************************************************************
 * Parse the command line arguments
 ***************************************************************************/
int* parseQueryArgs( prog_param_t *pp, char *argsStr, int *arrsize ) {

	int *arr = 0;
	char *ptr1 = NULL, *ptr2 = NULL;
	int  maxsize = 16 ;
	int  size = 0 ;

	*arrsize = 0;

	if( !pp->queryArgsStr )
		return NULL;
      
	if( !( arr = (int*)malloc( maxsize * sizeof(node_t)))){
		debug_r( "Error: malloc\n" );
		return NULL;
	}
      
	for( ptr1 = pp->queryArgsStr ; *ptr1 ; ptr1 = ptr2 ){

		/* Get all the spaces */ 
		for( ; *ptr1 && isspace( *ptr1 ) ; ptr1++  )
			*ptr1 = '\0';
            
		/* Get the argument */ 
		for( ptr2 = ptr1 ; *ptr2 && !isspace (*ptr2) ; ptr2++ )
			if( !(isdigit(*ptr2))){
				debug_r( "Error: unexpected char in args\n" );
				free(arr);
				return NULL;
			}
            
		for(  ; *ptr2 && isspace(*ptr2) ; ptr2++ )
			*ptr2 = '\0';

		if( size == maxsize ){
			maxsize *=2 ;
			if( !( arr = (node_t*)
			       realloc( arr, maxsize * sizeof(node_t)))){
				free(arr);
				debug_r( "Error: realloc Failed\n" );
				return NULL ;
			}
		}
                  
		arr[ (size)++ ] = atoi(ptr1) ;
	}
      
	*arrsize = size;
	return arr;
}

void
client_exit(prog_param_t *pp) {
     if(pp->verbose)
	  fprintf(stderr, "Error retrieving data. Maybe there is no infod "
		  "running on %s?\n\n", pp->server? pp->server:"the local machine" );
     exit( EXIT_FAILURE );
}

void sig_alarm_hndl(int sig) {
     
     // Got sigalarm so there was a timeout
     if(glob_pp->verbose)
	  fprintf(stderr, "Error: timeout of (%d sec) expired\n",
		  glob_pp->timeout);
     if (! glob_pp->interactiveMode)
	  exit(1);
}


void install_timeout(int sec)
{
     
     struct sigaction act;
     struct itimerval timeout;
     
     
     act.sa_handler = sig_alarm_hndl ;
     sigemptyset ( &( act.sa_mask ) ) ;
     sigaddset ( &( act.sa_mask) , SIGINT ) ;
     sigaddset ( &( act.sa_mask) , SIGTERM ) ;
     act.sa_flags = 0; //SA_RESTART ;
     
     if ( sigaction ( SIGALRM, &act, NULL ) < -1 ) {
	  fprintf(stderr, "Error: failled installing SIGALARM handler\n" ) ;
	  exit(1);
     }
     
     timeout.it_value.tv_sec = sec;
     timeout.it_value.tv_usec = 0;
     timeout.it_interval.tv_sec = sec;
     timeout.it_interval.tv_usec = 0;
     setitimer( ITIMER_REAL, &timeout, NULL);
     
}

void clear_timeout() {
     setitimer( ITIMER_REAL, NULL, NULL);
}

/****************************************************************************
 * Main:
 ***************************************************************************/
int
main ( int argc, char** argv ) {

     char             *desc = NULL;
     char             *names = NULL;
     subst_pes_args_t *sub; 
     cont_pes_args_t  *cont; 
     int               size = 0;
     unsigned long    *age = NULL;
     prog_param_t     *pp = malloc(sizeof(prog_param_t));

     mlog_init();
     mlog_addOutputFile("/dev/tty", 1);
     mlog_registerColorFormatter((color_formatter_func_t)sprintf_color);
     
     mlog_registerModule("infoxml", "XML generation of info", "infoxml");
     mlog_registerModule("inforeader", "Reading info", "inforeader");
     

     glob_pp = pp;
     set_defaults(pp);
     parse_cmdline( argc, argv, pp);

     

     if( pp->debug )
	  msx_set_debug( 1 );
     
     
     if(pp->timeout && !pp->interactiveMode)
	  install_timeout(pp->timeout);
     
     if (pp->interactiveMode)
     {
	  handleInteractive(pp);
	  return 0;
     }
     
     switch( pp->queryId ) {
	  
	 case OPT_ALL_DATA:
	      if(pp->infoItemsStr) 
		   desc = infoxml_all( pp->server, pp->port, pp->infoItemsList);
	      else
		   desc = infoxml_all( pp->server, pp->port, NULL );

	      if( desc ) {
		   printf( "%s\n", desc );
		   free(desc);
		   return 0;
	      }
	      client_exit(pp);
	      
	      
	 case OPT_CONT_DATA:
	      if( !( cont =(cont_pes_args_t*) parseQueryArgs(pp, pp->queryArgsStr, &size ))){
		   debug_r( "Error: failed parsing args\n" );
		   client_exit(pp);
	      }
	      
	      desc = infoxml_cont_pes( pp->server, pp->port, cont );
	      if( desc ) {
		   printf( "%s\n", desc );    
		   free(desc);
		   return 0;
	      }
	      client_exit(pp);
	      
	 case OPT_SUBSET_DATA:
	      if( !( sub = (subst_pes_args_t*) parseQueryArgs(pp, pp->queryArgsStr, &size )))
	      {
		   debug_r( "Error: failed parsing args\n" );
		   client_exit(pp);
	      }
	      
	      desc = infoxml_subset_pes( pp->server, pp->port, sub );
	      if( desc ) {
		   printf( "%s\n", desc );    
		   free(desc);
		   return 0;
	      }
	      client_exit(pp);
	      
	 case OPT_SUBSET_NAME_DATA:
	      desc = infoxml_subset_name( pp->server, pp->port, pp->queryArgsStr ) ; 
	      if( desc ) {
		   printf( "%s\n", desc );    
		   free(desc);
		   return 0;
	      }
	      client_exit(pp);
	      
	 case OPT_SUBSET_AGE_DATE:
	      if( !( age = (unsigned long*)parseQueryArgs(pp, pp->queryArgsStr, &size ))){
		   debug_r( "Error: failed parsing args\n" );
		   client_exit(pp);
	      }
	      
	      desc = infoxml_up2age( pp->server, pp->port, *age );
	      if( desc ) {
		   printf( "%s\n", desc );    
		   free(desc);
		   return 0;
	      }
	      client_exit(pp);
	      
	 case OPT_ALL_NAMES:
	      names = infolib_all_names( pp->server, pp->port );
	      printf( "Names:\n%s\n", names );
	      free( names );
	      return 0;
	      
	 case OPT_STATS:
	      desc = infoxml_stats( pp->server, pp->port );
	      if( desc ) {
		   printf( "%s\n", desc );
		   free(desc);
		   return 0;
	      }
	      client_exit(pp);
	      
	 case OPT_DESCRIPTION:
	      desc = infolib_info_description( pp->server, pp->port );
	      printf( "\n%s\n", desc ) ;
	      free( desc ) ;
	      return 0;
	      
	 case OPT_PS:
	      execPS(pp->server);
	      break;
	      
	      //---------- Nodes list ---------------
	 case OPT_DEAD:
	 case OPT_DEAD_LIST:
	      if(!getNodesList(pp, 1))
		   client_exit(pp);
	      break;
	 case OPT_LIVE_LIST:
	      if(!getNodesList(pp, 2))
		   client_exit(pp);
	      break;
	 case OPT_ALL_LIST:
	      if(!getNodesList(pp, 3))
		   client_exit(pp);
	      break;
	      
	      
	      // ---------- Age properties ---------
	 case OPT_AGE_HISTOGRAM:
	 case OPT_AGE_CDF:
	      if(!getAgeInfo(pp, pp->queryId))
		   client_exit(pp);
	      break;
	 case OPT_WINDOW:
	      if(!getInfoWindow(pp))
		   client_exit(pp);
	      break;
	 default:
	      fprintf(stderr,"Error unknown option!\n");
	      usage();
	      break;
     }
     return 0;
}


void printAgeHisto(int *ageHisto, int ageHistoSize, int nodes)
{
	printf("Total of %d nodes:\n", nodes); 
	for(int i=0; i<ageHistoSize ; i++) {
		printf("Histo %3d %4d\n", i, ageHisto[i]);
	}
}

void printAgeCDF(int *ageHisto, int ageHistoSize, int nodes)
{
	double *cdf;
	double  nodes_d = (double) nodes;
	
	cdf = malloc(ageHistoSize * sizeof(double));
	if(!cdf) {
		fprintf(stderr, "No memory\n");
		exit(1);
	}
	cdf[0] = (double)ageHisto[0]/nodes_d;
	for(int i=1; i<ageHistoSize ; i++) {
		ageHisto[i] += ageHisto[i-1];
		cdf[i] = (double)ageHisto[i]/nodes_d;
	       
	}
	for(int i=0; i<ageHistoSize ; i++) {
		printf("CDF %3d %4f\n", i, cdf[i]);
	}
	
}
// Getting the historgram/CDF of vector entries age
int getAgeInfo(prog_param_t *pp, int type) {
	idata_t        *rpl;
	idata_entry_t  *cur = NULL;
	idata_iter_t   *iter = NULL;
	
	rpl = infolib_all(pp->server, pp->port);
	if(!rpl)
		return 0;
	if( !( iter = idata_iter_init( rpl ))){
		free( rpl );
		return 0;
	}

	int maxSec = 0;
	int nodes = 0;
	// Iterating over all data
	while( (cur = idata_iter_next(iter))) {
		// Taking information only from alive nodes
		if(cur->data->hdr.status & INFOD_ALIVE )
		{
			nodes++;
			if(cur->data->hdr.time.tv_sec > maxSec)
				maxSec = cur->data->hdr.time.tv_sec;
		}
	}
	
	// Allocating the age histogran
	int *ageHisto;
	int  ageHistoSize = maxSec + 1;
	ageHisto = malloc(ageHistoSize  * sizeof(int));
	if(!ageHisto)
		return 0;
	bzero(ageHisto, ageHistoSize * sizeof(int));
		
	// Iterating again over the vector
	idata_iter_rewind(iter);
	while( (cur = idata_iter_next(iter))) {
		if(cur->data->hdr.status & INFOD_ALIVE )
		{
			int age = cur->data->hdr.time.tv_sec;
			ageHisto[age]++; // Updating histogram
		}
	}
	if(type == OPT_AGE_HISTOGRAM)
		printAgeHisto(ageHisto, ageHistoSize, nodes);
	else
		printAgeCDF(ageHisto, ageHistoSize, nodes);
	
	idata_iter_done( iter );
	free( rpl ) ;
	return 1;
	
}

/**********************************************************************
 Description
   Get a list of nodes from infod
 ARGS
   type     Type of list of nodes where
            0 - dead nodes
            1 - live nodes
            2 - all nodes
Return Value
  0 on error
  1 on success
*********************************************************************/


int getNodesList(prog_param_t *pp, int type) {
	idata_t        *rpl;
	idata_entry_t  *cur = NULL;
	idata_iter_t   *iter = NULL;
	int             nodeNum =0;
	
	rpl = infolib_all(pp->server, pp->port);
	if(!rpl)
		return 0;
	if( !( iter = idata_iter_init( rpl ))){
		free( rpl );
		return 0;
	}

	
	while( (cur = idata_iter_next(iter))) {
		int printNode = 0;
		
		switch (type) {
		    case 1:
			    // Only dead nodes
			    if(!(cur->data->hdr.status & INFOD_ALIVE )) 
				    printNode = 1;
			    break;
		    case 2:
			    // Only live nodes
			    if((cur->data->hdr.status & INFOD_ALIVE )) 
				    printNode = 1;
			    break;
		    case 3:
			    // All nodes
			    printNode = 1;
			    break;
		}

		// Printing the nodes list
		if(printNode) {
			printf("%c%s", nodeNum ? ',':' ', cur->name);
			nodeNum++;
		}
	}
	printf("\n");
	idata_iter_done( iter );
	free( rpl ) ;
	return 1;
}

int getInfoWindow(prog_param_t *pp)
{
	idata_t        *rpl;
	idata_entry_t  *cur = NULL;
	idata_iter_t   *iter = NULL;
	
	rpl = infolib_window(pp->server, pp->port);
	if(!rpl)
		return 0;
	if( !( iter = idata_iter_init( rpl ))){
		free( rpl );
		return 0;
	}

	// Iterating over all data
	while( (cur = idata_iter_next(iter))) {
		// Taking information only from alive nodes
		if(cur->data->hdr.status & INFOD_ALIVE )
		{
			printf("%-15s %d.%d\n",
			       cur->name,
			       (int) cur->data->hdr.time.tv_sec,
			       (int) cur->data->hdr.time.tv_usec);
		}
	}
	idata_iter_done( iter );
	free( rpl ) ;
	return 1;
}


/******************************************************************************
 *
 *****************************************************************************/

void handleInteractive(prog_param_t *pp)
{
     char command[64];
     const char* currentNodeName = NULL;
  

      while (1)
      {
           
          fgets(command, 63, stdin);
           
           if (strcasecmp(command,"EXIT") == 0)
           {
                return;
           }
           
           if (strncasecmp(command,"GET ",4) != 0)
           {
                fprintf(stdout,"<error>Bad Command</error>\n");
                fflush(stdout);
           }
           else
           {
                currentNodeName = (const char*)(&command[4]);
                //fprintf(stdout,"Getting %s",currentNodeName);
                
                char *desc = NULL;

                if(pp->timeout)
                     install_timeout(pp->timeout);
                
                desc = infoxml_all( (char *)currentNodeName, pp->port, NULL );

                if(pp->timeout)
                     clear_timeout();

                if( desc ) 
                {
                     char tmpStr[20];
                     int  tmpLen, descLen;
                     descLen = strlen(desc);
                     tmpLen = sprintf(tmpStr, "%i\n", descLen);  
                     write(1, tmpStr, tmpLen);
                     write(1, desc, descLen );
                     free(desc);
                } else
                {
                     fprintf(stdout,"<cluster_info>\n");
                     fprintf(stdout,"<error>error retrieving data from %s.\nmake sure that server is running with infod</error>",currentNodeName);
                     fprintf(stdout,"</cluster_info>\n");
                     fflush(stdout);
                     
                }
           }
      }
}


void execPS(const char* server)
{

      char cmdStr[256];

      sprintf(cmdStr,"ps -eo pid,user,%%cpu,%%mem,args --sort -%%cpu");
    	      
      execlp("//usr//bin//rsh","rsh",server,cmdStr,(char *)NULL);  
}
