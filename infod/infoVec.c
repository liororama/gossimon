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

/*****************************************************************************
 *
 *  File: infovec.c - implementation of the vector and its interface. 
 *  
 *****************************************************************************/
/*
  Information vector object.
  Gives each entry a time stamp and knows how to update according to time
  stamps

  Knows how to return a random node from the living/all nodes

  Knows how to take a window and merge it into the vector. Taking only
  valid nodes

  Knows how to respond to queries.

  Bases on IP of nodes, in the form of struct in_addr where the address
  should be in host byte order so we can sort the nodes ip...


  FIXME: give the vector a hint about the size of the information so the
         initial allocation of memory will not change in the usuall time
  
*/


#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

#include <ModuleLogger.h>
#include <Mapper.h>

//#include <infod.h>
#include <info.h>
#include <msx_error.h>
#include <msx_debug.h>

//#include <common.h>
#include <pe.h>


#include <infoVec.h>
#include <infoVecInternal.h>

//#include <distance_graph.h>


/****************************************************************************
 * Functions
 ***************************************************************************/
static int calcWinSizeFixed( ivec_t vec );
static int calcWinSizeUptoAge( ivec_t vec );
static void infoVecDoWinSizeMeasure(ivec_t vec, int currWinSize);
static void ivec_print_win( ivec_t vec );
static void ivec_kill_entry(ivec_t vec, ivec_entry_t *entry, unsigned int cause);

//static int
//compare( const void* a, const void* b){
//	return( *((node_t*)(a)) - *((node_t*)(b)));
//}

static int
compare_ips( const void* a, const void* b){
	struct in_addr *A, *B;
	A = (struct in_addr *)a;
	B = (struct in_addr *)b;
	
	return( A->s_addr - B->s_addr);
}


static unsigned long
compute_age( struct timeval *time, struct timeval *cur ) {
	
	unsigned long a, b;
        
        a = cur->tv_sec  - time->tv_sec;  
	b = cur->tv_usec - time->tv_usec;
	return (a * MILLI) + b;
}

void network_order(struct in_addr *ip) {
	ip->s_addr = htonl(ip->s_addr);
}
void host_order(struct in_addr *ip) {
	ip->s_addr = ntohl(ip->s_addr);
}

char *inet_host_ntoa(struct in_addr ip)
{
	network_order(&ip);
	return inet_ntoa(ip);
}

static int ipEqual(struct in_addr *ip1, struct in_addr *ip2)
{
	if(ip1->s_addr == ip2->s_addr) return 1;
	return 0;
}

// Assuming the vector and the index are ok
static inline
void indexToIp(ivec_t vec, int index, struct in_addr *ip)
{
	*ip = vec->vec[index].info->hdr.IP;

}

static inline
struct in_addr *indexToIpAddr(ivec_t vec, int index)
{
	return &(vec->vec[index].info->hdr.IP);
}

// Incrementing an ip which is given at netowrk order
static inline
void ipInc(struct in_addr *ip) {
	host_order(ip);
	ip->s_addr++;
	network_order(ip);
}

/****************************************************************************
 * Translates the time stamp to age 
 ***************************************************************************/
void
ivec_time2age( node_info_t *node, struct timeval *cur ) {

	node->hdr.time.tv_sec  = cur->tv_sec  - node->hdr.time.tv_sec;
	node->hdr.time.tv_usec = cur->tv_usec - node->hdr.time.tv_usec;

	if( node->hdr.time.tv_usec  < 0 ) {
		node->hdr.time.tv_sec--; 
		node->hdr.time.tv_usec += MILLI;
	}
}

/****************************************************************************
 * Translate age to time
 ***************************************************************************/
void
ivec_age2time( node_info_t *node, struct timeval *cur ) {
	
	node->hdr.time.tv_sec  = cur->tv_sec  - node->hdr.time.tv_sec; 
	node->hdr.time.tv_usec = cur->tv_usec - node->hdr.time.tv_usec;
	
	if( node->hdr.time.tv_usec < 0 ) {
		node->hdr.time.tv_sec--; 
		node->hdr.time.tv_usec += MILLI;
	}
}

/****************************************************************************
 * Init a single vector entry
 ***************************************************************************/
static int
ivec_init_entry( ivec_entry_t *entry, node_t pe, char *name, struct in_addr *ip )
{
	if( !( entry->info = (node_info_t*) malloc( NHDR_SZ ))) {
		debug_lr( VEC_DEBUG, "Error: malloc, vec entry \n" );
		return 0;
	}
	

	// The vector only information
	strcpy( entry->name, name );
	entry->isdead   = 1;
	// The entry information which is sent over the network
	entry->info->hdr.psize     = NODE_HEADER_SIZE;
	entry->info->hdr.fsize     = NODE_HEADER_SIZE;
	entry->info->hdr.pe        = pe;
	entry->info->hdr.status    = INFOD_DEAD_INIT;
	entry->info->hdr.cause     = 0;
	entry->info->hdr.IP        = *ip;
        entry->info->hdr.external_status = EXTERNAL_STAT_NO_INFO;
        
	//memcpy( entry->info->hdr.ip, ip, COMM_IP_VER );
	gettimeofday (&(entry->info->hdr.time), NULL);

	return 1;
}

/****************************************************************************
 * Init all the vector entries
 * Retruns the number of entries in the continouos mapping
 ***************************************************************************/

static int
infoVecInitEntries( ivec_t vec, mapper_t map) {
	
	mapper_iter_t  map_iter = NULL;
	node_t         *pes      = NULL;
	struct         in_addr  *ips = NULL;
	int             i = 0, j = 0;
	mapper_node_info_t  mni;
	/* create an iteration object for the msxmap object */ 
	
	if( !( map_iter= mapperIter( map, MAP_ITER_ALL ))){
		debug_lr( VEC_DEBUG, "Error: init, map iterator\n" );
		return 0;
	}

	/* allocate a temporary array for the pes */ 
	if( !(ips = (struct in_addr*) malloc( sizeof(struct in_addr) * vec->vsize ))) {
		debug_lr(  VEC_DEBUG,"Error: malloc, in_addr arr\n" );
		return 0;
	}

        debug_lb(VEC_DEBUG, "InfoVecInitEntries\n");
        
	/* get all the ips, not necessarily by order */ 
        i=0;
	while( (i<vec->vsize) && mapperNext( map_iter, &mni)) {
		ips[i] = mni.ni_addr;
		host_order(&ips[i]);
		
		debug_lg( VEC_DEBUG, "Mapper iter %d %s\n",
			  i, inet_ntoa(mni.ni_addr));
		//debug_lg( VEC_DEBUG, "Mapper iter %d %s\n",
		//	  i, inet_ntoa(ips[i]));
		i++;
	}
	mapperIterDone( map_iter );
      
	/* sanity check - must read vec->vsize pes */
	if( i !=  vec->vsize ) {
		debug_lr(  VEC_DEBUG, "Error: iterating map\n" );
		goto exit_with_free;
	}
	
	/* sort the ips */
	qsort( ips, vec->vsize, sizeof(struct in_addr), compare_ips );

	/* allocate info of each entry.  The initial size is 'info_size' */
	for( i = 0, j = 1; i < vec->vsize ; i++ )
	{
		char             name[MACHINE_NAME_SZ];
		mnode_t          PE;
		struct in_addr   ipaddr_norder;
		struct hostent   *host = NULL;
		char             *cur = NULL;

		//debug_lb(  VEC_DEBUG, "Doing %d\n", i);
		bzero( name, MACHINE_NAME_SZ );
		ipaddr_norder = ips[i];
		network_order(&ipaddr_norder);
		
		if( !( mapper_addr2node( map, &ipaddr_norder, &PE ))) {
			debug_lr(VEC_DEBUG, "Error in mapper_addr2node\n");
			goto exit_with_free;

		}

                if(vec->resolveHosts) {
                     if( (host = gethostbyaddr( (const char*)&ipaddr_norder,
                                                sizeof (struct in_addr), AF_INET )))
                     {
                          // If the name containes dosts we take only the first
                          // part
                          cur = strchr( host->h_name, '.' );
                          if( cur ) 
                               *cur ='\0';
                          strncpy( name, host->h_name, MACHINE_NAME_SZ -1);
                          
                     } else {
                          strncpy( name, inet_ntoa(ipaddr_norder), MACHINE_NAME_SZ -1);
                     }
                } else {
                     strncpy( name, inet_ntoa(ipaddr_norder), MACHINE_NAME_SZ -1);
                }

		debug_ly( VEC_DEBUG, "Got name %s\n", name);
		if(!ivec_init_entry( &(vec->vec[i]), PE, name, &ipaddr_norder ))
                        goto exit_with_free;

		//if( [i] == vec->local_pe )
		//vec->localNodeIndex = i;
		
		if( (i > 0) &&  ( ips[i].s_addr - ips[i-1].s_addr) > 1 )
			j++; 
	}
	
	free( pes );
        free( ips );
	pes = NULL;
	return j;

 exit_with_free:
	if( pes )
		free(pes);
        if( ips )
                free(ips);
	return 0;
}

