/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


/****************************************************************************
 *
 * File: test_load.c 
 *
 ***************************************************************************/

#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>    
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <msx_common.h>
#include <gossimon_config.h>

#ifdef MPI_SUPPORT
#include "mpi.h"
#endif

/****************************************************************************
 * Data structures 
 ***************************************************************************/

#define KB1           (1024)
#define MB1           (KB1*KB1)
#define DEF_TIME      (1800)
#define MAX_MIG_NUM   (1024)
#define BUFF_SZ       (512)
#define MICRO         (1000000)

/*
 * The possible tests
 */
typedef enum {
		 
		CPU_ONLY,           /* Only cpu                      */
		IO_READ,            /* Test read from file           */
		IO_WRITE,           /* Test write from file          */
		NONIO_SYSCAL,       /* Test with non IO system calls */
		CHKPT_DEMO          /* Run a demo for checkpoint     */
		
} test_type_t;


/*
 * The process profile
 */
typedef struct proc_profile {
	
	/* The amount of virtual time to run in sec */
	int            pp_time;
	
	/* The amount of memory to allocate*/
	int            pp_mem;

	// Fill memory with a random pattern
	char           pp_random_mem;
	/* The file to use in case of IO */
	char          *pp_filename;
	
	/* The file descriptor of the file ued */
	int            pp_iofd;
	
	/* A file handle to the file used for statistics and reporting */
	FILE          *pp_output;
	
	/* Number of basics loops to do every time */
	int            pp_cpu_units;
	
	/* Number of times to perfrom system calls chunks */
	int            pp_syscall_times;
	
	/* Number of loops in a system calls chunk */
	int            pp_syscall_per_chunk;
	
	/* Type of test to perfrom */
	test_type_t    pp_type;
	
	/* Type string */
	char           *pp_type_str;
	
	/* Buffer used for IO */
	char           *pp_io_buff;
	
	/* Size of IO buffer, in bytes */
	off_t           pp_io_buff_size;
	
	/* The amount of IO perfromed */
	off_t  pp_io_done_size;
	
	/* The maximum IO size to perfrom */
	off_t  pp_max_io_size;
	
	/* Counts the number of system calls perfromed */
	int            pp_syscall_num;
	
	/* Exit only when process running in its home */
	int            pp_exit_inhome;
	
	/* Number of migration */
	int            pp_mig_num;
	
	/* Report each migrations */
	int            pp_onmig_report;
	
	/* Track the migration */
	char           *pp_mig_arr[ MAX_MIG_NUM ];

	/* Print verbose information while running (for debugging) */
	char           pp_verbose;

	/* Checkpoint stuff */
	int            pp_self_chkpt;
	char          *pp_chkpt_file;
} proc_profile_t;


/*
 * The parameters to the program 
 */
typedef struct param
{
	int       time; 	 /* Time to run in sec */
	int       tlow;          /* Upper and lower bounds on the run time */
	int       thigh;
	int       mem; 	         /* The maount of memory to use in MB */
	int       random_mem;
	int       nproc;         /* Number of instances to run */
	int       curr_proc; 	 /* UNUSED */
	int       job_time;      /* UNUSED */
	char     *io_dir;        /* The directory that would hold the io file */
	char     *io_file; 	 /* The name of the IO file */

	off_t     do_read; 	 /* Perfrom read from the file */
	off_t     do_write; 	 /* Perfrom write to files */
 	int       do_nonio_syscall; 	/* Do nonio system calls - time*/
	int       do_chkpt_demo;   // Do checkpoint demo

	off_t     max_io_size; 	 /* The maixmum size of IO file in bytes  */
	off_t     io_size; 	 /* The size of IO total IO operations in bytes */
	off_t     syscall_chunk; /* The number of times to call a system call in each chunk */
	off_t     syscall_times; /* The number of chunks to perform */
	int       cpu_unit;      /* Determines the amount of work to do between syscall chunks */

	int       catch_sigmig; /* Catch sigmig */
	int       report_mig; 	/* Report migration operations */
	int       sleep_sec; 	/* Sleep before starting the test */
	
	/* Report results */
	int      report;
	
	/* Report on every migration */
	int      onmig_report;
	int      verbose;
	
	/* Exit when back at home */
	int      exit_inhome;
	
	/* The name of te output file */
	char     *output_file_name;
	
	/* Change the user id */
	int      change_uid;

	/* Checkpoint stuff */
	int      self_chkpt;
	char    *chkpt_file;
	
} prog_param_t;

char            *glob_prog_name;
volatile int    glob_got_sig_alarm;
volatile int    glob_do_exit = 0;
int             glob_verbos = 0;
proc_profile_t  *glob_profile;

/****************************************************************************
 * Usage
 ***************************************************************************/
