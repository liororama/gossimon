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

#define __USE_GNU
#define _GNU_SOURCE

#include <sys/param.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <dirent.h>

#include <msx_common.h>
#include <msx_proc.h>
#include <msx_error.h>
#include <msx_debug.h>

#include <info.h>
#include <infodctl.h>

#include <infolib.h>
#include <info_reader.h>
#include <info_iter.h>
#include <gossimon_config.h>

char *program_name;

#define GLOB_RES_BUFF_SIZE   (1024)
char glob_res_buff[GLOB_RES_BUFF_SIZE];

#define GLOB_ARG_BUFF_SIZE   (256)
char glob_arg_buff[GLOB_ARG_BUFF_SIZE];

// Infod and Mapping variables
char *glob_info_desc = NULL;
variable_map_t *glob_info_var_mapping = NULL;

char localhost_str[] = "localhost";
char *mosix_host = NULL;

// The ctl targets 
#define CTL_TRG_INFOD_INFO           (1)
#define CTL_TRG_INFOD_CTL            (2)

// Ctl operations
typedef enum {
	CTLOP_INFOD_STATUS = 1,           
	CTLOP_INFOD_DEBUG,            
	CTLOP_INFOD_QUIET,            
	CTLOP_INFOD_NO_QUIET,          
	CTLOP_INFOD_AVGAGE,
	CTLOP_INFOD_NO_AVGAGE,
	CTLOP_INFOD_WIN_FIX,
	CTLOP_INFOD_WIN_UPTOAGE,
	CTLOP_INFOD_AVGWIN,
	CTLOP_INFOD_NO_AVGWIN,
	CTLOP_INFOD_INFO_MSGS,
	CTLOP_INFOD_NO_INFO_MSGS,
	CTLOP_INFOD_SEND_TIME,
	CTLOP_INFOD_NO_SEND_TIME,
	CTLOP_INFOD_GOSSIP_ALGO,
	CTLOP_INFOD_DLC,   // Death log clear
	CTLOP_INFOD_DLP,   // Death log print
} CTLOP_ID;

char *cdesc[] =
{
	"Extra migration time per dirty page",
};

#define	iabs(x)	((x) >= 0 ? (x) : -(x))

typedef struct ctl_action_ {
    int ctl_id;          // id of action
    char *ctl_str;
    int ctl_target;      // to use kernel, infod, or both
    void *ctl_argp;      // pointer to the ctl arguments
    int ctl_arg_len;     // size of memory
    int ctl_res;         // result of ctl action 
    int ctl_res2;        // result 2 if needed
    void *ctl_resp;      // result pointer
    int ctl_res_len;     // result pointer len 
} ctl_action_t;

struct coms
{
	char *com;
	char no;
	int  type;
} coms[] =
{
	{"debug",     CTLOP_INFOD_DEBUG,       CTL_TRG_INFOD_CTL}, 
	{"status",    CTLOP_INFOD_STATUS,      CTL_TRG_INFOD_CTL},
	{"avgage",    CTLOP_INFOD_AVGAGE,      CTL_TRG_INFOD_CTL},
	{"no-avgage", CTLOP_INFOD_NO_AVGAGE,   CTL_TRG_INFOD_CTL},
	{"gossip",    CTLOP_INFOD_GOSSIP_ALGO, CTL_TRG_INFOD_CTL},
	{"fixwin",    CTLOP_INFOD_WIN_FIX,     CTL_TRG_INFOD_CTL},
	{"uptoage",   CTLOP_INFOD_WIN_UPTOAGE, CTL_TRG_INFOD_CTL},
	{"quiet",     CTLOP_INFOD_QUIET,       CTL_TRG_INFOD_CTL},
	{"noquiet",   CTLOP_INFOD_NO_QUIET,    CTL_TRG_INFOD_CTL},
	{"avgwin",    CTLOP_INFOD_AVGWIN,      CTL_TRG_INFOD_CTL},
	{"no-avgwin", CTLOP_INFOD_NO_AVGWIN,   CTL_TRG_INFOD_CTL},
	{"infomsgs",  CTLOP_INFOD_INFO_MSGS,   CTL_TRG_INFOD_CTL},
	{"no-infomsgs",  CTLOP_INFOD_NO_INFO_MSGS,   CTL_TRG_INFOD_CTL},
	{"sendtime",  CTLOP_INFOD_SEND_TIME,   CTL_TRG_INFOD_CTL},
	{"no-sendtime",  CTLOP_INFOD_NO_SEND_TIME,   CTL_TRG_INFOD_CTL},
	{"dlc",       CTLOP_INFOD_DLC,         CTL_TRG_INFOD_CTL},
	{"dlp",       CTLOP_INFOD_DLP,         CTL_TRG_INFOD_CTL},
	{(char *)0,         0,              0},
};