/****************************************************************************
 * Init the continuous mapping
 ***************************************************************************/
static int
infoVecInitContIps( ivec_t vec ) {

	int i = 0, j = 0;
	int size = vec->contIPs.size;
	
	if( !(vec->contIPs.data = (cont_vec_ips_ent_t*)
	      malloc( size * (sizeof(cont_vec_ips_ent_t)) ))){
		debug_lr(  VEC_DEBUG, "Error: malloc, continuous map\n" );
		return 0;
	}

	j = 0;
	vec->contIPs.data[j].index = 0;
	vec->contIPs.data[j].ip    = vec->vec[0].info->hdr.IP;
	host_order(&(vec->contIPs.data[j].ip));
	vec->contIPs.data[j].size  = 1;

	for( i = 1; i < vec->vsize; i++ ) {
		ivec_entry_t *first  = &(vec->vec[i-1]);
		ivec_entry_t *second = &(vec->vec[i]);
		struct in_addr firstIp = first->info->hdr.IP;
		struct in_addr secondIp = second->info->hdr.IP;
		//char *tmp1, *tmp2;
		//debug_ly(  VEC_DEBUG, "First %s \n", inet_ntoa(firstIp));
		host_order(&firstIp);
		host_order(&secondIp);
		//tmp1 = strdup(inet_host_ntoa(firstIp));
		//tmp2 = strdup(inet_host_ntoa(secondIp));
		//debug_ly(  VEC_DEBUG, "First %s second %s\n", tmp1, tmp2);
		
		if( (secondIp.s_addr - firstIp.s_addr ) > 1 ) {
			j++;
			vec->contIPs.data[j].index = i;
			vec->contIPs.data[j].ip   = secondIp;
			vec->contIPs.data[j].size  = 1;
		}
		else
			vec->contIPs.data[j].size++;
	}
	return 1;
}

/****************************************************************************
 * Init the window. Note that the actual window size is win_size - 1, since
 * we always save an entry to the local node.
 ***************************************************************************/
static int infoVecInitWin( ivec_t vec, int winType, int winTypeParam )
{
	vec->win.type    = winType;
	
	if(winType == INFOVEC_WIN_FIXED) {
		debug_lb(VEC_DEBUG, "Fixed size window %d\n", winTypeParam);
		// the -1 is because local node also takes one place
		vec->win.fixWinSize = winTypeParam -1; 
		vec->win.size       = 2 * winTypeParam - 1;
		vec->win.calcWinSizeFunc = calcWinSizeFixed;
	} else if (winType == INFOVEC_WIN_UPTOAGE) {
		debug_lb(VEC_DEBUG, "Upto age window %d\n", winTypeParam);
		// Assuming that the age is is milli seconds so we move it to
		// micro seconds 
		vec->win.uptoAge = winTypeParam * 1000;
		// As we saw from the simulations there might be some big windows
		// when the uptoage policy is used
		vec->win.size       = vec->vsize;
		vec->win.calcWinSizeFunc = calcWinSizeUptoAge;
	} else {
		debug_lr( VEC_DEBUG, "Error: unsupported winType %d\n", winType);
		return 0;
	}
	//vec->win.send_size  = win_size - 1;
	
	if( !(vec->win.data =
	      (info_win_entry_t*) malloc( vec->win.size * INFO_WIN_ENTRY_SIZE ))){
		debug_lr( VEC_DEBUG, "Error: malloc, win\n" );
		return 0;
	}
	bzero( vec->win.data, vec->win.size * INFO_WIN_ENTRY_SIZE );
	for(int i=0 ; i < vec->win.size ; i++)
		vec->win.data[i].index = -1;
	
	if( !(vec->win.aux = (int*) malloc( vec->win.size * sizeof(int)))){
		debug_lr( VEC_DEBUG, "Error: malloc, win aux\n" );
		return 0;
	}
	bzero( vec->win.aux, vec->win.size * sizeof(int));

	return 1;
}
	

/****************************************************************************
 * Compute the signature from the description
 ***************************************************************************/
static void
infoVecComputeSignature( ivec_t vec, char *description ) {

	vec->signature = 0;
	
	for( ; *description != '\0'; description++ )
		 vec->signature = *description + 31 * vec->signature;
}


int infoVecInitAgeMeasure(ivec_t vec) {
	vec->ageMeasure.im_maxSamples = 0;
	vec->ageMeasure.im_sampleNum = 0;
	vec->ageMeasure.im_avgAge = 0.0;
	return 1;
}
/****************************************************************************
 * Print the current time
 ***************************************************************************/
/****************************************************************************
 * Initiation function
 ***************************************************************************/
ivec_t
infoVecInit( mapper_t map, 
             unsigned long max_age, 
             int winType, 
             int winTypeParam,
	     char* description,
             int resolveHosts)
{
	ivec_t   vec = NULL;
	int      node_num = 0;

        mlog_registerModule("vec", "Information Vector Management", "vec");

	/* test arguments */ 
	if( !map || ( mapperTotalNodeNum( map ) <= 0) ||
	    !description ) {
		debug_lr( VEC_DEBUG, "Error: args, vec init\n" );
		return NULL;
	}
	
	/* allocate the structure */ 
	if( !(vec = (ivec_t)malloc( INFO_VEC_SIZE ))) {
		debug_lr( VEC_DEBUG, "Error: malloc, ivec_t\n" );
		return NULL;
	}
	bzero( vec,  INFO_VEC_SIZE);
        vec->resolveHosts = resolveHosts;
	
	node_num = mapperTotalNodeNum(map);
	vec->vsize = node_num;
    
	/* allocate the vector entries */    
	if( !(vec->vec = (ivec_entry_t*) malloc( vec->vsize * VENTRY_SZ ))) {
		debug_lr( VEC_DEBUG, "Error: malloc, info vector\n" );
		goto exit_with_free;
	}
	bzero( vec->vec, vec->vsize * VENTRY_SZ );

	/* Init all the vector entries */
	if( !(vec->contIPs.size = infoVecInitEntries( vec, map))) {
		debug_lr(VEC_DEBUG, "Error in infoVecInitEntries\n");
		goto exit_with_free;
	}
	/* Init the continous mapping */
	if( !( infoVecInitContIps( vec )))
		goto exit_with_free;
	
	/* Init the window */
	if( !( infoVecInitWin( vec, winType, winTypeParam )))
		goto exit_with_free;
	
	vec->max_age = max_age; 
	gettimeofday( &vec->init_time, NULL ) ;

	vec->prev_time = vec->init_time;
	
	bzero(&vec->lastDeadIP, sizeof(struct in_addr));
	vec->numAlive    = 0;

	vec->localPrio = 0;
	// This is based on the assumption that an important message will
	// be spread very fast O(log(n)) in the cluster, using regular gossip.
	// So we go on the safe side and set the constant to 2.
	vec->deadStartPrio = (unsigned int) (1.386 *  log(vec->vsize)/log(2.0));

	// Setting localIP and localIndex
	vec->localIndex = -1;
	if(!mapperGetMyIP(map, &vec->localIP)) {
		debug_lr(VEC_DEBUG, "Error mapper does not contain my IP");
		goto exit_with_free;
	}
	
	if(!infoVecFindByIP( vec, &vec->localIP, &vec->localIndex )) {
		debug_lr(VEC_DEBUG, "Error map contain my ip but vector dont\n");
		goto exit_with_free;
	}
	/* Compute the signature */
	infoVecComputeSignature( vec, description );
	
	/* allocate the buffer for messages */
	vec->msg_buff_size = MSG_BUFF_SZ;
	if( !(vec->msg_buff = malloc( vec->msg_buff_size ))) {
		debug_lr( VEC_DEBUG, "Error: malloc\n" );
		goto exit_with_free;
	}

	infoVecInitAgeMeasure(vec);
	
	mlog_bn_info("vec", "Info Vector Init done\n");	
	
	return vec;

 exit_with_free:
	infoVecFree( vec );
	return NULL;
}

/****************************************************************************
 * Free a single vector entry
 ***************************************************************************/
static void
infoVecFreeEntry( ivec_entry_t *entry ) {
	if( !entry )
		return;
	free( entry->info ) ;
}

/****************************************************************************
 * Free the vector and all its data structures
 ***************************************************************************/
