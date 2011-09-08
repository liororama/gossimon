/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


// 444444444444
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include <msx_error.h>
#include <defs.h>
#include <kernel_comm.h>
#include <kernel_comm_protocol.h>


void sig_alarm_hndl(int sig)
{
    return;
}

void sig_int_hndl(int sig)
{
    fprintf(stderr, "Got sigint\n");
    kcomm_stop();
    exit(1);
}

int install_timer(int milli)
{
    struct itimerval timeout;

    /* Setting the timeout */ 
    timeout.it_value.tv_sec = milli / 1000;
    timeout.it_value.tv_usec = (milli % 1000) * 1000;
    timeout.it_interval = timeout.it_value;
    
    setitimer( ITIMER_REAL, &timeout, NULL);
    return 1;
}

int test_consts_change()
{
    int i, j;
    unsigned long costs[7 * (TUNE_CONSTS + 2)];
    
    for(i=0 ; i<7 ; i++)
	for(j = 0 ; j < TUNE_CONSTS + 2 ; j++)
	    costs[i*(TUNE_CONSTS + 2) + j] = i*100 + j;
    
    kcomm_send_cost_change(7, costs);
    return 1;
}



int main(int argc, char **argv)
{
    
    fd_set rfds, wfds, efds;
    int max_sock;
    struct sigaction act;

    if(argc < 2)
    {
	fprintf(stderr, "Usage: %s <kcom_module>\n", argv[0]);
	return 1;
    }
    
    /* setting the signal handler of SIGALRM */
    act.sa_handler = sig_alarm_hndl ;
    sigemptyset ( &( act.sa_mask ) ) ;
    sigaddset ( &( act.sa_mask) , SIGHUP ) ;
    act.sa_flags = SA_RESTART ;

    if ( sigaction ( SIGALRM, &act, NULL ) < -1 )
       msx_critical_error( "Error: calling siaction on SIGALRM\n" ) ;
    siginterrupt( SIGALRM, 0 );
    install_timer(500);

    /* setting the signal handler of SIGINT */
    act.sa_handler = sig_int_hndl ;
    sigfillset ( &( act.sa_mask ) ) ;
    act.sa_flags = SA_RESTART ;

    if ( sigaction ( SIGINT, &act, NULL ) < -1 )
	msx_critical_error( "Error: calling siaction on SIGINT\n" ) ;
    
  
    msx_set_debug(KCOMM_DEBUG);
    
    if(!kcomm_init("/tmp/infod_mosix_sock", 100, argv[1]))
    {
	debug_lr(KCOMM_DEBUG, "Error in kcomm_init()\n");
	return 1;
    }
    
    
    while(1) {
	int res;
	
	FD_ZERO( &rfds );
	FD_ZERO( &wfds );
	FD_ZERO( &efds );
	
	max_sock = -1;
	kcomm_get_fdset(&rfds, &wfds, &efds);

	max_sock = kcomm_get_max_sock(max_sock);

	res = select(max_sock +1, &rfds, &wfds, &efds, NULL);
	
	if(res == -1)
	{
	    if(errno != EINTR)
		msx_critical_error( "Error: in select()\n%s",
				    strerror(errno));
	    else {
		// We got signal
		kcomm_periodic_admin();
	    }
	    continue;
	}
	else if(kcomm_exception(&efds))
	{
	    fprintf(stderr, "BBBBBBB got exception");
	    kcomm_reset();
	}
	else if(kcomm_is_new_connection(&rfds))
	{
	    if(!kcomm_setup_connection())
	    {
		fprintf(stderr, "Bummer: Error setup new connection\n");
		return 1;
	    }
	}
	else if(kcomm_is_req_arrived(&rfds))
	{
	    int magic, req_len = 512;
	    char req_buff[512];
	   
	    
	    debug_g("Got new msg\n");
	    if(!kcomm_is_auth())
	    {
		debug_g("Getting auth msg\n");
		if(!kcomm_recv_auth())
		    debug_r("Bad auth msg\n");

		else {
		    // OK authenticated channel
		    // We send speed change + constc change messages
		    if(!kcomm_send_speed_change(4000))
			debug_r("Error sending speed change\n");
		    
		    if(!test_consts_change())
			debug_r("Error sending cost change\n");
		}

	    }
	    else
	    {
		// We got request 
		res = kcomm_recv(&magic, req_buff, &req_len);
		if(res == -1)
		{
		    fprintf(stderr, "Error in kcomm_recv\n");
		    
		}
		else if (res == 0)
		{
		    fprintf(stderr, "Got partial message");
		}
		else {
		    fprintf(stderr, "Got compleate message %d %x\n",
			    req_len, magic);
		    kcomm_print_msg(req_buff, req_len);
		    kcomm_handle_msg(magic, req_buff, req_len);
		}
	    }
	}
	else if(kcomm_is_ready_to_send(&wfds))
	{
	    debug_g("Calling finish send\n");
	    if(!kcomm_finish_send())
	    {
		debug_r("Error finishing send\n");
		break;
	    }
	}
    }
    
    kcomm_stop();
    return 0;
}

