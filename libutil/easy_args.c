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
 * Author(s): Shlomo Matichin, Amar Lior
 *
 *****************************************************************************/


// This is a library written by shlomo that make the task of parsing
// programs arguments much more easier.

#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>

#include <string.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "easy_args.h"

char *easy_args_env_prefix = NULL;

struct full_args_found
{
    char             found;
    struct argument  *a;
}  *full_args_found;

static int parse_size( char *st , int *size ) {
	int	length	= strlen(st);
	char	sizet;

	if( length == 0 ) return 1;
	sizet = st[length-1];
	switch( sizet ) {
	    case 'B': case 'b': case 'K': case 'k': case 'M': case 'm':
	    case 'G':
		st[length-1] = '\0';
	}
	if( sscanf( st , "%d" , size ) < 1 ) return 2;
	
	switch( sizet ) {
	    case 'K': case 'k':	*size *= KILOBYTE; break;
	    case 'M': case 'm':	*size *= MEGABYTE; break;
	    case 'G': *size = KILOBYTE * MEGABYTE; break;
	}
	return 0;
}

static int parse_ip( char *st , unsigned long *ip ) {
	char	*t;
	
	t = strchr( st , '.' );
	if( ! t ) return 1;
	t = strchr( t+1 , '.' );
	if( ! t ) return 1;
	t = strchr( t+1 , '.' );
	if( ! t ) return 1;
	if( ! inet_aton( st , (struct in_addr *)ip ) )
		return 1;
	return 0;
}

void parse_argument( struct argument *argument , char *optarg , char *argument_name );