int show_copyright() {
     printf("infod-ctl version " GOSSIMON_VERSION_STR "\n" GOSSIMON_COPYRIGHT_STR "\n\n");
     exit(0);
}


void usage()
{
	fprintf(stderr,
                "%1$s version " GOSSIMON_VERSION_STR "\n"
                "\n"
                
		"Usage: %1$s command\n"
                "       %1$s --help\n"
                "       %1$s --copyright\n"
                "\n"
                "Commands:\n"
		"  debug              [debug levels]\n"
	  	"  status             Show infod status information\n"
		"  gossip <number> \n"
		"                     Set the gossip algorithm used by infod to the\n"
		"                     algorithm represented by the given positive number\n"
		"  fixwin <size>      Tell infod to use a fixed window\n"
		"  uptoage <age>      Tell infod send only entries which are up to age old\n"
		"\n"
		"Measurments:\n"
		"  avgage <samples num>\n"
		"                     Start average age measurment\n"
		"  no-avgage          Stop average age measurment\n"
		"  avgwin <samples num>\n"
		"                     Start average window size measurement\n"
		"  no-avgwin          Stop average window size measurement\n"
		"\n"
		"  infomsgs           Start measuring the number of info messages\n"
		"                     recieved per second\n"
		"  no-infomsgs        Stopn info messages measurment\n"
		"  sendtime <samples num>\n"
		"                     Start measuring the average send time\n"
		"  no-sendtime        Stop measuring send time\n"
		"  dlc                Death log clear\n"
		"  dlp                Death log print\n"
		,program_name);
	exit(1);
}


int infod_do_ctl(int na, char **argv, ctl_action_t *act);


int main(int na, char **argv)
{
	struct coms *c;
	char *x;
	ctl_action_t act;

	msx_read_debug_file(NULL);
	
	program_name = argv[0];
	if((x = strrchr(argv[0], '/')))
		program_name = x+1;
	
	if( na < 2 || na > 10 )
	    usage();
	
        for(int i=0 ; i < na ; i++) {
          if(strcmp(argv[i], "--help") == 0) {
               usage();
          }
          if(strcmp(argv[i], "--copyright") == 0) {
               show_copyright();
          }
        }


	for( x = argv[1] ; *x ; x++)
		if( *x >= 'A' && *x <= 'Z')
			*x += 'a' - 'A';
	for( c = coms ; c->com ; c++ ) 
		if(!strcmp(c->com, argv[1])) 
			break;
	
	if(!c->com) {
		printf("%s: No such option.\n", argv[0]);
		exit(1);
	}

	// Initilazing the act structure
	act.ctl_id = c->no;
	act.ctl_str = strdup(c->com);
	act.ctl_target = c->type;
	act.ctl_resp = glob_res_buff;
	act.ctl_res_len = GLOB_RES_BUFF_SIZE;
	act.ctl_argp = glob_arg_buff;
	act.ctl_arg_len = GLOB_ARG_BUFF_SIZE;
	
	// Performing ctl action
	if( c->type & CTL_TRG_INFOD_CTL) {
		if(!infod_do_ctl(na, argv, &act))
			exit(1);
	}
	else {
		printf("Only infod ctl supported\n");
		exit(1);
	}
	return(0);
}