void
infoVecFree( ivec_t vec ) {

	if( !vec )
		return;
	
	if( vec->contIPs.data )
		free( vec->contIPs.data );
	
	if( vec->vec ) {
		int i = 0;
		for( i = 0; i < vec->vsize ; i++ )
			infoVecFreeEntry( &(vec->vec[i]));
                free(vec->vec);
        }

        if(vec->win.data) {
                free(vec->win.data);
                free(vec->win.aux);
        }
        
	if( vec->msg_buff_size )
		free( vec->msg_buff );
	
	free(vec);
}

/****************************************************************************
 * Find a node in the vector
 ***************************************************************************/
ivec_entry_t*
infoVecFindByIP( ivec_t vec, struct in_addr *ip, int *index ) {

	int i = 0;
	static int last_ind = 0;
	ivec_entry_t *entry = NULL;
	struct in_addr ip_horder;
	
	ip_horder = *ip;
	host_order(&ip_horder);
	
	/* maybe the requests are serial */
	if( last_ind < (vec->vsize - 1)) {
		
		entry = &(vec->vec[last_ind+1]);
		if( ip_horder.s_addr == entry->info->hdr.IP.s_addr ) {
			last_ind++;
			*index = last_ind;
			return entry;
		}
	}
    
	for( i = 0 ; i < vec->contIPs.size ; i++ ) {
		if( ( ip_horder.s_addr >= vec->contIPs.data[i].ip.s_addr ) &&
		    ( ip_horder.s_addr <= ( vec->contIPs.data[i].ip.s_addr +
			      vec->contIPs.data[i].size - 1 ))) {
			int pos = vec->contIPs.data[i].index +
				(ip_horder.s_addr - vec->contIPs.data[i].ip.s_addr ) ;
			last_ind = pos;
			entry = &(vec->vec[pos]);
			*index = pos;
			return entry;
		}
	}

	*index = -1;
      	return NULL ;
}

/****************************************************************************
 * Compares the priority of two entries in the vector used for sorting the
 * win entries. Assumes that the time field of each entry is the age and
 * not the time.
 * Returns -1 if the first entry should come before the second
 *          0 if entries equals
            1 if second entry should come before the first 
 * window.
 ***************************************************************************/
static int
ivec_win_cmp( ivec_t vec, info_win_entry_t *firstWinEnt, info_win_entry_t *secondWinEnt )
{
	if( firstWinEnt->priority > secondWinEnt->priority )
		return -1;
	if( firstWinEnt->priority < secondWinEnt->priority )
		return 1;
	
	if( firstWinEnt->priority == secondWinEnt->priority )
	{
		ivec_entry_t *firstVecEnt  = &(vec->vec[ firstWinEnt->index ]);
		ivec_entry_t *secondVecEnt = &(vec->vec[ secondWinEnt->index ]);
		
		if( timercmp( &(firstVecEnt->info->hdr.time),
			      &(secondVecEnt->info->hdr.time), > ))
			return -1;
		else
			return 1;
	}
	return 0;
}

/****************************************************************************
 * Update the place of the window entry
 * Note that the function is called only if the corresponding vector entry
 * was updated.
 ***************************************************************************/
static int
ivec_update_win( ivec_t vec, info_win_entry_t *entry ) {
	
	int i = 0, j = 0;

	/* The local infod is not part of the window. */
	if( vec->localIndex == entry->index)
		return 0;
	// Checking if the entry is already part of the window
	for( i = 0 ; i < vec->win.size ; i++ ) {
		// We reached an empty place 
		if(vec->win.data[ i ].index == -1)
			break;
		if(vec->win.data[ i ].index == entry->index)
			break;
	}
	// If the entry is not part of the window taking the place of the last
	if( i == vec->win.size )
		i = vec->win.size - 1;

	/*
	 * In any case, test that the new entry has higher priority.
	 * If it was not found in the window, compare it to the
	 * window entry with the lowest priority. If it was found in the
	 * window, check that the window priority is not bigger than the
	 * update priority.
	 */
	if(vec->win.data[ i ].index != -1)
	{
		if( (ivec_win_cmp( vec, &(vec->win.data[ i ]), entry )) != 1 )
			return 0;
	}
	// The following is for alowing the high priority to decay properly
	if(vec->win.data[i].index == entry->index)
	{
		if(vec->win.data[i].priority > 0)
			entry->priority = vec->win.data[i].priority;
	}
	vec->win.data[ i ] = *entry;
	
	// Perform parital sort to float the new window entry to its right place
	// Since the priority must be bigger we float to the start. The only
	for( j = i-1 ; j >= 0 ; j-- ) {
		if( ( ivec_win_cmp( vec, &(vec->win.data[ j ]),
				    &(vec->win.data[ j + 1]) )) == 1 ) {
		
			info_win_entry_t  tmp = vec->win.data[ j ];
			vec->win.data[ j ] = vec->win.data[ j + 1 ];
			vec->win.data[ j + 1 ] = tmp;
		}
		else
			break;
	}
	return 1;
}

/****************************************************************************
 *
 ***************************************************************************/
static void
ivec_reset_entry( ivec_entry_t *entry, struct timeval *curTime ) {

	unsigned int del_size = entry->info->hdr.fsize - NHDR_SZ;
			
	entry->isdead            = 1;
	entry->info->hdr.status  = INFOD_DEAD_VEC_RESET;
	entry->info->hdr.cause   = 0;
	entry->info->hdr.time    = *curTime;
        entry->info->hdr.external_status = EXTERNAL_STAT_NO_INFO;
        
	/* empty all the fields */
	if( del_size != 0 )
		bzero( entry->info->data, del_size );
}

/****************************************************************************
 * Reset all the vector informarion. In the case that something happened in
 * time (went back to past)
 ***************************************************************************/
static void
ivec_reset( ivec_t vec ) {

	int i = 0;
	struct timeval  currTime;
	
	/* Reset all the vector entries */
	gettimeofday( &currTime, NULL );
	for( i = 0; i < vec->vsize; i++ )
		ivec_reset_entry( &( vec->vec[ i ]), &currTime );
	
	vec->numAlive = 0;
	
	/* Reset all the window entries */
	bzero( vec->win.data, vec->win.size * INFO_WIN_ENTRY_SIZE );
	bzero( vec->win.aux, vec->win.size * sizeof(int));
}

/****************************************************************************
 * Update a single vector entry
 ***************************************************************************/