int parse_command_line( int argc , char **argv , struct argument original_arguments [] ) {
	struct argument		*arguments						= original_arguments;
	struct option		long_options			[MAX_OPTIONS];
	struct argument		*long_options_arguments		[MAX_OPTIONS];
	int			long_options_count					= 0;
	char			short_options			[2*MAX_OPTIONS + 1];
	struct argument		*short_options_arguments	[2*MAX_OPTIONS];
	int			short_options_count					= 0;
	struct argument		*unnamed_options_arguments	[MAX_OPTIONS]		= { NULL };
	int			unnamed_options_count					= 0;
	char			temp_name[128]						= "a";
	char			*argument_name;
	struct argument		*usage							= NULL;
	int			opt_index;
	int			ret;
	struct argument		*argument						= NULL;
	int			i;

	int                     args_num = 0;

	if(!arguments)
	    goto PROGRAMMER_ERROR;
	
	for(; arguments->type != 0 ; arguments++ ) {
		switch( (arguments->type) & ARGUMENT_TYPES ) {
			case ARGUMENT_USAGE_HANDLER:
				if( arguments->type & ~ARGUMENT_USAGE_HANDLER )
					goto PROGRAMMER_ERROR;
				arguments->type |= ARGUMENT_FLAG;
				usage = arguments;
				long_options_arguments[long_options_count] = arguments;
				long_options[long_options_count].name = "usage";
				long_options[long_options_count].has_arg = 0;
				long_options[long_options_count].flag = 0;
				long_options[long_options_count].val = long_options_count +
					BASE_NUMBER_FOR_LONG_OPTIONS;
				long_options_count++;
				long_options_arguments[long_options_count] = arguments;
				long_options[long_options_count].name = "help";
				long_options[long_options_count].has_arg = 0;
				long_options[long_options_count].flag = 0;
				long_options[long_options_count].val = long_options_count +
					BASE_NUMBER_FOR_LONG_OPTIONS;
				long_options_count++;
				break;
			case ARGUMENT_FLAG:
			case ARGUMENT_STRING:
			case ARGUMENT_NUMERICAL:
			case ARGUMENT_BYTESIZE:
			case ARGUMENT_DOUBLE:
			case ARGUMENT_IP:
				if( ! ( (  (arguments->type & ARGUMENT_BOTH) && 
				          !(arguments->type & ARGUMENT_UNNAMED) ) ||
				        ( !(arguments->type & ARGUMENT_BOTH) &&
				           (arguments->type & ARGUMENT_UNNAMED) ) ) )
					goto PROGRAMMER_ERROR;
				if( (arguments->type & ARGUMENT_OPTIONAL) && (arguments->type & ARGUMENT_VARIABLE) &&
					! (arguments->type & ARGUMENT_NUMERICAL) )
					goto PROGRAMMER_ERROR;
				if( arguments->type & ARGUMENT_FULL ) {
					long_options_arguments[long_options_count] = arguments;
					long_options[long_options_count].name = arguments->long_name;
					if( arguments->type & ARGUMENT_NEED_PARAMETER )
						long_options[long_options_count].has_arg = 
							(arguments->type & ARGUMENT_OPTIONAL)?2:1 ;
					long_options[long_options_count].flag = 0;
					long_options[long_options_count].val = long_options_count +
						BASE_NUMBER_FOR_LONG_OPTIONS;
					long_options_count++;
					args_num++;
				}
				if( arguments->type & ARGUMENT_SHORT ) {
					short_options_arguments[short_options_count] = arguments;
					short_options[short_options_count++] = arguments->short_name;
					if( arguments->type & ARGUMENT_NEED_PARAMETER ) {
						short_options[short_options_count++] = ':';
						if( arguments->type & ARGUMENT_OPTIONAL )
							short_options[short_options_count++] = ':';
					}
				}
				if( arguments->type & ARGUMENT_UNNAMED ) {
					if( unnamed_options_count<arguments->short_name )
						unnamed_options_count = arguments->short_name;
					if( unnamed_options_arguments[ (int)arguments->short_name ] != NULL )
						goto PROGRAMMER_ERROR2;
					unnamed_options_arguments[ (int)arguments->short_name ] = arguments;
				}
				break;
			default:
				goto PROGRAMMER_ERROR;
		}
	}

	//fprintf(stderr, "number of long args are %d\n", args_num);

	// Building the full_args_found data structure
	if(!(full_args_found =
	     malloc(args_num * sizeof(struct full_args_found))))
	{
	    fprintf(stderr, "Not enough memory\n");
	    exit( COMMAND_LINE_PARSING_ERROR_CODE);
	}
	bzero(full_args_found, args_num * sizeof(struct full_args_found));
	{
	    int full_pos = 0;
	    struct argument *aa;
	    for(aa = original_arguments ; aa->type != 0 ; aa++)
	    {
		if(aa->type & ARGUMENT_FULL)
		{
		    full_args_found[full_pos].a = aa;
		    full_pos++;
		}
	    }

	    //for(kk=0 ; kk < args_num ; kk++)
	    //fprintf(stderr, "Full pos %d name %s\n", kk, full_args_found[kk].a->long_name);
	}
	
	
	
	long_options[long_options_count].name = NULL;
	long_options[long_options_count].has_arg = 0;
	long_options[long_options_count].flag = 0;
	long_options[long_options_count].val = 0;
	short_options[short_options_count] = '\0';
	for( i=1 ; i<=unnamed_options_count ; i++ )
		if( ! unnamed_options_arguments[ i ] )
			goto PROGRAMMER_ERROR3;

	while(1) {
		ret = getopt_long( argc , argv , short_options , long_options , &opt_index );
		switch(ret) {
			case -1:
				goto PARSE_UNNAMED_ARGUMENTS;

			case '?':
				if( usage )
					((argument_handler *)usage->handler)(0);
				else
					fprintf(stderr, "Usage is incorrect (Usage message unavailable)\n");
				exit( COMMAND_LINE_PARSING_ERROR_CODE);	

			case ':':
				if( optopt )
					fprintf( stderr, "Argument `%c' is missing a parameter\n" , optopt );
				else
					fprintf( stderr, "Arguemtn `%s' is missing a parameter\n" ,
						long_options[opt_index].name );
				exit( COMMAND_LINE_PARSING_ERROR_CODE );

			default:
				if( (ret>=BASE_NUMBER_FOR_LONG_OPTIONS) &&
				    (ret<(BASE_NUMBER_FOR_LONG_OPTIONS + long_options_count)) )
					argument = long_options_arguments[opt_index];
				if( (ret<255) && strchr( short_options , ret ) )
					argument = short_options_arguments[ strchr( short_options , ret )
							- short_options ];
				if( ! argument ) {
					fprintf(stderr , "Error: Getopt returned unknown value.\n" );
					exit( COMMAND_LINE_PARSING_ERROR_CODE );
				}
				if( ret < 255 ) {
					temp_name[0] = argument->short_name;
					argument_name = temp_name;
				} else 
					argument_name = argument->long_name;

			      
				parse_argument( argument , optarg , argument_name );
				// After parsing the argument we check if it was a full one and if so we mark it
				// so we wont check if it exsits as an environment variable

				if(argument->type & ARGUMENT_FULL)
				{
				    int full_pos = 0;
				    struct argument *aa;
				    //fprintf(stderr, "We have full %s\n", argument_name);
				    // we search it 
				    
				    for(aa = original_arguments ; aa->type != 0 ; aa++)
				    {
					    
					if(aa->long_name && (!strcmp(aa->long_name, argument_name)))
					{
					    //fprintf(stderr, "Found it at pos %d\n", full_pos);
					    full_args_found[full_pos].found = 1;
					}
					if(aa->type & ARGUMENT_FULL)
					    full_pos++;
				    }
				}
				break;
		}
	}


 PARSE_UNNAMED_ARGUMENTS:
	{
	    // Finding the full arguments that was not in the command line ad
	    // searching them in the environment

	    char env_name[256];
	    char upper_name[256];
	    char *env_val;
	    
	    int kk, ii;
	    for(kk=0 ; kk < args_num ; kk++)
	    {
		//fprintf(stderr, "@@@@@ %d\n", full_args_found[kk].found);
		if(!full_args_found[kk].found)
		{
		    for(ii=0; ii< strlen(full_args_found[kk].a->long_name) ; ii++)
			upper_name[ii] = toupper(full_args_found[kk].a->long_name[ii]);
		    upper_name[ii] = '\0';
		    sprintf(env_name, "%s_%s", 
			    easy_args_env_prefix, upper_name);
		    env_val = getenv(env_name);
		    if(env_val)
			parse_argument( full_args_found[kk].a ,
					env_val,
					full_args_found[kk].a->long_name);
		    
		}
	    }
	}


	for( i=1 ; i<=unnamed_options_count ; optind++,i++ ) {
		if( optind >= argc ) return i-1;
		sprintf( temp_name , "Unnamed argument %d" , i );
		parse_argument( unnamed_options_arguments[ i ] , argv[ optind ] , temp_name );
	}
	if( optind < argc ) {
		fprintf( stderr , "Too many unnamed arguments\n" );
		((argument_handler *)usage->handler)(0);
		exit( COMMAND_LINE_PARSING_ERROR_CODE );
	}
	return i-1;

	PROGRAMMER_ERROR:
		fprintf(stderr,
			"Error: This program has given invalid information to it's"
			" command line parsing module. Please contact the "
			"programmer with this information:\n"
			"handler %d has an invalid argument type %x\n" , 
			(int)(arguments-original_arguments), arguments->type );
		exit(COMMAND_LINE_PARSING_ERROR_CODE);
	PROGRAMMER_ERROR2:
		fprintf(stderr,
			"Error: This program has given invalid information to it's"
			" command line parsing module. Please contact the "
			"programmer with this information:\n"
			"handler %d is a double %d unnamed argument\n",
			(int)(arguments-original_arguments) , argument->short_name );
		exit(COMMAND_LINE_PARSING_ERROR_CODE);
	PROGRAMMER_ERROR3:
		fprintf(stderr,
			"Error: This program has given invalid information to it's"
			" command line parsing module. Please contact the "
			"programmer with this information:\n"
			"Unnamed argument %d is missing\n",
			i );
		exit(COMMAND_LINE_PARSING_ERROR_CODE);
}