int parse_infod_debug_ctl(int args, char **debug_str)
{
	int debug = 0;
	int i;
	if(!debug_str)
		return 0;

	// If we get a hex number 0x we give it as is to the infod
	if(args == 1 && strlen(debug_str[0]) >= 3 &&
	   debug_str[0][0] == '0' && debug_str[0][0] == 'x')
	{
		sscanf(debug_str[0], "%x", &debug);
		printf("Got hex debug %d %x\n", debug, debug);
		return debug;
	}

	// Moving lower case to upper case (so we can give debug also
	// in lower case
	
	for(i=0 ; i<args ; i++)
	{
		int k;
		for(k=0 ; k < strlen(debug_str[i]) ; k++) 
			debug_str[i][k] = toupper(debug_str[i][k]);
		
		if(!strcmp(debug_str[i], "CTL"))
			debug |= CTL_DEBUG;
		else if(!strcmp(debug_str[i], "KCOMM"))
			debug |= KCOMM_DEBUG;
		else if(!strcmp(debug_str[i], "GINFOD"))
			debug |= GINFOD_DEBUG;
		else if(!strcmp(debug_str[i], "INFOD"))
			debug |= INFOD_DEBUG;
		else if(!strcmp(debug_str[i], "VEC"))
			debug |= VEC_DEBUG;
		else if(!strcmp(debug_str[i], "MAP"))
			debug |= MAP_DEBUG;
		else if(!strcmp(debug_str[i], "COMM"))
			debug |= COMM_DEBUG;
		else if(!strcmp(debug_str[i], "CLEAR"))
		        debug |= CLEAR_DEBUG;
		//else if(!strcmp(debug_str[i], "INFOLIB"))
		//debug |= INFOLIB_DEBUG;
		
		else
		{
			fprintf(stderr, "%s unknown debug mode", debug_str[i]);
			return 0;
		}
	}
	return debug;
}

int is_empty_line(char *str)
{
    int len, i;
    if(!str)
	return 0;
    
    len = strlen(str);
    for(i=0 ; i<len ; i++)
	if(!isspace(str[i]))
	    return 0;
    return 1;
}

int is_int(char *str)
{
    int len, i;
    if(!str || !(len = strlen(str)))
	return 0;

    for(i=0 ; i<len ; i++)
	if(!isdigit(str[i]))
	    return 0;
    return 1;
}

int handleAvgAgeMeasurement(ctl_action_t *act, int na, char **argv,
			   infod_ctl_hdr_t *msg)
{
	int samplesNum = 0;
	
	if(na >= 3)
		samplesNum = atoi( argv[2] );
	else
		samplesNum = 0;
	
	if(act->ctl_id == CTLOP_INFOD_NO_AVGAGE)
		samplesNum = -1;
	
	msg->ctl_type = INFOD_CTL_MEASURE_AVG_AGE;
	msg->ctl_arg1 = samplesNum;
	msg->ctl_arg2 = 0;
	msg->ctl_datalen = 0;
	msg->ctl_data = NULL;
	return 1;
}


int handleAvgWinMeasurement(ctl_action_t *act, int na, char **argv,
			   infod_ctl_hdr_t *msg)
{
	int samplesNum = 0;
	
	if(na >= 3)
		samplesNum = atoi( argv[2] );
	else
		samplesNum = 0;
	
	if(act->ctl_id == CTLOP_INFOD_NO_AVGWIN)
		samplesNum = -1;
	
	msg->ctl_type = INFOD_CTL_WIN_SIZE_MEASURE;
	msg->ctl_arg1 = samplesNum;
	msg->ctl_arg2 = 0;
	msg->ctl_datalen = 0;
	msg->ctl_data = NULL;
	return 1;
}