int
infoVecUpdate( ivec_t vec, node_info_t* update, int size,
	       unsigned int priority  )
{
	ivec_entry_t *entry = NULL;
	struct timeval  curr_time;
	
      	if( !vec || !update || size <= 0  ) {
		debug_lr(  VEC_DEBUG, "Error: args, vec update %d\n", size );
		return 0;
	}

	if( update->hdr.fsize > size ) {
		debug_lr(  VEC_DEBUG, "Error: size too big %d %d buffer is not big enough\n",
			   update->hdr.fsize, size);
		return 0;
	}

	gettimeofday( &curr_time, NULL );

	/* Test that the time is moving forwared. If local time was updated it
	   might move backword (it did happend once)
	*/
	if( timercmp( &curr_time, &(vec->prev_time), < )) {
		debug_lr( WIN_DEBUG, "Problem with time: got time in the past"
			  " Curr ( %u, %u), Prev( %u, %u )\n",
			  curr_time.tv_sec, curr_time.tv_usec,
			  vec->prev_time.tv_sec, vec->prev_time.tv_usec );

		ivec_reset( vec );
		vec->prev_time = curr_time;
		return 0;
	}
	vec->prev_time = curr_time;
	
	
	if( timercmp( &(curr_time), &( update->hdr.time ), < )) {
		debug_lg( WIN_DEBUG, "Information from the future!? Pe (%d) "
			  "Cur ( %u, %u ) Age ( %u, %u )\n",
			  update->hdr.pe,
			  curr_time.tv_sec,
			  curr_time.tv_usec,
			  update->hdr.time.tv_sec,
			  update->hdr.time.tv_usec );
		return 0;
	}

	curr_time.tv_sec  -= vec->max_age;
	
	if( timercmp( &(curr_time), &( update->hdr.time ), > )) {
		debug_lg( WIN_DEBUG, "Age of %d is out of bounds. "
			  "Max allowed ( %u, %u ) Age ( %u, %u )\n",
			  update->hdr.pe,
			  curr_time.tv_sec,
			  curr_time.tv_usec,
			  update->hdr.time.tv_sec,
			  update->hdr.time.tv_usec );
		return 0;
	}

	/* Find the entry in the vector */
	int index;
	if( !(entry = infoVecFindByIP( vec, &update->hdr.IP, &index ))) {
		debug_ly( VEC_DEBUG, "Failed updating  %s. No such IP\n",
			  inet_ntoa(update->hdr.IP) );
	 	return 0;
	}
	
	// Updating the entry only if its creation time is newer than the time
	// already stored in the vector.
	if( timercmp( &(entry->info->hdr.time), &(update->hdr.time), < ) ) {
		
		info_win_entry_t dummy;

		debug_lb(VEC_DEBUG, "Updating entry (%s) fsize %d %d %d\n",
			 inet_ntoa(entry->info->hdr.IP),
			 entry->info->hdr.fsize,
			 update->hdr.fsize,
			 size);

		// We need to update the local node priority. Each node is
		// responsible for its own priority. If the priority is 0 we
		// do not update, since the localPrio will be decreased to 0
		// automatically.
		if(index == vec->localIndex && priority > 0)
			vec->localPrio = priority;
		
		/* realloc mem if required  */
		if( entry->info->hdr.fsize < update->hdr.fsize ) {
			if( !( entry->info = realloc( entry->info,
						      update->hdr.fsize ))) {
				debug_lr( VEC_DEBUG, "Error: realloc\n" );
				return 0;
			}
			entry->info->hdr.fsize = update->hdr.fsize;	
		}

		// Coping the data 
		memcpy( entry->info->data, update->data, update->hdr.fsize - NODE_HEADER_SIZE);
		// Taking only the necessary fields from the header
		entry->info->hdr.status = update->hdr.status;
		entry->info->hdr.cause = update->hdr.cause;
		entry->info->hdr.param = update->hdr.param;
		entry->info->hdr.psize = update->hdr.psize;
		entry->info->hdr.time = update->hdr.time;
                entry->info->hdr.external_status = update->hdr.external_status;
                
		entry->reserved = 0;

		/*
		 * Entry was dead an now it is alive
		 */
		if( ( entry->isdead == 1 ) &&
		    (update->hdr.status & INFOD_ALIVE ))
		{
			entry->isdead = 0;
			vec->numAlive++;
			entry->info->hdr.cause = INFOD_ALIVE;
		}
		/*
		 * Entry was alive and now is dead
		 */
		if( ( entry->isdead == 0 ) &&
		    !( update->hdr.status & INFOD_ALIVE ))
		{
			ivec_kill_entry(vec, entry, update->hdr.cause);
			
		}

		/* update the window */
		//dummy.pe       = update->hdr.pe;
		//dummy.size     = size;
		dummy.index    = index;
		dummy.priority = priority;
		
		ivec_update_win( vec, &dummy );
		return 1;
	}
	
	debug_lr( VEC_DEBUG, "Not updating because time is older IP %s\n",
		  inet_ntoa(update->hdr.IP));

	return 0;
}
/*
 * Performing a kill on an entry. This will be called from the infoVecPunish and
 * by the infoVecUpdate.
 * Assumption: The time field of the entry is filled.
 * The function set the entry as dead and take keep the time it took the death
 * message to propagate through the cluster
 */
void ivec_kill_entry(ivec_t vec, ivec_entry_t *entry, unsigned int cause)
{
	ivec_death_log_t   *dm;
	int                 maxSize = INFOVEC_DEATH_LOG_SIZE;
	int                 pos = 0;
	struct timeval      currTime;
	
	dm = &(vec->deathLog);

	// Setting the death info
	vec->numAlive--;
	entry->isdead = 1;
	entry->info->hdr.cause = cause;

	// The place to add this death measure
	
	// No room left so cycling
	if(dm->size >= maxSize) {
		
		memcpy(&dm->ips[1], dm->ips, (dm->size-1)*sizeof(struct in_addr));
		memcpy(&dm->deathPropagationTime[1],
		       dm->deathPropagationTime,
		       (dm->size-1)*sizeof(float));
                dm->size--;
        }

        pos = dm->size;
        dm->size++;

	// Keeping the details of the newly dead node
	dm->ips[pos] = entry->info->hdr.IP;
	gettimeofday(&currTime, NULL);
	dm->deathPropagationTime[pos] = (float)compute_age( &(entry->info->hdr.time), &currTime )/MILLI;
}
/* /\**************************************************************************** */
/*  * Remove an entry from the window */
/*  ***************************************************************************\/ */
/* void */
/* ivec_win_remove( ivec_t *vec, node_t pe ) { */

/* 	int i = 0; */
/* 	if( !vec || pe <= 0 ) { */
/* 		debug_lr(  VEC_DEBUG, "Error: args, win_remove\n" ); */
/* 		return; */
/* 	} */

/* 	for( i = 0 ; i < vec->win.size ; i++ ) { */
/* 		if( vec->win.data[i].pe == pe ) { */
/* 			int j = 0; */
/* 			for( j = i ; j < vec->win.size - 1 ; j++ ) */
/* 				vec->win.data[j] = vec->win.data[j+1]; */
			
/* 			bzero( &(vec->win.data[vec->win.size-1]), WENTRY_SZ ); */
/* 			return; */
/* 		} */
/* 	} */
/* } */

/****************************************************************************
 * Punish the vector entry (e.g., connect failed)
 ***************************************************************************/
int
infoVecPunish( ivec_t vec, struct in_addr *ip, unsigned int cause )
{
	ivec_entry_t *entry = NULL;
	int index = -1;
	
	if( !vec ) {
		debug_lr(  VEC_DEBUG, "Error: args, vec punish\n" );
		return 0;
	}

	if( !( entry = infoVecFindByIP( vec, ip, &index ))){
		debug_lr( VEC_DEBUG, "Error: ip not found --> %s\n",
			  inet_ntoa(*ip) ) ;
		return 0;
	}
	// All the rest is done only if the node was alive before and now is
	// dead
	if( entry->isdead == 0  ) {
		info_win_entry_t dummy;
		unsigned int del_size = entry->info->hdr.fsize - NHDR_SZ;

		gettimeofday( &(entry->info->hdr.time), NULL );
		entry->info->hdr.status = cause ;
		ivec_kill_entry(vec, entry, cause);

		/* empty all the fields */
		if( del_size != 0 )
			bzero( entry->info->data, del_size );

		vec->lastDeadIP = *ip;

		/* insert the dead node into the window */
		dummy.index    = index;
		//dummy.pe       = pe;
		//dummy.ip       = *ip;
		//dummy.size     = entry->info->hdr.fsize;

		// FIXME next line is redundant
		//gettimeofday( &(entry->info->hdr.time), NULL );

		// Setting the newly dead node to have the maximal prio so its
		// information will tavel fast to all the other nodes
		// Since we are the one who punish the entry, it means that we
		// directly detected that this node is dead
		dummy.priority = vec->deadStartPrio;
		ivec_update_win( vec, &dummy );
	}
	else {
		if( cause != INFOD_DEAD_AGE )
			entry->info->hdr.status = cause ;
			entry->info->hdr.cause = cause;
	}
	return 1;
}

static 
int ivec_calc_replay_size( ivec_entry_t** ivecptr, int size ) {
	int rep_size = 0, i = 0;

	rep_size = INFO_REPLAY_SIZE;
	
	for( i = 0; i < size ; i++ ) {
		rep_size += INFO_REPLAY_ENTRY_SIZE;
		if( ivecptr[i] != NULL )
			rep_size += ivecptr[i]->info->hdr.fsize;
	}
	return rep_size;
}


/****************************************************************************
 * Pack the query replay to a single memory buffer which can be sent over
 * the network.
 * If the size of the message is <= size of buff, then buff is used else
 * memory is allocated
 ***************************************************************************/
char *
infoVecPackQueryReplay( ivec_entry_t** ivecptr, int size, char *buff, int *buffSize)
{
	
	info_replay_t   *rep      = NULL;
	void            *rep_buff = NULL;
	int              rep_size  = 0;
    
	int              i = 0;
	struct timeval   curtime;
	void            *base_ptr;
	int              cur_len;
	
	/* Get the size of the reply */ 
	rep_size = ivec_calc_replay_size( ivecptr, size );

	if( rep_size < *buffSize )
		rep = (idata_t*)buff;
	else {
		if( !( rep_buff = malloc( rep_size ))){
			debug_lr( VEC_DEBUG, "Error: Malloc\n");
			return NULL;
		}
		rep = (idata_t*)rep_buff;
	}

	/* points to the data area of the reply  */
	base_ptr = ((void*)(rep->data));
	cur_len = 0;
	gettimeofday( &curtime, NULL );
	
	/* Arrange the reply in the sent buffer */       
	for( i = 0 ; i < size ; i++  )
	{
		idata_entry_t *cur = ((idata_entry_t*)(base_ptr + cur_len));
		cur_len      += INFO_REPLAY_ENTRY_SIZE;
		cur->size     = INFO_REPLAY_ENTRY_SIZE;
		cur->valid    = 0;
		cur->name[0]  = '\0';
		
		if( ivecptr[i] != NULL ) {
			
			cur->valid = 1;
			strncpy( cur->name, ivecptr[i]->name,
				 MACHINE_NAME_SZ -1 );
			cur->size  += ivecptr[i]->info->hdr.fsize;
			cur_len    += ivecptr[i]->info->hdr.fsize;
			
			memcpy( cur->data, ivecptr[i]->info,
				ivecptr[i]->info->hdr.fsize );
			ivec_time2age( cur->data, &curtime );
		}
	}

	rep->num      = size;
	rep->total_sz = cur_len + INFO_REPLAY_SIZE;

	if( rep->total_sz != rep_size ) {
		debug_lr( VEC_DEBUG, "Error: reply size\n" );
		goto exit_with_error;
	}
	*buffSize = rep_size;
	return (char *)rep;

 exit_with_error:
	if( rep_buff ) {
		free( rep_buff );
		rep_buff = NULL;
	}
	return NULL;
}