void parse_argument( struct argument *argument , char *optarg , char *argument_name )
{
	argument_handler	*argument_handler_func;
	int			temp_int;
	double			temp_double;
	int			temp_bytesize;
	unsigned long		temp_ip;


	//	fprintf(stderr, "Got arg %s ## %s ## \n", argument_name, optarg);

	argument_handler_func = (argument_handler *) argument->handler;
	if( (argument->type & ARGUMENT_OPTIONAL) && ! optarg ) {
		if( argument->type & ARGUMENT_VARIABLE )
			*(int *)argument->handler = 1;
		else if( argument_handler_func( NULL ) )
			exit( COMMAND_LINE_PARSING_ERROR_CODE );
		return;
	}
	if( ((argument->type & ARGUMENT_VARIABLE)!=0) &&(argument->handler==NULL) ) {
		fprintf(stderr , "Error: Argument %s repeats itslef more than once\n",
			argument_name);
		exit( COMMAND_LINE_PARSING_ERROR_CODE );
	}
	switch( argument->type & (ARGUMENT_NEED_PARAMETER | ARGUMENT_FLAG) ) {
		case ARGUMENT_FLAG:
			if( argument->type & ARGUMENT_VARIABLE )
				*(int *)argument->handler = 1;
			else if( argument_handler_func( NULL ) )
				exit( COMMAND_LINE_PARSING_ERROR_CODE );
			break;
		case ARGUMENT_STRING:
			if( argument->type & ARGUMENT_VARIABLE )
				strcpy( (char *)argument->handler , optarg );
			else if( argument_handler_func( optarg ) )
				exit( COMMAND_LINE_PARSING_ERROR_CODE );
			break;
		case ARGUMENT_NUMERICAL:
			if( sscanf( optarg , "%i" , &temp_int ) < 1 ) {
				fprintf(stderr, "Argument `%s' requires an integer parameter, and `%s' is not.\n" ,
					argument_name , optarg );
				exit(COMMAND_LINE_PARSING_ERROR_CODE);
			}
			if( argument->type & ARGUMENT_VARIABLE )
				*(int *)argument->handler = temp_int;
			else if( argument_handler_func( &temp_int ) )
				exit(COMMAND_LINE_PARSING_ERROR_CODE);
			break;
		case ARGUMENT_BYTESIZE:
			if( parse_size( optarg , &temp_bytesize ) ) {
				fprintf(stderr, "`%s' is not a valid byte size!\n",
					optarg );
				exit(COMMAND_LINE_PARSING_ERROR_CODE);
			}
			if( argument->type & ARGUMENT_VARIABLE )
				*(int *)argument->handler = temp_bytesize;
			else if( argument_handler_func( &temp_bytesize ) )
				exit(COMMAND_LINE_PARSING_ERROR_CODE);
			break;
		case ARGUMENT_DOUBLE:
			if( sscanf( optarg , "%lf" , &temp_double ) < 1 ) {
				fprintf(stderr, "Argument `%s' requires a double"
					"parameter, and `%s' is not.\n" ,
					argument_name , optarg );
				exit(COMMAND_LINE_PARSING_ERROR_CODE);
			}
			if( argument->type & ARGUMENT_VARIABLE)
				*(double *)argument->handler = temp_double;
			else if( argument_handler_func( &temp_double ) )
				exit(COMMAND_LINE_PARSING_ERROR_CODE);
			break;
		case ARGUMENT_IP:
			if( parse_ip( optarg , &temp_ip ) ) {
				fprintf(stderr , "`%s' is not a valid IP address!\n",
					optarg );
				exit(COMMAND_LINE_PARSING_ERROR_CODE);
			}
			if( argument->type & ARGUMENT_VARIABLE )
				*(unsigned long *)argument->handler = temp_ip;
			else if( argument_handler_func( &temp_ip ) )
				exit(COMMAND_LINE_PARSING_ERROR_CODE);
			break;
		default:
			fprintf(stderr , "Error in command line parsing module!\n"
				"Debugging information: argument_type: %x\n" , argument->type);
			exit(COMMAND_LINE_PARSING_ERROR_CODE);
	}
	if( argument->type & ARGUMENT_VARIABLE )
		argument->handler = NULL;
}