int handleInfoMsgsMeasurement(ctl_action_t *act, int na, char **argv,
			      infod_ctl_hdr_t *msg)
{
	int samplesNum = 0;
	
	if(na >= 3)
		samplesNum = atoi( argv[2] );
	else
		samplesNum = 0;
	
	if(act->ctl_id == CTLOP_INFOD_NO_INFO_MSGS)
		samplesNum = -1;
	
	msg->ctl_type = INFOD_CTL_MEASURE_INFO_MSGS;
	msg->ctl_arg1 = samplesNum;
	msg->ctl_arg2 = 0;
	msg->ctl_datalen = 0;
	msg->ctl_data = NULL;
	return 1;
}

int handleSendTimeMeasurement(ctl_action_t *act, int na, char **argv,
			      infod_ctl_hdr_t *msg)
{
	int samplesNum = 0;
	
	if(na >= 3)
		samplesNum = atoi( argv[2] );
	else
		samplesNum = 0;
	
	if(act->ctl_id == CTLOP_INFOD_NO_SEND_TIME)
		samplesNum = -1;
	
	msg->ctl_type = INFOD_CTL_MEASURE_SEND_TIME;
	msg->ctl_arg1 = samplesNum;
	msg->ctl_arg2 = 0;
	msg->ctl_datalen = 0;
	msg->ctl_data = NULL;
	return 1;
}

/*********************************************************************
 * Sending infod a request to change gossip algorithm
 ********************************************************************/
int handleGossipAlgo(ctl_action_t *act, int na, char **argv,
		     infod_ctl_hdr_t *msg)
{
	int gossipAlgoIndex;

 	if(na >= 3)
		gossipAlgoIndex = atoi( argv[2] );
	else {
		fprintf(stderr, "No parameter was given for the gossip algorithm number\n");
		return 0;
	}
	
	msg->ctl_type = INFOD_CTL_GOSSIP_ALGO;
	msg->ctl_arg1 = gossipAlgoIndex;
	msg->ctl_arg2 = 0;
	msg->ctl_datalen = 0;
	msg->ctl_data = NULL;
	return 1;
}


int handleDLC(ctl_action_t *act, int na, char **argv,
		     infod_ctl_hdr_t *msg)
{
	msg->ctl_type = INFOD_CTL_DEATH_LOG_CLEAR;
	msg->ctl_arg1 = 0;
	msg->ctl_arg2 = 0;
	msg->ctl_datalen = 0;
	msg->ctl_data = NULL;
	return 1;
}

int handleDLP(ctl_action_t *act, int na, char **argv,
	      infod_ctl_hdr_t *msg)
{
	char *buff;
	int   res;
	
	if(!(buff = malloc(4096)))
		return 0;
	
	msg->ctl_type = INFOD_CTL_DEATH_LOG_GET;
	msg->ctl_datalen = 0;
	msg->ctl_res = buff;
	msg->ctl_reslen = 4096;
	if((res = infod_ctl(-1, msg )) == -1)
	{
		fprintf(stderr, "Error connecting infod\n");
		free(buff);
		return 0;
	}
	printf("%s", (char*)msg->ctl_res);
	free(buff);
	return 1;
}

int handleWindowParameters(ctl_action_t *act, int na, char **argv,
			   infod_ctl_hdr_t *msg)
{
	int winType, winTypeParam;

	winType = act->ctl_id == CTLOP_INFOD_WIN_FIX ? 0 : 1;

 	if(na >= 3)
		winTypeParam = atoi( argv[2] );
	else {
		fprintf(stderr, "no parameter was given for the window type\n");
		return 0;
	}
	
	msg->ctl_type = INFOD_CTL_WIN_TYPE;
	msg->ctl_arg1 = winType;
	msg->ctl_arg2 = winTypeParam;
	msg->ctl_datalen = 0;
	msg->ctl_data = NULL;
	return 1;
}



/*
 * Handling ctl function which need infod (using infodctl interface)
 *
 */