/****************************************************************************
 * Return all the vector enteries
 ***************************************************************************/
ivec_entry_t**
infoVecGetAllEntries( ivec_t vec ) {

	ivec_entry_t **ret = NULL;
	struct timeval curtime ;
	unsigned long age = 0;
	int i = 0;
      
	if( !vec ) {
		debug_lr( VEC_DEBUG, "Error: args, ivec_ret_all\n");
		return NULL;
	}

	if( !(ret = (ivec_entry_t**) malloc( vec->vsize *
					     sizeof(ivec_entry_t*)))) {
		debug_lr( VEC_DEBUG, "Error: malloc, ivec_ret_all\n");
		return NULL;
	}
      
	gettimeofday( &curtime, NULL );
	for( i = 0 ; i < vec->vsize ; i++ ) {
		age = compute_age( &(vec->vec[i].info->hdr.time), &curtime );
		if( age > vec->max_age )
			infoVecPunish( vec, &vec->vec[i].info->hdr.IP,
				       INFOD_DEAD_AGE);
		ret[i] = &(vec->vec[i]);
	}

	return ret;
}


ivec_entry_t**
infoVecGetWindowEntries( ivec_t vec, int *winSize )
{
	ivec_entry_t **ret = NULL;
	int i = 0;
      
	if( !vec ) {
		debug_lr( VEC_DEBUG, "Error: args, infoVecGetWindowEntries\n");
		return NULL;
	}

	// Calculating the size of window
	gettimeofday( &vec->currTime, NULL );
	vec->win.calcWinSizeFunc( vec );
	if( !(ret = (ivec_entry_t**) malloc( (vec->win.sendSize + 1) *
					     sizeof(ivec_entry_t*)))) {
		debug_lr( VEC_DEBUG, "Error: malloc, infoVecGetWindowEntries\n");
		return NULL;
	}
	
	// Adding the local entry to the result	
	ret[0] = &(vec->vec[vec->localIndex]);

	// Adding all the rest of the window entries
	for( i = 0 ; i < vec->win.sendSize ; i++ ) {
		int index = vec->win.data[i].index;
		ret[i+1] = &(vec->vec[index]);
	}

	*winSize = vec->win.sendSize + 1;
	return ret;
}

/****************************************************************************
 * Return the information about the 'pes' in the given array
 ***************************************************************************/
ivec_entry_t**
infoVecGetEntriesByIP( ivec_t vec, struct in_addr* ips, int size ) {

	struct timeval curtime;
	ivec_entry_t **ret = NULL;
	unsigned long age = 0;
	int i = 0, index = -1;
	
      
	if( !vec || !ips || ( size <= 0 ) ) {
		debug_ly( VEC_DEBUG, "Error: args, infoVecGetEntriesByIP\n");
		return NULL;
	}

	if( !(ret = (ivec_entry_t**) malloc( size * sizeof(ivec_entry_t*)))) {
		debug_ly( VEC_DEBUG, "Error: malloc, infoVecGetEntriesByIP\n");
		return NULL;
	}

	/* initialize the return vector */
	bzero( ret, size * sizeof(ivec_entry_t*) );
	gettimeofday( &curtime, NULL );
	
	for( i = 0 ; i < size; i++ ) {
		ivec_entry_t *cur = infoVecFindByIP( vec, &ips[i], &index );
		if( !cur )
			continue;
		
		age = compute_age( &(cur->info->hdr.time), &curtime );
		if( age > vec->max_age )
			infoVecPunish(vec, &cur->info->hdr.IP, INFOD_DEAD_AGE );
		ret[i] = cur;
	}
	return ret;
}

/****************************************************************************
 * return the information about a continuous set of pes
 ***************************************************************************/
ivec_entry_t**
infoVecGetContIPEntries( ivec_t vec, struct in_addr *baseIP, int size ) {

	ivec_entry_t     **ret = NULL;
	struct timeval     curtime;
	unsigned long      age = 0;
	int                i = 0, index = -1;

	if( !vec || ( size <= 0 )) {
		debug_ly( VEC_DEBUG, "Error: args, infoVecGetContIPEntries\n" );
		return NULL;
	}

	if( !(ret = (ivec_entry_t**) malloc( size * sizeof(ivec_entry_t*)))) {
		debug_ly( VEC_DEBUG, "Error: malloc, infoVecGetContIPEntries\n" );
		return NULL;
	}

	/* initialize the return vector */
	bzero( ret, size*sizeof(ivec_entry_t*));
	gettimeofday( &curtime, NULL ) ;

	struct in_addr tmpIP = *baseIP;
	for( i = 0 ; i <size ; i++ ) {

		ivec_entry_t *cur = infoVecFindByIP( vec, &tmpIP, &index );
		// Incrementing the ip to the next one
		ipInc(&tmpIP);

		if( !cur )
			continue;
		
		age = compute_age( &(cur->info->hdr.time), &curtime );
		if( age > vec->max_age )
			infoVecPunish( vec, &cur->info->hdr.IP, INFOD_DEAD_AGE );
		ret[i] = cur;
	}
	return ret;
}


/****************************************************************************
 * Returns all the nodes up to the age specified in time
 ***************************************************************************/
ivec_entry_t**
infoVecGetEntriesByAge( ivec_t vec, unsigned long max_age, int* size ) {

	ivec_entry_t    **ret = NULL;
	struct timeval    curtime;
	unsigned long     age = 0;
	int               i = 0, j = 0;

	/* Move from seconds to milli seconds */
	max_age *= MILLI;
    
	if( !vec ) {
		debug_ly( VEC_DEBUG, "Error: args, infoVecGetEntriesByAge\n" );
		return NULL;
	}

	if( !(ret = (ivec_entry_t**)
	      malloc( vec->vsize * sizeof(ivec_entry_t*)))) {
		debug_ly( VEC_DEBUG, "Error: malloc, info_vec_ret_up2age\n");
		return NULL;
	}

	bzero( ret, vec->vsize * sizeof(ivec_entry_t*));
	gettimeofday( &curtime, NULL );

	/* get the nodes with info up to the given age */
	for( i = 0, j = 0 ; i < vec->vsize ; i++ ) {
		
		ivec_entry_t *cur = &(vec->vec[i]);
		if( cur->isdead != 0 )
			continue;
		
		age = compute_age( &(cur->info->hdr.time), &curtime );
		if( age < max_age )
			ret[ j++ ] = cur;
		else if( age > vec->max_age )
			infoVecPunish( vec, &cur->info->hdr.IP, INFOD_DEAD_AGE );
	}

	*size = j ;
	return ret;
}


/****************************************************************************
 * The window is supposed to always be sorted (using the win_cmp func).
 * When the window is requested, infoVec need to decide how much entries
 * participates in the window. In case of fixed window the maximal size is
 * known but the actuall size can be somaller, for example due to dead nodes.
 ***************************************************************************/
static int calcWinSizeFixed( ivec_t vec )
{
	int           i = 0;
	
	//bzero( vec->win.aux, vec->win.size * sizeof(int));
	
	/* perfrom bubble sort, begin by copying to the aux  */
	for( i = 0; i < vec->win.fixWinSize ; i++ ) {
		
		// Unused window entry contain -1 in thier index field
		if( vec->win.data[ i ].index == -1 )
			break;
		
		//vec->win.aux[ i ] = i;
	}

	vec->win.sendSize = i;

	return 1;
}

/* Selecting the number of entries which will compose the new window requested.
 * This is done by taking all the window entry which are upto vec->win.uptoAge.
 * The maximal size is the vec->win.win_size;
 * 
*/
static int calcWinSizeUptoAge( ivec_t vec )
{
	int           i = 0;
	
	for( i = 0; i < vec->win.size ; i++ ) {
		int   index =  vec->win.data[ i ].index;
		unsigned long entAge;
		int   prio;
		
		if( index == -1 )
			break;

		// Check that the age of the entry is still alowable
		entAge = compute_age(&vec->vec[index].info->hdr.time, &vec->currTime);
		prio   = vec->win.data[i].priority;

		// We break the window only if the prio is 0 (which is assumed
		// to be the normal prio. And the age is too old.
		if(prio == 0 && entAge > (vec->win.uptoAge)) {
			// We decrement i by one since the current entry is not
			// up to the requested age
			// A big bug because i is the index of the firts old entry which is
			// also the number of good entries.
			//i--;
			break;
		}
	}
	
	vec->win.sendSize = i;
	// We assume this will be called ever
	return 1;
}