// Command line parsing using the easyargs
#if 0
int reserve_percent;
int reserve_number;
int sleep_time;
int unreserve_delay;
int debug;

int set_percentage( void *void_int ){
	reserve_percent =  *((int*) void_int);
	if(reserve_percent <= 0 )
	{
		fprintf(stderr, "Error, rp must be > 0\n");
		return 1;
	}
	return 0;
}

int set_number( void *void_int ){
	reserve_number = *((int*) void_int);
	if(reserve_number <= 0)
	{
		fprintf(stderr, "Error, rn must be > 0\n");
		return 1;
	}
	return 0;
}



int set_period( void *void_int ){
	sleep_time = *((int*) void_int);
	if(sleep_time <= 0)
	{
		fprintf(stderr, "Error, period must be > 0\n");
		return 1;
	}
	return 0;
}

int set_delay( void *void_int ){
	unreserve_delay = *((int*) void_int);
	if(unreserve_delay <= 0)
	{
		fprintf(stderr, "Error: Delay must by > 0\n");
		return 1;
	}
	return 0;
}

int set_debug( void *void_int) {
	printf("setdebug\n");
	debug = 1;
	return 0;
	
}

int
usage(void *void_str) {

	fprintf( stderr, 
		 "Usage: ownerd [OPTIONS]\n"
		 "\t--rp=[1..]            Reserved percentage\n"
		 "\t--rn=[1..]            Reserved number\n"
		 "\t--period=sec          Number of seconds to sleep\n"
		 "\t--delay=sec           Number of seconds to delay unblock\n"
		 "\t--debug               Do not become daemon and print debug messages\n"
		 
		 "\t--help                This message\n"
		 "\n"
		 "The --rp and --rn options can not coexist\n"



		);
	return 1;
}