void usage() {
	printf("%1$s version " GOSSIMON_VERSION_STR "\n"
               "Usage: %1$s [OPTIONS]\n"
	       "Create CPU processes for testing a MOSIX system\n"
	       "\n"
	       "OPTIONS:\n"
	       "-t, --time=TIME           virtual time to run (sec)\n"
	       "-h, --thigh=TIME          upper bound on the virtual time (sec)\n"
	       "-l, --tlow=TIME           lower bound on the virtual time (sec)\n"
	       "                          if thigh or tlow are given the \n"
	       "                          time to run is a random value  \n"
	       "                          between thigh and tlow         \n"
	       "-m, --mem=MEGABYTES       memory in megabytes to consume\n"
	       "    --random-mem          Fill memory with random pattern\n"
	       "\n"
               "I/O options:\n"
	       "-d, --dir=DIRNAME         Directory name for creating files\n"
	       "                          The file name is dir/tmp_file.pid\n"
	       "-f, --file=FILENAME       File name to use\n"
	       "    --read[=SIZE]         Perform each read system call in (SIZE)KB\n"
	       "    --write[=SIZE]        Perform each write system call in (SIZE)KB\n"
	       "    --noniosyscall=NUM     Perform non I/O syscall like time\n"
	       "    --iosize              Total amount of data to read/write in MB\n"
	       "    --maxiosize           Maximum size accessible in file in MB\n"
	       "    --iotimes=NUM         Number of chunks to read/write\n"
	       "    --iochunk=NUM         Size of I/O chunk that is read/written every cycle (KB)\n"
	       "    --cpu=SIZE            Amount of work to do for every read/write/time\n"    
               "\n"
               "Job options:\n"
               "-s, --jobsize=NUM         Number of processes to create in the job\n"
               "\n"
	       "Checkpoint options:\n"	       
	       "   --chkpt-demo           Do some computation and then print a number\n"
	       "   --self-chkpt=i         Perform self checkpoint after i units\n"
	       "   --chkpt-file=file      Use the given file name for the checkpoints\n"
               "\n"
               "Misc options:\n"
	       "    --report-mig          Report migrations when done to stdout\n"
	       "    --onmig-report        Report Time and location of migration\n"
	       "    --exit-inhome         Terminate when migrated back home\n"
	       "    --nosigmig            Do not catch sigmig and report about migrations\n"
	       "    --sleep=SEC           Sleep SEC number of seconds before starting\n"
	       "-r, --report              Report running times statistics\n"
	       "-v, --verbose             Print verbose information while running\n"
	       "-o, --output              Report to an alternative file (default stdin)\n"
	       "    --uid                 Only as root, run as a different uid\n"
               "    --copyright           Show copyright message\n"
               "    --help                Show this help screen\n"
	       "\n"
               "\n"
	       "If running time is not given testload runs for 1800 seconds\n"
	       "If memory size is not given processes consume no memory\n"
//	       "Time and Jobtime can be in seconds,minutes or hours\n"
//	       "If JOBTIME time has passed all processes are killed\n"
	       , glob_prog_name);
}

int show_copyright() {
  printf("%s version " GOSSIMON_VERSION_STR "\n" GOSSIMON_COPYRIGHT_STR "\n\n",
         glob_prog_name);
}

/****************************************************************************
 * Error function
 ***************************************************************************/
static void
test_load_error( char *str ) {
	fprintf( stderr, "Error: %s\n", str );
	exit( EXIT_FAILURE );
}

/****************************************************************************
 * Handle command line arguments 
 ***************************************************************************/