inline int addEntToBuff(ivec_t vec, 
			info_msg_entry_t *msgEnt, int size,
			int index, int sizeFlag, unsigned int *prio)
{
	ivec_entry_t      *vecEnt = NULL;
	int                nodeInfoSize=0;
	
	vecEnt = &(vec->vec[ index ]);

	if( sizeFlag )
		nodeInfoSize = vecEnt->info->hdr.fsize;
	else
		nodeInfoSize = vecEnt->info->hdr.psize;

	// Checking that there is ehough space in the 
	if(nodeInfoSize > size) {
		mlog_bn_dr("vec", "Error remaining space in message in not big enough %d < %d\n", size, nodeInfoSize);	
		return 0;
	}

	msgEnt->size     = INFO_MSG_ENTRY_SIZE + nodeInfoSize;
	msgEnt->priority = *prio; 
	
	memcpy( msgEnt->data, vecEnt->info, nodeInfoSize );
	ivec_time2age( msgEnt->data, &vec->currTime );
	return 1;
}

/****************************************************************************
 * Get information message (the window to be sent to another daemon)
 * The buffer returned is a static buffer which will be used again next
 * time this function is called.
 ***************************************************************************/
void*
infoVecGetWindow( ivec_t vec, int *size, int size_flag  ) {

	unsigned int       total = 0;
	unsigned int       remaining_space;
	info_msg_t        *msg  = NULL;
	info_msg_entry_t  *entryPtr = NULL;
	void              *data = NULL;
	//int               *aux = NULL;
	
	if( !vec || !size ) {
		debug_lr( VEC_DEBUG, "Error: args, infoVecGetWindow\n" );
		return NULL;
	}

	bzero( vec->msg_buff, vec->msg_buff_size );
	msg        = vec->msg_buff;
	data       = msg->data;
	entryPtr   = msg->data;

	ivec_print_win( vec );
	gettimeofday( &vec->currTime, NULL );
	
	/* Add the local entry */
	remaining_space = vec->msg_buff_size - 4096;
	addEntToBuff(vec, entryPtr, remaining_space, vec->localIndex, size_flag, &vec->localPrio);
	// Decreasing the priority by one as part of the priority decay
	if( vec->localPrio > 0 )
		vec->localPrio--;
	
	// Advancing past the first entry
	total     += entryPtr->size ;
	remaining_space -= entryPtr->size;
	entryPtr   = data + total;

	vec->win.calcWinSizeFunc( vec );
	//aux = vec->win.aux;
	
	// The starting value is one since the local entry is always young enough/should be
	// in the out going window
	int nodesInWindow = 1;
	/* Add the other window entries */
	for(int i = 0 ; i < vec->win.sendSize; i++, nodesInWindow++ ) {
		int res;
		// End of aux reached
		//if( aux[ i ] == - 1 )
		//break;
		// End of window reached
		//if( vec->win.data[ aux[ i ]].index == -1 )
		//break;

		// Another sanity check
		//if( vec->win.data[ aux[ i ]].index < 0 ||
		//  vec->win.data[ aux[ i ]].index >= vec->vsize )
		//return NULL;
		
		res = addEntToBuff(vec, entryPtr, remaining_space, vec->win.data[i].index, size_flag,
		         	     (unsigned int *)&(vec->win.data[i].priority));
		
		if(!res) {
			mlog_bn_dr("vec", "Error adding entry to window message (not enough space) skipping ..\n");
			break;			
		}	
		// Deacreasing the priority by one for each window sent
		if( vec->win.data[ i ].priority > 0 )
			vec->win.data[ i ].priority--;
				
		total    += entryPtr->size;
		remaining_space -= entryPtr->size;
		entryPtr  = data + total;
	}

	total             += INFO_MSG_SIZE;
	msg->signature     = vec->signature;
	msg->num           = nodesInWindow;
	msg->tsize         = total;
	*size              = msg->tsize;

	debug_ly( WIN_DEBUG, "Getting Window Window: sendSize %d winSize %d \n",
		  vec->win.sendSize, nodesInWindow);

	// Adding the measurment of the window size
	infoVecDoWinSizeMeasure(vec, nodesInWindow);

	return vec->msg_buff;
}

/****************************************************************************
 * Handle an info message from another infod
 ***************************************************************************/
int
infoVecUseRemoteWindow( ivec_t vec, void *buff, int size ) {
	
	info_msg_t           *msg = (info_msg_t*)buff;
	info_msg_entry_t     *ptr  = NULL;
	void                 *data = NULL;
	struct timeval        curtime;
	unsigned int          i = 0,  curlen = 0;
	
	if( !vec || !buff || ( size < INFO_MSG_SIZE ) ) {
		debug_lr( VEC_DEBUG, "Error: args, handle_msg\n" );
		return 0;
	}
	if( msg->signature != vec->signature ) {
		debug_lr( VEC_DEBUG, "Error: invalid infod message\n" );
		return 0;
	}
	
	gettimeofday( &curtime, NULL );

		
	data = msg->data;
	debug_ly( WIN_DEBUG, "\n-----> Got remote window. Window entries: ( %u, %u )\n\n",
		  curtime.tv_sec, curtime.tv_usec );
	
	for( i = 0; i < msg->num ; i++ )
	{
		int info_sz = 0;
		ptr = data + curlen;
		// Skeeping the entry if it belong to local node
		if (ipEqual(&ptr->data->hdr.IP, &vec->localIP)) {
			curlen += ptr->size;
			continue;
		}
		
		info_sz = ptr->size - INFO_MSG_ENTRY_SIZE;

		// Moving the time of the new information from age to creation time
		ivec_age2time( ptr->data, &curtime );
		debug_ly( WIN_DEBUG, "Win Entry: IP (%s) st(%d) Time( %d, %d )\n",
			  inet_ntoa(ptr->data->hdr.IP),
			  ptr->data->hdr.status,
			  ptr->data->hdr.time.tv_sec,
			  ptr->data->hdr.time.tv_usec );
		// Updating the vector and the window
		infoVecUpdate( vec, ptr->data, info_sz, ptr->priority );
		curlen += ptr->size;
	}
	return 1;
}


/****************************************************************************
 * Select a random node
 ***************************************************************************/
static int
ivec_get_any_rand( ivec_t vec ) {

	int rand_node = 0;
	struct in_addr ip;
	do {
		rand_node = (int)( rand() % vec->vsize );

	} while( ipEqual(&vec->vec[rand_node].info->hdr.IP,
			 &vec->localIP) );

	indexToIp(vec, rand_node, &ip);
	//debug_ly(VEC_DEBUG, "*******************> randomizing from  ANY        %s\n",
	//inet_ntoa(ip));
	return rand_node;
}

/****************************************************************************
 * Select only and active randome node
 ***************************************************************************/
static int
ivec_get_rand_active( ivec_t vec )
{

	int rand_node = 0, num = 0, i = 0;
	int live_nodes = vec->numAlive;
      

	// If the local node is alive we does not take it into account
	if(vec->vec[vec->localIndex].isdead == 0)
		live_nodes--;
	
	// No live nodes or only the local node is alive
	if( live_nodes <= 0 ) {
		return -1;
	}

	// An easy case
	if( vec->numAlive == vec->vsize )
		return ivec_get_any_rand( vec );
	
	// Both dead nodes and live nodes exists
	rand_node = (int)( rand() % live_nodes);
	for( i = 0; i < vec->vsize ; i++ ) {
		if( ( vec->vec[i].isdead == 0 ) &&
		    ( !ipEqual(&(vec->vec[i].info->hdr.IP), &(vec->localIP) ) ))
		{
			if( num == rand_node )
				break;
			num++;
		}
	}
	if( i == vec->vsize ) {
		debug_lr( VEC_DEBUG, "Error: rand node %d %d\n",
			  rand_node, num );
		return -1;
	}

	struct in_addr ip;
	indexToIp(vec, rand_node, &ip);
	//debug_ly(INFOD_DEBUG, "+++++++++++++++++> randomizing from    ACTIVE        %d  %s\n",
	//rand_node, inet_ntoa(ip));
		
	return i;
}