int infod_do_ctl(int na, char **argv, ctl_action_t *act)
{
	infod_ctl_hdr_t msg;
	int res;
	int debug_mode;
	int samplesNum = 0;
	//printf("Contacting infod and telling it to  %d\n", cmd);

	uid_t euid;

	euid = geteuid();
	if(euid != 0) {
		fprintf(stderr, "Error: user id is not 0\n");
		return 0;
	}
	
	msg.ctl_name = MSX_INFOD_CTL_SOCK_NAME;
	msg.ctl_namelen = strlen(msg.ctl_name);
	msg.ctl_res = NULL;
	msg.ctl_reslen = 0;
	msg.ctl_data = NULL;
	msg.ctl_datalen = 0;
	
	switch(act->ctl_id)
	{
	    case CTLOP_INFOD_QUIET:
	    case CTLOP_INFOD_NO_QUIET:
	    {
	    
		printf("Contacting Infod telling it to go %s\n",
		       act->ctl_id == CTLOP_INFOD_QUIET ? "Quite" : "No Quiet");
		msg.ctl_type = act->ctl_id  == CTLOP_INFOD_QUIET ? INFOD_CTL_QUIET : INFOD_CTL_NOQUIET;
	    }
	    break;
	    case CTLOP_INFOD_DEBUG:
	    {
		    if(na >= 3)
			    debug_mode = parse_infod_debug_ctl(na - 2, &argv[2]);
		    else
			debug_mode = 0;
		    msg.ctl_type = INFOD_CTL_SET_DEBUG;
		    msg.ctl_datalen = sizeof(int);
		    msg.ctl_data = &debug_mode;
	    }
	    break;
	    case CTLOP_INFOD_STATUS:
	    {
		char *buff;

		if(!(buff = malloc(4096)))
		    return 0;
		
		msg.ctl_type = INFOD_CTL_GET_STATUS;
		msg.ctl_datalen = 0;
		msg.ctl_res = buff;
		msg.ctl_reslen = 4096;
		if((res = infod_ctl(-1, &msg )) == -1)
		{
		    fprintf(stderr, "Error connecting infod\n");
		    free(buff);
		    return 0;
		}
		printf("%s", (char*)msg.ctl_res);
		free(buff);
		return 1;
	    }
	    break;
	    case CTLOP_INFOD_GOSSIP_ALGO:
		    if(!handleGossipAlgo(act, na, argv, &msg))
			    return 0;
		    break;
	    case CTLOP_INFOD_WIN_FIX:
	    case CTLOP_INFOD_WIN_UPTOAGE:
		    if(!handleWindowParameters(act, na, argv, &msg))
			    return 0;
		    break;

	    case CTLOP_INFOD_AVGAGE:
	    case CTLOP_INFOD_NO_AVGAGE:
		    if(!handleAvgAgeMeasurement(act, na, argv, &msg))
			    return 0;
		    break;
	    case CTLOP_INFOD_AVGWIN:
	    case CTLOP_INFOD_NO_AVGWIN:
		    if(!handleAvgWinMeasurement(act, na, argv, &msg))
			    return 0;
		    break;
	    case CTLOP_INFOD_INFO_MSGS:
	    case CTLOP_INFOD_NO_INFO_MSGS:
		    if(!handleInfoMsgsMeasurement(act, na, argv, &msg))
			    return 0;
		    break;
	    
	    case CTLOP_INFOD_SEND_TIME:
	    case CTLOP_INFOD_NO_SEND_TIME:
		    if(!handleSendTimeMeasurement(act, na, argv, &msg))
			    return 0;
		    break;
	    case CTLOP_INFOD_DLC:
		    if(!handleDLC(act, na, argv, &msg))
			    return 0;
		    break;
	    case CTLOP_INFOD_DLP:
		    if(!handleDLP(act, na, argv, &msg))
			    return 0;
		    break;
	    default:
		    fprintf(stderr, "Unknown infod ctl\n");
		    return 0;
	}
	
	if((act->ctl_res = infod_ctl(-1, &msg )) == -1)
	{
		fprintf(stderr, "Error infod is not fully running\n");
		return 0;
	}
	return 1;
}