static void
parse_args(int argc, char **argv, prog_param_t *pp ) {

	int c;
		
	while( 1 ) {
				
		//int this_option_optind = optind ? optind : 1;
		int option_index = 0;

		static struct option long_options[] = {
			{"time",        1, 0, 't'},
			{"tlow",        1, 0, 'l'},
			{"thigh",       1, 0, 'h'},
			{"mem",         1, 0, 'm'},
			{"random-mem",  0, 0,  0 },
			{"dir",         1, 0, 'd'},
			{"file",        1, 0, 'f'},
                        {"help",        0, 0, 0},
                        {"read",        1, 0,  0 },
			{"write",       1, 0,  0 },
			{"noniosyscall",   1, 0,  0 },
			{"cpu",         1, 0,  0 },
			{"iotimes",     1, 0,  0 },
			{"iochunk",     1, 0,  0 },
			{"iosize",      1, 0,  0 },
			{"maxiosize",   1, 0,  0 },
			{"nosigmig",    0, 0,  0 },
			{"exit-inhome", 0, 0,  0 },
			{"sleep",       1, 0,  0 },
			{"report",      0, 0, 'r'},
			{"onmig-report",   0, 0,  0},
			{"verbose",     0, 0, 'v'},
			{"copyright",     0, 0, 0},
                        {"output",      1, 0, 'o'},
			{"jobsize",     1, 0, 's'},
			{"cursub",      1, 0, 'c'},
			{"jobtime",     1, 0,  0 },
			{"uid",         1, 0,  0 },
			{"chkpt-demo",  0, 0,  0 },
			{"chkpt-demo",  0, 0,  0 },
			{"self-chkpt",  1, 0,  0 },
			{"chkpt-file",  1, 0,  0 },
			{0, 0, 0, 0}
		};
	
		c = getopt_long (argc, argv, "t:h:l:m:s:c:d:f:o:rv",
				 long_options, &option_index);
		if (c == -1)
			break;
	
		switch (c) {
                case 0:
                  if(strcmp(long_options[option_index].name,
                            "copyright") == 0) {
                    show_copyright();
                    exit(0);
                  }
                  if(strcmp(long_options[option_index].name,
                            "help") == 0) {
                    usage();
                    exit(0);
                  }

                  
                  if(strcmp(long_options[option_index].name,
                            "jobtime") == 0)
                    pp->job_time = atoi(optarg);
                  
                  if(strcmp(long_options[option_index].name,
                            "read") == 0)
                    pp->do_read = (off_t)atoi(optarg) * KB1;
                  
                  if(strcmp(long_options[option_index].name,
                            "write") == 0)
                    pp->do_write = (off_t)atoi(optarg) * KB1;
                  
                  if(strcmp(long_options[option_index].name,
                            "noniosyscall") == 0)
                    pp->do_nonio_syscall = atoi(optarg);
                  
                  if(strcmp(long_options[option_index].name,
                            "iosize") == 0)
                    pp->io_size = (off_t)atoi(optarg) * MB1;
                  
                  if(strcmp(long_options[option_index].name,
                            "maxiosize") == 0) {
                    pp->max_io_size = (off_t)atoi(optarg);
                    pp->max_io_size *= MB1;
                  }
                  
                  if(strcmp(long_options[option_index].name,
                            "iochunk") == 0)
                    pp->syscall_chunk = (off_t)atoi(optarg) *
                      KB1;
                  
                  if(strcmp(long_options[option_index].name,
                            "iotimes") == 0)
                    pp->syscall_times = atoi(optarg);
                  
                  if(strcmp(long_options[option_index].name,
                            "cpu") == 0)
                    pp->cpu_unit = atoi(optarg);
                  
                  if(strcmp(long_options[option_index].name,
                            "sleep") == 0)
                    pp->sleep_sec = atoi(optarg);
                  
                  if(strcmp(long_options[option_index].name,
                            "nosigmig") == 0) 
                    pp->catch_sigmig = 0;
                  
                  if(strcmp(long_options[option_index].name,
                            "onmig-report") == 0)
                    pp->onmig_report = 1;
                  
                  if(strcmp(long_options[option_index].name,
                            "exit-inhome") == 0)
                    pp->exit_inhome = 1;
                  
                  if(strcmp(long_options[option_index].name,
                            "uid") == 0)
                    pp->change_uid = atoi(optarg);
                  if(strcmp(long_options[option_index].name,
                            "chkpt-demo") == 0)
                    pp->do_chkpt_demo = 1;
                  if(strcmp(long_options[option_index].name,
                            "self-chkpt") == 0)
                    pp->self_chkpt = atoi(optarg);
                  if(strcmp(long_options[option_index].name,
                            "chkpt-file") == 0)
                    pp->chkpt_file = strdup(optarg);
                  if(strcmp(long_options[option_index].name,
                            "random-mem") == 0) 
                    pp->random_mem = 1;
                  
                  break;
                  
                case 't':
                  pp->time = atoi(optarg);
                  break;
			    
                case 'l':
                  pp->tlow = atoi(optarg);
                  break;
                  
                case 'h':
                  pp->thigh = atoi(optarg);
                  break;
                  
                case 'm':
                  pp->mem = atoi(optarg);
                  break;
                  
                case 's':
                  pp->nproc = atoi(optarg);
                  break;
                  
                case 'c':
                  pp->curr_proc = atoi(optarg);
                  break;
                case 'd':
                  pp->io_dir = strdup(optarg);
                  break;
                case 'f':
                  pp->io_file = strdup(optarg);
                  break;
                case 'o':
                  pp->output_file_name = strdup(optarg);
                  break;
                case 'r':
                  pp->report = 1;
                  break;
                case 'v':
                  pp->verbose = 1;
                  glob_verbos = 1;
                  break;
                case '?':
                  usage();
                  exit(0);
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
 * Allocate the chunk of memory 
 ***************************************************************************/
static void
allocate_mem( char ***mem, int size, int random ) {

	int i = 0, j;

	srand(getpid());
	
	*mem = (char**) malloc( size * sizeof(char*));
	if( !*mem )
		test_load_error( "Malloc");
		
	for(i=0 ; i< size ; i++) {
		(*mem)[ i ] = (char*)malloc( MB1 );
		if( !( *mem )[ i ] ) 
			test_load_error( "Malloc" );

                memset((*mem)[i], 5, MB1);
        }
	if(random) {
		int *intPtr;
		for(i=0 ; i< size ; i++) {
			intPtr = (int *)(*mem)[i];
			for(j=0 ; j < MB1/sizeof(int) ; j++)
			{
				intPtr[j] = rand();
			}
		}
	}
}
/****************************************************************************
 * Access memory
 ***************************************************************************/
static void
access_mem( char *mem, int size, int random ) {
	if(random) {
		int  i;
		int  pageSize = getpagesize();
		int *intPtr;

		for(i=0 ; i< size/pageSize ; i++) {
			intPtr = (int *)(&mem[i*pageSize]);
			*intPtr = rand();
		}
	}
	else {
		memset( mem, '2', size );
	}
}

/****************************************************************************
 * Catch sigmig
 ***************************************************************************/
void sig_mig_hndl( int sig ) {
		
	int    whereFd;
	int    nmigs;
	char   whereStr[50];
	char  *prevWhereStr;
	
	whereFd = open("/proc/self/where", O_RDONLY);
	if(whereFd == -1) {
		strcpy(whereStr,"problem");
	} else 	{
		read(whereFd, whereStr, 48);
	}
	
	nmigs = open("/proc/self/nmigs", 0|O_CREAT);
	if (nmigs == -1) {
		test_load_error(" nmigs ");
	}
	
	if( glob_profile->pp_mig_num >= MAX_MIG_NUM ) {
		goto out;
	}
	
	prevWhereStr = glob_profile->pp_mig_arr[ glob_profile->pp_mig_num - 1 ];
	glob_profile->pp_mig_arr[ glob_profile->pp_mig_num++ ] = whereStr;
		
	if( glob_profile->pp_onmig_report ) {
		//time_t  t;
		//char   *timep;
		int     pid = getpid();
		int		whereami;
		/*t = time( NULL );
		timep = ctime(&t);
		timep[ strlen(timep) - 1 ] = '\0';
		fprintf(glob_profile->pp_output,
			"Pid: %d Time: %s %s -> %s  %d %d %s\n",
			pid, timep, prevWhereStr, whereStr, nmigs,
			glob_profile->pp_mig_num,
			(nmigs >= glob_profile->pp_mig_num ) ? "Problem" : "");*/
		whereami = open("/proc/self/whereami", 0);
		fprintf(glob_profile->pp_output, "\n%d Migrated to:", pid);
		if (whereami==0)
			fprintf(glob_profile->pp_output, " home");
		else
			fprintf(glob_profile->pp_output, " %2d", whereami);
		fprintf(glob_profile->pp_output, " (%3d total)\n", nmigs);
	}
	if( strcmp(whereStr, "0") == 0 && glob_profile->pp_exit_inhome )
		glob_do_exit = 1;

 out:
	close(whereFd);
	//close(nmigs);
}

/****************************************************************************
 * Signal handlers 
 ***************************************************************************/
void sig_term_hndl( int sig ) {
	glob_do_exit = 1;
}

void sig_vtalarm_hndl( int sig ) {
	glob_got_sig_alarm = 1;
	if(glob_verbos)
		fprintf(stderr, "Got signal\n");
}

/****************************************************************************
 * Install a virtual timer 
 ***************************************************************************/
void install_timer( int time ) {
		
	struct itimerval timer;

	if( signal(SIGVTALRM, sig_vtalarm_hndl) == SIG_ERR)
		test_load_error( "signal, SIGVTALRM");
		
	timer.it_value.tv_sec = time;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
    
	if( setitimer(ITIMER_VIRTUAL, &timer, NULL) < 0 )
		test_load_error( "setitimer");
}

/******************************************************************************
 *  Doing work, we assume we can get time interrupt access files in a given
 *  place or talk to father on pipe ....
 *  Once the function exit the process should stop
 ****************************************************************************/
static int
do_proc_work( proc_profile_t *p ) {
		
	int  i = 132, j = 0, k = 0;
	char **mem;

        allocate_mem( &mem, p->pp_mem, p->pp_random_mem );
        
	if( p->pp_cpu_units ) {
             for( k = 0; k < p->pp_cpu_units; k++ ) {
                  //access_mem( mem[ k % p->pp_mem ], MB1, p->pp_random_mem );
                  for( j = 0; j < MICRO; j++ )
                       if( glob_do_exit )
                            return 1;
             }
        }
	else {
		/* Allocating all memory at one time */
		glob_got_sig_alarm = 0;
		
		/* Set the timer if required */
		if( p->pp_time > 0)
			install_timer(p->pp_time);
				
		while(1) {
						
			/* Stop if the timer has expired */
			if( glob_got_sig_alarm || glob_do_exit )
				break;
			
			/* Touch the memory */
			if( p->pp_mem ) {
				for( j = 0; j < p->pp_mem; j++ )
                                     access_mem( mem[ j ], MB1, p->pp_random_mem );
				
			}
			i = i*5 + 12;
		}
	}
	return i;
}


/******************************************************************************
 *  Doing mpi test.  We assume we can get time interrupt access files in a given
 *  place or talk to father on pipe ....
 *  Once the function exit the process should stop
 ****************************************************************************/

#ifdef MPI_SUPPORT
static int
do_mpi_test( proc_profile_t *p ) {
	int  myid, numprocs;
	int  namelen;
	char processor_name[MPI_MAX_PROCESSOR_NAME];

	
	int  i = 132, j = 0, k = 0;
	char **mem;

	MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD,&myid);
	MPI_Get_processor_name(processor_name,&namelen);

	if(p->pp_verbose)
		fprintf(stderr,"Process %d on %s\n",
			myid, processor_name);
	
	MPI_Bcast(&p->pp_time, 1, MPI_INT, 0, MPI_COMM_WORLD);

	if(p->pp_verbose)
		fprintf(stderr, "Process %d got time %d\n",
			myid, p->pp_time);
	
	
	
	if( p->pp_cpu_units ) {
		for( k = 0; k < p->pp_cpu_units; k++ )
			for( j = 0; j < MICRO; j++ )
				if( glob_do_exit )
					return 1;
	}
	else {
		/* Allocating all memory at one time */
		allocate_mem( &mem, p->pp_mem, p->pp_random_mem );
		glob_got_sig_alarm = 0;
			
		/* Set the timer if required */
		if( p->pp_time > 0)
			install_timer(p->pp_time);
		
		while(1) {
			
			/* Stop if the timer has expired */
			if( glob_got_sig_alarm || glob_do_exit )
				break;
			
			/* Touch the memory */
			if( p->pp_mem ) {
				for( j = 0; j < p->pp_mem; j++ )
					access_mem( mem[ j ], MB1, p->pp_random_mem );
				
			}
			i = i*5 + 12;
		}
	}

	if(p->pp_verbose)
		fprintf(stderr, "Before barrier %d\n", myid );
	MPI_Barrier(MPI_COMM_WORLD);
	
	if(p->pp_verbose)
		fprintf(stderr, "After barrier %d\n", myid);
	return i;
}

#endif // MPI_SUPPORT
/****************************************************************************
 * Perform the system calls 
 ***************************************************************************/
static int
do_syscall( proc_profile_t *p ) {
		
	int res = 0;
	
	static unsigned long total_io = 0;
	switch( p->pp_type ) {
		
	    case IO_READ:
		    res = read( p->pp_iofd, p->pp_io_buff, p->pp_io_buff_size);
		    if(res == -1) {
			    perror( "Error doing read" );
			    return 0;
		    }
		    break;
		    
	    case IO_WRITE:
		    res = write(p->pp_iofd, p->pp_io_buff, p->pp_io_buff_size);
		    if(res == -1) {
			    perror("Error doing write");
			    return 0;
		    }
		    break;
		    
	    case NONIO_SYSCAL:
		    res = time(0);
		    if(res == -1) {
			    perror("Error doing time(0)");
			    return 0;
		    }
		    res = 0;
		    break;
		    
	    default:
		    printf( "Non supported action type %d\n",
			    p->pp_type);
		    return 0;
	}
	
	p->pp_syscall_num++;
	p->pp_io_done_size += res;
	total_io += res;
	
	/* Jump back to the beginning */
	if( p->pp_max_io_size > 0 && total_io >= p->pp_max_io_size ) {
		lseek( p->pp_iofd, 0, SEEK_SET );
		total_io = 0;
	}
	return 1;
}


void finish_syscall_work( proc_profile_t *p )
{
        switch( p->pp_type ) {
		
	    case IO_READ:
            case IO_WRITE:
                    close(p->pp_iofd);
		    break;
            default:
                    ;
        }		    
}
/****************************************************************************
 *
 ***************************************************************************/
static int
do_syscall_work( proc_profile_t *p ) {
	
	register int  j = 0, k = 0, l = 0;
	char          **mem = NULL;;
	int           io_counter = 0;
	
	// Allocating all memory at one time
	allocate_mem( &mem, p->pp_mem, p->pp_random_mem);
	
	// Access it once for it to be dirty
	for( j = 0; j < p->pp_mem; j++ )
		access_mem( mem[ j ], MB1, p->pp_random_mem);
	
	glob_got_sig_alarm = 0;
	
	// We have a timer running only if time > 0
	if( p->pp_time > 0 )
		install_timer( p->pp_time );
	
	while( 1 ) {
		
		// if our time has expire then we stop
		if( glob_got_sig_alarm || glob_do_exit )
			break;
		
		// The constant work which depend on cpu_unit;
		for( k = 0; k < p->pp_cpu_units; k++ )
			for( l = 0; l < MICRO ; l++ );
		
		// Perform the system call 
		for( k = 0; k < p->pp_syscall_per_chunk; k++ )
			if( !do_syscall( p ) )
				return 0;
		io_counter++;
		
		if( p->pp_verbose && !( io_counter % 100 ) )
			printf( "%d outof %d done\n", io_counter,
				p->pp_syscall_times );
		if( ( p->pp_syscall_times != 0 ) &&
		    ( io_counter >= p->pp_syscall_times ))
			break;
	}
        finish_syscall_work(p);
        return 1;
}
	
/****************************************************************************
 * Set the time to run
 * If time was already given use it. Else if pp->thigh is bigger than
 * pp->tlow, choose a random value in that range.
 ***************************************************************************/
static void
set_time( proc_profile_t *profile, prog_param_t *pp ) {

	if( ( pp->time == DEF_TIME ) && ( pp->thigh > pp->tlow )) {
		srand( time(NULL) * getpid());
		pp->time = (rand()%(pp->thigh - pp->tlow + 1 )) + pp->tlow;
	}

	if(pp->verbose)
		fprintf(stderr, "Time to run %d <= %d <= %d\n",
			pp->tlow, pp->time, pp->thigh);
	if( pp->time > 0 )
		profile->pp_time = pp->time;
	else
		profile->pp_time = 0;
}

/****************************************************************************
 * Handle the migration options 
 ***************************************************************************/
static void
init_proc_migration( proc_profile_t *prof, prog_param_t *pp ) {

	int     whereFd = 0;
	int     nmigs = 0;
	char    whereStr[50];
	
	/* Reporting on migration */
	if( pp->onmig_report )
		prof->pp_onmig_report = 1;
		
	/* Setting migration numbers */
	whereFd = open( "/proc/self/where", O_RDONLY );
	if( whereFd == -1 )
		strcpy(whereStr, "Problem");
	else {
		int r;
		r = read(whereFd, whereStr, 45);
		whereStr[r] = '\0';
		//fprintf(stderr, "Got %d: %s\n", r, whereStr);
	}
	nmigs = open("/proc/self/nmigs", 0|O_CREAT);
	if( nmigs == -1 )
		nmigs = 0;
	
	prof->pp_mig_arr[ 0 ] = "0";
	prof->pp_mig_num      = 1;
		
	if( strcmp(whereStr, "0") != 0 && nmigs > 0 ) {
		prof->pp_mig_arr[ 1 ] = strdup(whereStr);
		prof->pp_mig_num++;
	}
		
	if( prof->pp_onmig_report ) 
		fprintf(prof->pp_output, "%d %s %s\n",
			getpid(),
			nmigs > 0 ? "Migrated at birth to:" : "Born at:",
			whereStr );
	close(whereFd);
	//close(nmigs);
}

/****************************************************************************
 * Set up all the need data 
 ***************************************************************************/
static int
init_proc_profile( proc_profile_t *prof, prog_param_t *pp ) {

	bzero( prof, sizeof( proc_profile_t ));
		
	/* Create the output file and setit line buffered */
	if( pp->output_file_name) {
		if( !( prof->pp_output = fopen( pp->output_file_name, "w" )))
			test_load_error( "Opening output file");
				
		setlinebuf( prof->pp_output );
	}

	/* Use stdout */
	else 
		prof->pp_output = stdout;

	/* Handle the migration data */
	init_proc_migration( prof, pp );
		
	/* Set the time to run + size of memory to use */
	set_time( prof, pp );
	prof->pp_mem        =  pp->mem;
	prof->pp_random_mem =  pp->random_mem;
	prof->pp_cpu_units  =  pp->cpu_unit;
	prof->pp_verbose    =  pp->verbose;
		
	// Setting the processing type
	prof->pp_type     = CPU_ONLY;
	prof->pp_type_str = "CPU";

	/* Perfrom reading */
	if( pp->do_read >= 0 ) {
		prof->pp_type     = IO_READ;
		prof->pp_type_str = "READ";
	}

	/* Perfrom writing */
	else if( pp->do_write >= 0) {
		prof->pp_type = IO_WRITE;
		prof->pp_type_str = "WRITE";
	}

	/* Perfrom non io system calls */
	else if( pp->do_nonio_syscall ) {
		prof->pp_type     = NONIO_SYSCAL;
		prof->pp_type_str = "NONIO SYSCALL";
	}
	else if( pp->do_chkpt_demo ) {
		prof->pp_type     = CHKPT_DEMO;
		prof->pp_type_str = "CHECKPOINT DEMO";
		
		prof->pp_self_chkpt = pp->self_chkpt;
		prof->pp_chkpt_file = pp->chkpt_file;
	}
	
	/* Handle the writing/reading from a file */
	if( ( prof->pp_type == IO_READ ) || ( prof->pp_type == IO_WRITE )) {
				
		char path[ BUFF_SZ ];

		/* Set the basic unit for a single read/write */
		if( prof->pp_type == IO_READ )
			prof->pp_io_buff_size = pp->do_read;
		else
			prof->pp_io_buff_size = pp->do_write;
		
		/* Allocate the buffer for read */
		if( prof->pp_io_buff_size > 0 ) {
						
			prof->pp_io_buff = malloc( prof->pp_io_buff_size );
			if( !prof->pp_io_buff )
				test_load_error( "Malloc io_buff" );
						
			/* Set the max io size */
			if( pp->max_io_size > 0 )
				prof->pp_max_io_size = pp->max_io_size;

			/*
			 * Set the number of iteration that should be
			 * done in each loop. The default is 0.
			 */
			prof->pp_syscall_per_chunk = ( pp->syscall_chunk > 0 )?
				(pp->syscall_chunk / prof->pp_io_buff_size): 1;
						
			/* Number of loops to do when testing system calls */
			if( pp->syscall_times >= 0 )
				prof->pp_syscall_times = pp->syscall_times;
						
			else if( pp->io_size > 0 ) {
							
				prof->pp_syscall_times =
					( pp->io_size ) / ((pp->syscall_chunk > 0) ?
							   pp->syscall_chunk : 	prof->pp_io_buff_size) ;
				//	( prof->pp_io_buff_size *
				//	 prof->pp_syscall_per_chunk );
			}

			/* Copy the file name to use */
			if( pp->io_file )
				strcpy( path, pp->io_file );

			/* Set the file name in the given directory */
			else if( pp->io_dir ) {
				int pid = getpid();
				sprintf( path, "%s/tmp_file.%d",
					 pp->io_dir, pid );
			}
			else 
				test_load_error( "I/O requested but no "
						 "file name given\n" );

			/* Open the file */
			prof->pp_iofd = open( path, O_LARGEFILE | O_RDWR |
					      O_CREAT, 0600);
			if( prof->pp_iofd == -1 ) 
				test_load_error( "opening I/O file\n" );
						
			prof->pp_filename = strdup(path);
		}
		else {
			prof->pp_syscall_times = pp->syscall_times;
			prof->pp_syscall_per_chunk = 0;
			prof->pp_iofd = 0;
		}
	}
		
	else if( prof->pp_type == NONIO_SYSCAL ) {
		prof->pp_syscall_times    = pp->syscall_times;
		prof->pp_syscall_per_chunk = pp->do_nonio_syscall;
	}
	
	if( pp->report ) {
		fprintf( prof->pp_output,
			 "\n\n"
			 "Type:           %s\n"
			 "Time:           %d\n"
			 "Mem:            %d\n"
			 "CPU_UNIT:       %d \n"
			 "SYSCALL_TIMES:     %d\n"
			 "SYSCALL_PER_CUNK:  %d\n"
			 "IO_OP_SIZE:     %d\n"
			 "IO_FD:          %d\n"
			 "TOTAL:          %dMB\n",
			 prof->pp_type_str,
			 prof->pp_time,
			 prof->pp_mem,
			 prof->pp_cpu_units,
			 prof->pp_syscall_times,
			 (int)prof->pp_syscall_per_chunk,
			 (int)prof->pp_io_buff_size,
			 prof->pp_iofd,
			 (int) pp->io_size);
	}
	return 1;
}

/****************************************************************************
 * Initialize the command options 
 ***************************************************************************/
static void
prog_param_init( int argc, char **argv, prog_param_t *param ) {
		
	param->time         = DEF_TIME;
	param->tlow         = 0;
	param->thigh        = 0;
	param->mem          = 0;
	param->random_mem   = 0;
	param->nproc        = 1;
	param->curr_proc    = 0;
	param->job_time     = 0;
	param->cpu_unit     = 0;
	param->do_read      = -1;
	param->do_write     = -1;
	param->do_nonio_syscall = 0;
	param->do_chkpt_demo = 0;
	param->syscall_times     = -1;
	param->syscall_chunk     = 0;
	param->io_size      = 0;
	param->max_io_size  = 0;
	param->report       = 0;
	param->io_dir       = NULL;
	param->io_file      = 0;
	param->sleep_sec    = 0;
	param->catch_sigmig = 1;
	param->onmig_report = 0;
	param->exit_inhome  = 0;
	param->output_file_name = NULL;
	param->change_uid   = 0;
	param->verbose = 0;
	param->self_chkpt = 0;
	param->chkpt_file = NULL;
	
	glob_prog_name = argv[ 0 ];
	parse_args( argc, argv, param );
}

/****************************************************************************
 * Hanle uid 
 ***************************************************************************/
static void
handle_uid( int uid  ){

	if( setuid( uid ) == -1) {
		char buf[ BUFF_SZ ];
		sprintf( buf, "changing uid to %d\n", uid );
		test_load_error( buf );
	}
}


/****************************************************************************
 * Hanle signals
 ***************************************************************************/
static void
handle_signals( int catch_sigmig ) {

	if( catch_sigmig ) {
		int res = 0;
		res = open( "/proc/self/sigmig", 1|O_CREAT, 55);
		if( signal( 55, sig_mig_hndl ) < 0 )
			test_load_error( "signal, sigmig" );
	}
	
	if( signal( SIGTERM, sig_term_hndl ) < 0 )
		test_load_error( "signal, SIGTERM " );
		
	if( signal( SIGINT, sig_term_hndl ) < 0 )
		test_load_error( "signal, SIGTERM " );
	
}

/****************************************************************************
 *  Report the results 
 ***************************************************************************/
static void
test_load_report( struct timeval *start, struct timeval *end,
		  prog_param_t *param, proc_profile_t *profile ) {

	struct timeval total;
	float sec = 0.0, syscall_per_sec = 0.0;
		
	fprintf( profile->pp_output,"\nResults:\n=============\n");
	total.tv_sec  = end->tv_sec - start->tv_sec;
	total.tv_usec = end->tv_usec - start->tv_usec;
		
	if( total.tv_usec < 0 ) {
		total.tv_sec--;
		total.tv_usec += MICRO;
	}
		
	sec = total.tv_sec;
	sec += (float)total.tv_usec /(float)MICRO;
	syscall_per_sec = (float)profile->pp_syscall_num/sec;
		
	fprintf( profile->pp_output,"Time:       %.5f sec\n", sec );
	fprintf( profile->pp_output,"Syscalls:   %d\n", profile->pp_syscall_num);
	fprintf(profile->pp_output,"Syscall/Sec  %.3f\n", syscall_per_sec );
		
	if( ( profile->pp_type == IO_READ ) || ( profile->pp_type == IO_WRITE )){

		float io_rate = 0.0;

		fprintf( profile->pp_output,"Total I/O:   %ld %ldKB %ldMB\n",
			 (long int)(profile->pp_io_done_size),
			 (long int)(profile->pp_io_done_size/KB1),
			 (long int)(profile->pp_io_done_size/ MB1));
				
		io_rate =
			(float)(profile->pp_syscall_times *
				profile->pp_io_buff_size *
				profile->pp_syscall_per_chunk) / sec;
				
		fprintf(profile->pp_output,"IO Rate:         "
			"%.2fKB/sec %.4fMB/sec\n",
			io_rate/(float)KB1, io_rate/(float)MB1);
		io_rate = (float)(profile->pp_io_done_size) / sec;
		fprintf( profile->pp_output,"Actual I/O Rate: "
			 "%.2fKB/sec\n", io_rate/(float)KB1);
	}
        
	if( param->catch_sigmig) {
				
		fprintf( profile->pp_output,"Migs:    %d\n",
			 profile->pp_mig_num );
				
		if( profile->pp_mig_num > 0 ) {
			int j = 0;
			for( j = 0; j < profile->pp_mig_num; j++ )
				fprintf( profile->pp_output,"%s ",
					 profile->pp_mig_arr[j]);
			fprintf( profile->pp_output,"\n" );

		}
	}
}


static int
do_checkpoint_demo( proc_profile_t *p )
{
	int    j, k;
	char **mem;
	// Setting a default of 100 cycles if no cpu unit is given
	if(p->pp_cpu_units <= 0)
		p->pp_cpu_units = 100;

	
	allocate_mem( &mem, p->pp_mem, p->pp_random_mem );
			
	for( k = 0; k < p->pp_cpu_units; k++ )
	{
		for( j = 0; j < MICRO * 1000; j++ )
		{
			if( glob_do_exit )
				return 1;
		}
		printf("Unit %d\n", k);

		// Perforing a self checkpoint 
		if(p->pp_self_chkpt && k ==p->pp_self_chkpt) {
			int fd;
			
			if (p->pp_chkpt_file) {
				fd = open("/proc/self/checkpointfile", 1|O_CREAT, p->pp_chkpt_file);
				if (fd == -1) {
					fprintf(stderr, "Error setting self checkpoint filename\n");
					return 0;
				}
			}

			fd = open(MOSIX_CHKPT_FILENAME, 1|O_CREAT, 1);
			if(fd == -1) {
				fprintf(stderr, "Error doing self checkpoint \n");
				return 0;
				
			}
			else if(fd == 0) {
				printf("Checkpoint successful\n");
			}
		}
	}
	return 1;
}
int generate_job_processes(int nproc)
{
        int i;
        int pid;
        for(i=0; i<nproc-1; i++) {
                pid = fork();
                if(pid == -1) {
                        fprintf(stderr, "Error in fork\n");
                        return 0;
                }
                else if(pid == 0 ) {
                        return 1;
                }
        }
        return 1;
}
        
/****************************************************************************
 * Main
 ***************************************************************************/
int
main(int argc, char **argv) {
		
	prog_param_t prog_opts;
	proc_profile_t profile;
	struct timeval start, end;
	
#ifdef MPI_SUPPORT
	MPI_Init(&argc,&argv);
#endif
	
	/* Handle the program options */
	prog_param_init( argc, argv, &prog_opts );
	
	/* Hanlde the change of uid */
	if( prog_opts.change_uid > 0 )
		handle_uid( prog_opts.change_uid );
	
	/* Set the process profile */
	if(!init_proc_profile( &profile, &prog_opts ))
		return 1;
	
	/* So sigmig hndl can modify the migration number */
	glob_profile = &profile;
	
	/* Hanlde signals */
	handle_signals( prog_opts.catch_sigmig );
	
	if( prog_opts.sleep_sec > 0 ) {
             if(prog_opts.verbose)
                  fprintf(stderr, "Going to sleep for %d seconds ...",
                          prog_opts.sleep_sec);
             sleep( prog_opts.sleep_sec );
             if(prog_opts.verbose)
                  printf(" done\n");
	}
	/*Do the work */
#ifdef MPI_SUPPORT
        // In mpi mode there is no need for job
        prog_opts.nproc = 1;
#endif
        // Job is not allowed in checkpoint demo
        if(profile.pp_type == CHKPT_DEMO)
                prog_opts.nproc = 1;
        
        if(prog_opts.nproc > 1) {
                if(!generate_job_processes(prog_opts.nproc))
                        return 1;
        }
        gettimeofday( &start, NULL );
        switch(profile.pp_type)
        {
            case CPU_ONLY:
#ifdef MPI_SUPPORT
                    do_mpi_test(&profile);
                    break;
#else
                    do_proc_work( &profile );
                    break;
#endif
            case CHKPT_DEMO:
                    do_checkpoint_demo( &profile);
                    break;
            default:
                    do_syscall_work( &profile );
        }

	gettimeofday( &end, NULL );
	
	if(prog_opts.report)
		test_load_report( &start, &end, &prog_opts, &profile );
	
	fclose( profile.pp_output );

#ifdef MPI_SUPPORT
	MPI_Finalize();
#endif
	return 0;
}

/****************************************************************************
 *                            E O F
 ***************************************************************************/