/****************************************************************************
 * Select a random node.
 * only_alive == 1, return only a node that is up.
 *
 * Add the following code for debugging
 *  time_t rawtime;
 *	struct tm * timeinfo;
 *  time ( &rawtime );
 *	timeinfo = localtime ( &rawtime );
 *		debug_lg( VEC_DEBUG, "Only Alive Pe=%d\t%s\n\n", cur,
 *			  asctime (timeinfo));
 ***************************************************************************/
int
infoVecRandomNode( ivec_t vec, int onlyAlive, struct in_addr *ip ) {

	int index = 0;

	// No need for randomness but we need to check that we are not
	// the only node in the vector.
	if( vec->vsize == 1 ) {
		// We are in the vector so can not random other nodes
		if(vec->localIndex == 0)
			return 0;
		index = 0;
	}
	else if( onlyAlive == 1 ) {
		if(vec->numAlive > 0) {
			index = ivec_get_rand_active( vec );
			if(index == -1)
				return 0;
		}
		else return 0;
	}
	else
		index = ivec_get_any_rand( vec );
	
	indexToIp(vec, index, ip);
	debug_ly(INFOD_DEBUG, "%%%%%%%%%%%%%%%  Random node   %s\n", inet_ntoa(*ip));
	return 1;
}
/****************************************************************************
 * Getting the oldest alive node, for the pull query
 ***************************************************************************/
int
infoVecOldestNode( ivec_t vec, struct in_addr *ip ) {

	unsigned int    i = 0, oldestIndex = -1;
	unsigned long   age = 0;
	unsigned long   oldestAge = 0;
	double          oldestAgeF;
	struct timeval  cur;
	int             res = 0;
	
	if( !vec ) {
		debug_lr( VEC_DEBUG, "Error: args, stats\n" );
		return 0;
	}
	
	gettimeofday( &cur, NULL );
	
	for( i = 0; i < vec->vsize ; i++ ) {
		if( vec->vec[i].isdead == 0 ) {
			
			age = compute_age( &(vec->vec[i].info->hdr.time),
					   &cur );
			if(age > oldestAge && i != vec->localIndex)
			{
				oldestAge = age;
				oldestIndex = i;
			}
		}
	}
	oldestAgeF = (double)oldestAge/(double)MILLI;

	if(oldestIndex != -1) {
		res = 1;
		indexToIp(vec, oldestIndex, ip);
		debug_ly(INFOD_DEBUG, "%%%%%%%%%%%%%%%  Oldest node:   %s (%.3f)\n",
			 inet_ntoa(*ip), oldestAgeF);
	}
	return res;
}

/****************************************************************************
 * Get the structure holding the information vector statistics
 ***************************************************************************/
int
infoVecStats( ivec_t vec, infod_stats_t *stats ) {
	
	unsigned int    i = 0, num = 0;
	unsigned long   age = 0;
	double          ageF;
	double          ageSum = 0.0;
	struct timeval  cur;
	
	if( !vec || !stats ) {
		debug_lr( VEC_DEBUG, "Error: args, stats\n" );
		return 0;
	}
	
	stats->total_num  = vec->vsize;
	stats->num_alive  = vec->numAlive;
	stats->maxage = 0.0;
	gettimeofday( &cur, NULL );
	
	for( i = 0; i < vec->vsize ; i++ ) {
		if( vec->vec[i].isdead == 0 ) {
			
			age = compute_age( &(vec->vec[i].info->hdr.time),
					   &cur );
			// Summing the ages in a double since we dont want to
			// get an overflow as we did in 256 cluster
			ageF = (double)age/(double)MILLI;
			if(ageF > stats->maxage)
				stats->maxage = ageF;
			ageSum += ageF;
			num++;
		}
	}

	//fprintf(stderr, "Max age %.1f\n", stats->maxage);
	stats->avgage = (double)ageSum/(double)num;
	return 1;
}

/****************************************************************************
 * Access functions
 ***************************************************************************/
ivec_entry_t*
infoVecGetVector( ivec_t vec ) {
	if( vec )
		return vec->vec ;
	return NULL;
}

int
infoVecGetSize( ivec_t vec ){
	if( vec )
		return vec->vsize ;
	return -1;
}

int
infoVecNumAlive( ivec_t vec ){

	if( vec )
		return vec->numAlive ;
	return 0;
}

int
infoVecNumDead( ivec_t vec ){

	if( vec )
		return( vec->vsize - vec->numAlive );
	return -1;
}

int
infoVecGetWinSize( ivec_t vec ) {

	int i = 0;
	if( !vec )
		return -1;
	
	for( i = 0 ; i < vec->win.size ; i++ )
		if( vec->win.data[i].index == -1 )
			break;
	return i;
}

/****************************************************************************
 * Print. Used for debugging
 ***************************************************************************/
static void
ivec_print_entry( ivec_entry_t *entry, struct timeval *cur ) {

	unsigned long age = compute_age( &(entry->info->hdr.time), cur );
	struct in_addr addr;
	
	addr = entry->info->hdr.IP;
	//network_order(&addr); 
	
	debug_ly( VEC_DEBUG, "%9s D=%d", entry->name, entry->isdead );
	debug_lg( VEC_DEBUG, "\tPe (%3ld) IP (%15s) St (%ld) PSz (%d) FSz (%d) "
		  "Time ( %lu, %lu )\n",
		  entry->info->hdr.pe, inet_ntoa(addr), entry->info->hdr.status,
		  entry->info->hdr.psize, entry->info->hdr.fsize,
		  age/MILLI, age%MILLI );
}

static void
ivec_print_vec( ivec_t vec ) {

	int i = 0;
	struct timeval cur;

	gettimeofday( &cur, NULL );
	debug_ly( VEC_DEBUG, "\n========= Vector ==========\n\n" );
	
	for( i = 0 ; i < vec->vsize ; i++ )
		ivec_print_entry( &(vec->vec[i]), &cur );
	
	debug_ly( VEC_DEBUG, "\n===========================\n" ); 
}

/****************************************************************************
 * Printing the alive entries of the vector sorted by their age
 ***************************************************************************/
ivec_t tmp_glob_vec;

int sortIndexByAge(const void *a, const void *b)
{
	int indexA = *(int *)a;
	int indexB = *(int *)b;
	
	ivec_entry_t *entryA = &(tmp_glob_vec->vec[indexA]);
	ivec_entry_t *entryB = &(tmp_glob_vec->vec[indexB]);

	return timercmp(&(entryA->info->hdr.time), &(entryB->info->hdr.time), <);
}

static void
ivec_print_by_age( ivec_t vec)
{
	int  i, j;
	int  alive = vec->numAlive;
	int *indexArr;
	struct timeval cur;

	gettimeofday( &cur, NULL );

        if(alive <= 0) {
                debug_lr( VEC_DEBUG, "\n======= Vector doe not conatian alive nodes ========\n\n" );
                return;
        }
        
	// Allocating memory in the first time to hold the nodes and their age
	if((indexArr = malloc(sizeof(int) * alive)) == NULL) {
		debug_lr(VEC_DEBUG, "Error allocating memory for indexArr\n");
		return;
	}
	// Filling the array with the live index
	for( i = 0, j = 0; i < vec->vsize ; i++ ) {
		if(vec->vec[i].isdead)
			continue;

		indexArr[j++] = i;
	}
	if(alive != j) {
		debug_lr(VEC_DEBUG, "Error j != alive\n");
		alive=j;
	}
	// Sorting by age
	tmp_glob_vec = vec;
	qsort(indexArr, alive, sizeof(int), sortIndexByAge);
	// Printing
	debug_ly( VEC_DEBUG, "\n========= Vector Sorted By Age ========\n\n" );
	for( j = 0 ; j < alive ; j++ )
		ivec_print_entry( &(vec->vec[indexArr[j]]), &cur );
	
	debug_ly( VEC_DEBUG, "\n=======================================\n" ); 
	free(indexArr);
}



static void
infoVecPrintContIPs(ivec_t vec ) {

	int i = 0;
	struct in_addr addr_norder;
	debug_ly( VEC_DEBUG, "\n========= Cont mapping ==========\n\n" );
	for( i = 0 ; i < vec->contIPs.size ; i++ ) {
		addr_norder = vec->contIPs.data[i].ip;
		network_order(&addr_norder);
		debug_lb( VEC_DEBUG, "IP %s Size (%ld) Ind (%ld)\n",
			  inet_ntoa(addr_norder), vec->contIPs.data[i].size,
			  vec->contIPs.data[i].index );
	}
	debug_ly( VEC_DEBUG, "\n================================\n" );
}