struct argument args[] = {
	{ ARGUMENT_NUMERICAL | ARGUMENT_FULL, 0, "rp", set_percentage},
	{ ARGUMENT_NUMERICAL | ARGUMENT_FULL, 0, "rn", set_number},
	{ ARGUMENT_NUMERICAL | ARGUMENT_FULL, 0, "period", set_period},
	{ ARGUMENT_NUMERICAL | ARGUMENT_FULL, 0, "period", set_period},
	{ ARGUMENT_NUMERICAL | ARGUMENT_FULL, 0, "delay",  set_delay},
	{ ARGUMENT_FLAG   | ARGUMENT_FULL, 0, "debug", set_debug},
	{ ARGUMENT_FLAG   | ARGUMENT_FULL, 0, "machines", set_debug},

	{ ARGUMENT_FLAG   | ARGUMENT_FULL, 0, "help", usage},
	{ ARGUMENT_FLAG   | ARGUMENT_SHORT, 'm', NULL, usage},
	{ ARGUMENT_FLAG   | ARGUMENT_SHORT, 'h', NULL, usage},
	{ ARGUMENT_USAGE_HANDLER, 0, "", usage},
	{ 0, 0, 0, 0},
};



int main(int argc, char **argv)
{
	// Parsing command line
	parse_command_line( argc, argv, args );

	return 0;
}
#endif 