static void
ivec_print_win( ivec_t vec ) {

	int i = 0;
	unsigned long age = 0;
	ivec_entry_t *entry = NULL;
	struct timeval cur;

	gettimeofday( &cur, NULL );
	debug_ly( WIN_DEBUG, "\n========= Window  ==========\n\n" );
	
	for( i = 0 ; i < vec->win.size; i++ ) {

		if( vec->win.data[i].index == -1 )
			break;
		
		entry = &(vec->vec[ vec->win.data[i].index ]);
		age = compute_age( &(entry->info->hdr.time), &cur );

		debug_lb( WIN_DEBUG, "IP (%s) Pri (%u) Size (%ld) Indx (%ld) "
			  "Age (%ld) (%ld)\n",
 			  inet_ntoa(entry->info->hdr.IP),
			  vec->win.data[i].priority,
			  entry->info->hdr.fsize,
			  vec->win.data[i].index,
			  age/MILLI, age%MILLI );
	}

	debug_ly( WIN_DEBUG, "\n============================\n" );
}

void
ivec_print_stats( ivec_t vec ) {
	infod_stats_t stats;

	// FIXME to uncomment this line once we can make it work
//	ivec_get_stats( vec, &stats );

	debug_ly( VEC_DEBUG, "\n========= Stats ==========\n" );
	debug_lg( VEC_DEBUG, "Num (%d) Alive (%d) AvgAge (%.3f)\n",
		  stats.total_num, stats.num_alive, stats.avgage ) ;
	debug_ly( VEC_DEBUG, "==========================\n" );

}

void
infoVecPrint( FILE *f, ivec_t vec, int flag ) {
	if( !vec )
		return;

	switch( flag ) {
	    case 0:
		    ivec_print_vec( vec );
		    break;
	    case 1:
		    infoVecPrintContIPs( vec );
		    break;
	    case 2:
		    ivec_print_win( vec );
		    break;
	    case 3:
		    ivec_print_stats( vec );
		    break;
	    case 4: ivec_print_by_age(vec);
		    break;
	    default:
		    return;
	}
	return;
}

// Measurements functions
void  infoVecClearDeathLog(ivec_t vec) {
	vec->deathLog.size = 0;

}
void  infoVecGetDeathLog(ivec_t vec, ivec_death_log_t *dl)
{
	*dl = vec->deathLog;
}

void   infoVecClearAgeMeasure(ivec_t vec) {
	vec->ageMeasure.im_sampleNum = 0;
	vec->ageMeasure.im_avgAge = 0.0;
	vec->ageMeasure.im_avgMaxAge = 0.0;
	vec->ageMeasure.im_maxMaxAge = 0.0;
}
void   infoVecSetAgeMeasureSamplesNum(ivec_t vec, int num) {
	vec->ageMeasure.im_maxSamples = num;
}

void infoVecDoAgeMeasure(ivec_t vec) {
	infod_stats_t is;
	// No need to measure
	if(vec->ageMeasure.im_maxSamples > 0 &&
	   vec->ageMeasure.im_sampleNum >= vec->ageMeasure.im_maxSamples)
		return;

	// Taking the stats
	infoVecStats(vec, &is);
	// First measurment
	if(vec->ageMeasure.im_sampleNum == 0) {
		vec->ageMeasure.im_avgAge = is.avgage;
	} else {
		double n = vec->ageMeasure.im_sampleNum;
		double oldAvg = vec->ageMeasure.im_avgAge;
		double oldAvgMax = vec->ageMeasure.im_avgMaxAge;
		
		vec->ageMeasure.im_avgAge =
			is.avgage * 1.0/(n+1.0) + oldAvg*(n/(n+1.0));
		vec->ageMeasure.im_avgMaxAge =
			is.maxage * 1.0/(n+1.0) + oldAvgMax*(n/(n+1.0));
		if(is.maxage > vec->ageMeasure.im_maxMaxAge)
			vec->ageMeasure.im_maxMaxAge = is.maxage;
		
		debug_ly( MEASURE_DEBUG, "n = %f old avg %.4f curr %.4f new %.4f\n",
			  n, oldAvg, is.avgage, vec->ageMeasure.im_avgAge );
	}
	vec->ageMeasure.im_sampleNum++;
}

void   infoVecGetAgeMeasure(ivec_t vec, ivec_age_measure_t *m)
{
	*m = vec->ageMeasure;
}

void infoVecClearEntriesUptoageMeasure(ivec_t vec) {
     vec->entriesUptoageMeasure.im_maxSamples = 0;
     vec->entriesUptoageMeasure.im_sampleNum = 0;
     vec->entriesUptoageMeasure.im_size = 0;
}

void infoVecSetEntriesUptoageMeasure(ivec_t vec, int samples, float *ageArr, int size) {
     vec->entriesUptoageMeasure.im_maxSamples = samples;
     vec->entriesUptoageMeasure.im_sampleNum = 0;
     if(size > INFOVEC_ENTRIES_UPTO_SIZE)
          size = INFOVEC_ENTRIES_UPTO_SIZE;
     vec->entriesUptoageMeasure.im_size = size;
     for(int i=0 ; i < size ; i++) {
          vec->entriesUptoageMeasure.im_ageArr[i] = ageArr[i];
          vec->entriesUptoageMeasure.im_entriesUpto[i] = 0.0;
     }
     printf("Setting upto size %d\n", vec->entriesUptoageMeasure.im_size);
}

void infoVecGetEntriesUptoageMeasure(ivec_t vec, ivec_entries_uptoage_measure_t *m) {
     *m = vec->entriesUptoageMeasure;
}


void infoVecDoEntriesUptoageMeasure(ivec_t vec) {
     unsigned long   age = 0;
     double          ageF;
     struct timeval  cur;

     ivec_entries_uptoage_measure_t *m = &(vec->entriesUptoageMeasure);
     int             entriesArr[INFOVEC_ENTRIES_UPTO_SIZE];
     
     // No need to measure
     if(m->im_maxSamples > 0 && m->im_sampleNum >= m->im_maxSamples)
          return;
     
     // Obtaining the current number of entries upto a given age
     gettimeofday( &cur, NULL );
     memset(entriesArr, 0, sizeof(int) * INFOVEC_ENTRIES_UPTO_SIZE);
     for(int v = 0; v < vec->vsize ; v++ ) {
          if( vec->vec[v].isdead) continue;
          
          age = compute_age( &(vec->vec[v].info->hdr.time), &cur );
          // Summing the ages in a double since we dont want to
          // get an overflow as we did in 256 cluster
          ageF = (double)age/(double)MILLI;

          // Checking if the entry age is below the limits we measure
          for(int i=0 ; i<m->im_size ; i++) {
               if(ageF <= m->im_ageArr[i])
                    entriesArr[i]++;
          }
     }

     // Computing the new average
     for(int i=0; i< m->im_size ; i++) {
          if(m->im_sampleNum == 0) {
               m->im_entriesUpto[i] = entriesArr[i];
          } else {
               double n = m->im_sampleNum;
               double oldAvg = m->im_entriesUpto[i];
               
               m->im_entriesUpto[i] =
                    entriesArr[i] * 1.0/(n+1.0) + oldAvg*(n/(n+1.0));
               
               debug_ly( MEASURE_DEBUG, "n = %f old avg %.4f curr %.4f new %.4f\n",
                         n, oldAvg, entriesArr[i], m->im_entriesUpto[i] );
          }
     }
     
     vec->entriesUptoageMeasure.im_sampleNum++;
}


// Window size measurment
// Measurements functions
void   infoVecClearWinSizeMeasure(ivec_t vec) {
	vec->winSizeMeasure.im_sampleNum = 0;
	vec->winSizeMeasure.im_avgWinSize = 0.0;
}
void   infoVecSetWinSizeMeasureSamplesNum(ivec_t vec, int num) {
	vec->winSizeMeasure.im_maxSamples = num;
}

static 
void   infoVecDoWinSizeMeasure(ivec_t vec, int currWinSize) {

	// No need to measure
	if(vec->winSizeMeasure.im_maxSamples > 0 &&
	   vec->winSizeMeasure.im_sampleNum >= vec->winSizeMeasure.im_maxSamples)
		return;

	if(vec->winSizeMeasure.im_sampleNum == 0) {
		vec->winSizeMeasure.im_avgWinSize = currWinSize;
	} else {
		double n = vec->winSizeMeasure.im_sampleNum;
		double oldAvg = vec->winSizeMeasure.im_avgWinSize;

		vec->winSizeMeasure.im_avgWinSize = currWinSize * 1.0/(n+1.0) + oldAvg*(n/(n+1.0));
		debug_ly( VEC_DEBUG, "n = %f old avg %.4f curr %.4f new %.4f\n",
			  n, oldAvg, currWinSize, vec->winSizeMeasure.im_avgWinSize );
	}
	vec->winSizeMeasure.im_sampleNum++;
}

void   infoVecGetWinSizeMeasure(ivec_t vec, ivec_win_size_measure_t *m)
{
	*m = vec->winSizeMeasure;
}


/****************************************************************************
 *                            E O F
 ***************************************************************************/
