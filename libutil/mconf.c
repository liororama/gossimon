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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>


#include <msx_common.h>
#include <msx_error.h>
#include <msx_proc.h>
#include <msx_debug.h>
#include <mapper.h>
#include <distance_graph.h>

/**
   The structure of mconf is a memory mapped file containing two parts
   The first is fixed in size and the second is the current map string which
   is not fixed in size and is placed right after the fixed object
 */

#define MCONF_MAGIC    (0xaaddff09);
#define DEFAULT_MAX_MAP_STR_SIZE   (getpagesize() * 2)
struct mconf_struct
{
	char        fileName[256];
	int         fd;
	int         mmapProt;
	struct mconf_fixed_info *info;
	int         mapStrLen;
	char       *mapStr;
	mapper_t    mapper;
	int         mapperMapVersion;  // The version of the map the mapper is
	                               // based on;
};

struct mconf_fixed_info {
	int     magic;
	int     offMode;    // value of 1 indicate going into off mode
	int     nrUsed;     // reflect the number of currently live mconf

	                   // object. <only for debugging>
	int     maxMapStrLen;
	int     mapStrLen;  // Size of map string after the fixed object 
	int     mapVersion; // A version number for the map. it start with 0 and
	                    // incremented every time the map is modified
	
	// Actuall data
	int     manualPresedenceMode; // manual block 
	int     maxGuests;  // The number maxguests should have when the cluster
	                    // is open to the world
	int     minGuestDist; // The minimum distance to a guest 
	int     maxGuestDist; // The maximal distance to a guest
	int     maxMigDist;   // The maximal distance a migrated process can go

	int     minRunningDist; // The minimal running level of mosix
	                         // processes in this node. -1 means no mosix
	                         // processes or not set
	int     mosProcNum;  // Number of mosix processes (including deputies)
	int     ownerdStatus; // Status of ownerd
};

#define MCONF_FIXED_SIZE  (sizeof(struct mconf_fixed_info))

#include <mconf.h>


typedef enum {
	MCONF_FILE_OK,
	MCONF_FILE_NOT_EXIST,
	MCONF_BAD_MAGIC,
	MCONF_FILE_BAD,
} mconf_file_status_t;


static mconf_file_status_t check_mconf_file(char *fname);
static int create_new_mconf_file(char *fname);
static inline void mconf_sync(mconf_t mconf);
//static char *guest_type_str(guest_t gt);

/** Checking if the given mconf is valid. not null, info is not null
    and not in offMode
  @param    mconf  An mconf obejct
  @return   0 if we can proceed and use this mconf object and 1 if not
*/
static inline int can_not_use_mconf(mconf_t mconf)
{
	if(!mconf || !mconf->info || mconf->info->offMode)
		return 1;
	return 0;
}
/** Starting a new dynamic configuration. This should be called when the system
    first come up. The routine create the mconf file and intialize the data to
    the defaults value. If the file already exists it is overwritten
    @param fname    File name to use for the configuration
    @return  0 on error 1 on success
*/
int start_mconf(char *fname)
{
	int         fd = -1;
	struct mconf_fixed_info fixed_info;
	mconf_file_status_t status;
	
	char *b;
	
	if(!fname)
		return 0;
	if(strlen(fname) == 0)
		return 0;
	
	status = check_mconf_file(fname);
	if(status == MCONF_FILE_BAD)
		return 0;
	else if(status == MCONF_FILE_NOT_EXIST) {
		if((fd = create_new_mconf_file(fname)) == -1)
			return 0;
	}
	else if (status == MCONF_FILE_OK) {
		if((fd = open(fname, O_RDWR)) == -1)
		{
			debug_lr(MCONF_DEBUG, "Error in open\n");
			return 0;
		}
	}
	
	// Default management valus
	fixed_info.magic = MCONF_MAGIC;
	fixed_info.offMode = 0;
	fixed_info.nrUsed = 0;
	fixed_info.mapStrLen = 0;
	fixed_info.maxMapStrLen = DEFAULT_MAX_MAP_STR_SIZE;
	fixed_info.mapVersion = 0;
	// Default information
	fixed_info.manualPresedenceMode = 0;
	fixed_info.maxGuests = 0;
	fixed_info.minGuestDist = SAME_CLUSTER_MAX_DIST;
	fixed_info.maxGuestDist = SAME_GRID_MAX_DIST;
	fixed_info.maxMigDist = SAME_GRID_MAX_DIST;
	fixed_info.minRunningDist = -1;
	fixed_info.mosProcNum = 0;
	fixed_info.ownerdStatus = 0;
	
	if(write(fd, &fixed_info, MCONF_FIXED_SIZE) != MCONF_FIXED_SIZE)
	{
		debug_lr(MCONF_DEBUG, "Error: partial write (default values)\n");
		close(fd);
		return 0;
	}

	if(!(b = malloc(DEFAULT_MAX_MAP_STR_SIZE)))
	{
		debug_lr(MCONF_DEBUG, "Error: no memory for max map str buff\n");
		return 0;
	}
	bzero(b, DEFAULT_MAX_MAP_STR_SIZE);
	lseek(fd, getpagesize(), SEEK_SET);
	if(write(fd, b, DEFAULT_MAX_MAP_STR_SIZE) != DEFAULT_MAX_MAP_STR_SIZE) {
		debug_lr(MCONF_DEBUG, "Error: pre allocating space on file for map\n");
		return 0;
	}
	close(fd);
	return 1;
}

/** Stopping the current dynamic configuration. If the file does not exists
    error is returned. This routine should be called when the system is going
    down. The file is deleted and the memory is set to indicate a stop value
 */
int stop_mconf(char *fname)
{
	mconf_file_status_t status;
	int fd = -1;
	struct mconf_fixed_info fixed_info;
	
	if(!fname)
		return 0;
	if(strlen(fname) == 0)
		return 0;
	
	status = check_mconf_file(fname);
	if(status == MCONF_FILE_BAD || status == MCONF_FILE_NOT_EXIST)
				return 0;
	else if (status == MCONF_FILE_OK) {
		if((fd = open(fname, O_RDWR)) == -1)
		{
			debug_lr(MCONF_DEBUG, "Error in open\n");
			return 0;
		}
	}
	
	// We should have a valid fd;
	fixed_info.offMode = 1;
	fixed_info.nrUsed = 0;
	fixed_info.manualPresedenceMode = 0;

	if(write(fd, &fixed_info, MCONF_FIXED_SIZE ) != MCONF_FIXED_SIZE)
		debug_lr(MCONF_DEBUG, "Error: partial write (stop mode)\n");
	close(fd);
	
	if(unlink(fname) == -1) {
		debug_lr(MCONF_DEBUG, "Error: delete file %s failed\n", fname);
		return 0;
	}
	return 1;
}

void sprintf_fixed_info(char *buff, struct mconf_fixed_info *fi)
{
	sprintf(buff,
		"Magic                %x\n"
		"Off mode             %d\n"
		"nrUsed               %d\n"
		"Manual Mode          %d\n"
		"Max Guests           %d\n"
		"Min Guest Distance   %d\n"
		"Max Guest Distance   %d\n"
		"Max Mig Distance     %d\n"
		"Min Running Distance %d\n"
		"Mosix Processes      %d\n"
		"Ownerd Status        %d\n",
		fi->magic, fi->offMode, fi->nrUsed,
		fi->manualPresedenceMode, fi->maxGuests,
		fi->minGuestDist, fi->maxGuestDist, fi->maxMigDist,
		fi->minRunningDist, fi->mosProcNum,
		fi->ownerdStatus);
	
}


void mconf_file_sprintf(char *buff, char *fname)
{
	int fd;
	struct mconf_fixed_info fi;
	if(!buff) return;
	if((fd = open(fname, O_RDONLY)) == -1) {
		sprintf(buff, "Error opening file %s\n", fname);
		return;
	}
	
	if(read(fd, &fi, MCONF_FIXED_SIZE) != MCONF_FIXED_SIZE) {
		sprintf(buff, "Partial read\n");
		return;
	}
	sprintf_fixed_info(buff, &fi);
}

void mconf_file_fprintf(FILE *stream, char *fname) {
	char buff[500];

	mconf_file_sprintf(buff, fname);
	fprintf(stream, "%s", buff);
}
void mconf_file_printf(char *fname) {
	mconf_file_fprintf(stdout, fname);
}
void mconf_sprintf(char *buff, mconf_t mconf) {
	sprintf_fixed_info(buff, mconf->info);
}
void mconf_fprintf(FILE *stream, mconf_t mconf) {
	char buff[500];
	mconf_sprintf(buff, mconf);
	fprintf(stream, "%s", buff);
}
void mconf_printf(mconf_t mconf) {
	mconf_fprintf(stdout, mconf);
}

/** Create a new mconf_t object. The fname is used to access the configuration
    file. If the file does not exists then error is reported.
 */
mconf_t mconf_init(char *fname)
{
	mconf_t mconf;
	int     fd;
	uid_t   ruid;
	int     open_mode;
	
	if(!(mconf = malloc(sizeof(struct mconf_struct))))
		return NULL;
	bzero(mconf, sizeof(struct mconf_struct));
	strncpy(mconf->fileName, fname, 255);
	mconf->fileName[255] = '\0';
	mconf->mapStr = NULL;
	mconf->mapStrLen = 0;
	mconf->mapper = NULL;
	mconf->mapperMapVersion = -1;
	// Checking if the file exists.
	ruid = getuid();

	open_mode = (ruid == 0) ? O_RDWR : O_RDONLY;
	mconf->mmapProt = (ruid == 0) ? (PROT_READ | PROT_WRITE) : PROT_READ;
	// Opening the file
	if((fd = open(fname, open_mode)) == -1)
	{
		debug_lr(MCONF_DEBUG, "open failed\n");
		//perror("open");
		goto failed;
	}
	mconf->fd = fd;
	mconf->info = mmap(0, sizeof(struct mconf_fixed_info),
			   mconf->mmapProt, MAP_SHARED,
			   fd, 0);
	if(mconf->info == MAP_FAILED) {
		debug_lr(MCONF_DEBUG, "mmap failed\n");
		//perror("mmap");
	        goto failed;
	}
	if(mconf->info->offMode == 1)
	{
		debug_lr(MCONF_DEBUG, "New mconf is in offMode\n");
		goto failed;
	}
	// There is a map in the second page so we load it
	if(mconf->info->mapStrLen) {
		int size = mconf->info->mapStrLen;
		mconf->mapStr = mmap(0, size,
				     mconf->mmapProt, MAP_SHARED,
				     fd, getpagesize());
		if(mconf->mapStr == MAP_FAILED) {
			debug_lr(MCONF_DEBUG, "mmap of map string failed\n");
			goto failed;
		}
		// The size of the loaded map is saved so we can reload if map
		// changes
		mconf->mapStrLen = size;
	}
	return mconf;
 failed:
	free(mconf);
	close(fd);
	return NULL;
}
void mconf_done(mconf_t mconf) {
	if(!mconf)
		return;

	if(mconf->mapper)
		mapper_done(mconf->mapper);
	mconf->mapper = NULL;
	mconf->mapperMapVersion = -1;
	
	munmap(mconf->info, MCONF_FIXED_SIZE);
	munmap(mconf->mapStr, mconf->mapStrLen);
	close(mconf->fd);
	free(mconf);
}

/** Using msync to synchronize the mconf object with the file
 */
static inline
void mconf_sync(mconf_t mconf)
{
	msync(mconf->info, MCONF_FIXED_SIZE, MS_SYNC | MS_INVALIDATE);
}

int mconf_get_off_mode(mconf_t mconf, int *offmode)
{
	if(!mconf) return 0;
	*offmode = mconf->info->offMode;
	return 1;
}
int mconf_set_manual_presedence_mode(mconf_t mconf)
{
	if(!mconf)
		return 0;
	mconf->info->manualPresedenceMode = 1;
	mconf_sync(mconf);
	return 1;
}
int mconf_clear_manual_presedence_mode(mconf_t mconf)
{
	if(!mconf)
		return 0;
	mconf->info->manualPresedenceMode = 0;
	mconf_sync(mconf);
	return 1;
}
	
int mconf_is_manual_presedence_mode(mconf_t mconf, int *is_mpm)
{
	if(!mconf)
		return 0;
	*is_mpm = mconf->info->manualPresedenceMode;
	return 1;
}

int mconf_set_maxguests(mconf_t mconf, int maxguests)
{
	if(!mconf || maxguests < 0)
		return 0;
	mconf->info->maxGuests = maxguests;
	mconf_sync(mconf);
	return 1;
}

int mconf_get_maxguests(mconf_t mconf, int *maxguests)
{
	if(!mconf)
		return 0;
	*maxguests = mconf->info->maxGuests;
	return 1;
}
/** Setting the minimum level of all the currently running mosix proxesses
    A value of -1 indicate there are no current mosix processes
    (value of 0 is same partition)
 @param mconf   Mconf object
 @param mrl     The value to set
 @return 1 on success 0 on error
*/
int mconf_set_min_run_dist(mconf_t mconf, int mrl) {
	if(!mconf || mrl < -1)
		return 0;
	mconf->info->minRunningDist = mrl;
	mconf_sync(mconf);
	return 1;
}
int mconf_get_min_run_dist(mconf_t mconf, int *mrl)
{
	if(!mconf) return 0;
	*mrl = mconf->info->minRunningDist;
	return 1;
}

/** Setting the number of mosix processes running on the nodes.
    locals + guests is not enough since it does not count deputies

 @param mconf   Mconf object
 @param val     The value to set
 @return 1 on success 0 on error
*/
int mconf_set_mos_procs(mconf_t mconf, int val) {
	if(!mconf || val < 0)
		return 0;
	mconf->info->mosProcNum = val;
	mconf_sync(mconf);
	return 1;
}
int mconf_get_mos_procs(mconf_t mconf, int *val)
{
	if(!mconf) return 0;
	*val = mconf->info->mosProcNum;
	return 1;
}

/** Setting the map string in mconf. The map is saved in the second page,
    since the first page is used for the fixed configuration.

    If we already have a file mapped then we perform munmap of the
    area we perform mmap for a new size and write the file
 */

int mconf_set_map(mconf_t mconf, char *map, int size)
{
	mapper_t mapper;
	
	if(!mconf || !map)
		return 0;
	
	if(getuid() != 0)
		return 0;
	if(size > mconf->info->maxMapStrLen) {
		debug_lr(MCONF_DEBUG, "New map size is bigger then the space"
			 " Allocated on the mconf file\n");
		return 0;
	}
	// Trying to build a mapper object from the string 
	mapper = mapper_init_from_mem(map, size, -1);
	if(!mapper) {
		debug_lr(MCONF_DEBUG, "Error creating mapper object from string\n");
		return 0;
	}
	mapper_done(mapper);
	
	// Unmapping the current map string if it is mapped
	if(mconf->mapStr)
		munmap(mconf->mapStr, mconf->mapStrLen);
	
	// Mapping the buffer for the new map
	mconf->mapStr = mmap(0, size +1,
			     mconf->mmapProt, MAP_SHARED,
			     mconf->fd, getpagesize());
	if(mconf->mapStr == MAP_FAILED) {
		debug_lr(MCONF_DEBUG, "mapping of string size failed\n");
		return 0;
	}
	
	memcpy(mconf->mapStr, map, size);
	mconf->mapStr[size] = '\0';
	mconf->info->mapStrLen = size +1;
	mconf->info->mapVersion++;
	mconf->mapStrLen = size +1;
	msync(mconf->mapStr, size+1, MS_SYNC | MS_INVALIDATE);
	mconf_sync(mconf);
	return 1;
}

/** Return the size of the map string
 * @param mconf      The mosix configuration object
 * @return  The size of the map or 0 if there is no map or not mconf
 */
int mconf_get_map_size(mconf_t mconf)
{
	if(can_not_use_mconf(mconf))
		return 0;
	return mconf->info->mapStrLen;
}

/** We get the map string from mconf. If the size of the loaded
    map is different from the size of the map in the fixed size
    area we reload the map memory segment

*/
int mconf_get_map(mconf_t mconf, char *map, int size) {
	int curr_size;
	if(!mconf || !map)
		return 0;

	// No map installed yey
	if(!mconf->info->mapStrLen)
		return 0;
	// First we check if we need to reload the map
	curr_size = mconf->info->mapStrLen;
	if(curr_size != mconf->mapStrLen) {
		if(mconf->mapStr)
			munmap(mconf->mapStr, mconf->mapStrLen);
		// Mapping the buffer for the new map
		mconf->mapStr = mmap(0, curr_size,
				     mconf->mmapProt, MAP_SHARED,
				     mconf->fd, getpagesize());
		if(mconf->mapStr == MAP_FAILED) {
			debug_lr(MCONF_DEBUG, "mapping of string size failed\n");
			return 0;
		}
	}
	// Copying the map to the buffer
	if(size < curr_size)
		curr_size = size;
	memcpy(map, mconf->mapStr, curr_size);
	return 1;
}

/** Return an updated mapper object. While the map does not change the
    same mapper object will be returend. If map changes then a new one
    will be created and returned from now on.

 @param  mconf   The mconf obejct
 @param  mype    The current node pe. If 0 then pe is resolved using my local ip
                 address. If > 0 then used as my pe
 @return A valid mapper object or null on error.
*/

mapper_t mconf_get_mapper(mconf_t mconf) {
	int size;
	char *buff;
	static pe_t mype;
	pe_t my_curr_pe;

	if(can_not_use_mconf(mconf))
		return NULL;
	
	if(!msx_readval(MOSPE_FILENAME, &my_curr_pe)) {
		debug_lr(MCONF_DEBUG, "Error getting my pe\n");
		return NULL;
	}
	if(!my_curr_pe)
		return NULL;
	
	// The common case where the mapper version is the same as the map
	if(mconf->mapper &&
	   mconf->mapperMapVersion == mconf->info->mapVersion &&
	   my_curr_pe == mype)
		return mconf->mapper;
	// We update the mapper

	debug_lg(MCONF_DEBUG, "Getting mapper\n");
	mype = my_curr_pe;
	size = mconf->info->mapStrLen;
	if(size == 0)
		return NULL;
	if(!(buff = malloc(size)))
		return NULL;
	if(!mconf_get_map(mconf, buff, size)) {
		free(buff);
		return NULL;
	}
	mconf->mapper = mapper_init_from_mem(buff, strlen(buff), mype);
	free(buff);
	if(!mconf->mapper) 
		return NULL;

	mconf->mapperMapVersion = mconf->info->mapVersion;
	return mconf->mapper;
}


/** Checking if an mconf file is ok. We have several posibilities
    o File does not exist.
    o Failure in stat (rare)
    o File magic is bad (todo)
    o File is OK

    @param   fname    Name of file to check
    @retrn The status of the check, one of the mconf_file_status_t values
*/
static mconf_file_status_t check_mconf_file(char *fname)
{
	struct stat s;
	int fd = -1;
	
	if(stat(fname, &s) == -1) {
		// File does not exist (this is ok)
		if(errno == ENOENT)
		{
			debug_lg(MCONF_DEBUG, "File does not exists\n");
			return MCONF_FILE_NOT_EXIST;
		}
		else {
			debug_lr(MCONF_DEBUG, "Error in stat\n");
			return MCONF_FILE_BAD;
		}
	}
	else {
		// Checking if the file is regular
		if(!S_ISREG(s.st_mode)) {
			debug_lr(MCONF_DEBUG, "File is not regular file\n");
			return MCONF_FILE_BAD;
		}
		if((fd = open(fname, O_RDWR)) == -1)
		{
			debug_lr(MCONF_DEBUG, "Error opening the existing file\n");
			return MCONF_FILE_BAD;
		}
	}

	close(fd);
	return MCONF_FILE_OK;
}

/** Creating a new mconf file. Only creating the file (+ opening)
    And setting the size of the file to the write size (0 filled)
    @param  fname   Name of file to create
    @return The file descriptor (-1 on error)
*/
static int create_new_mconf_file(char *fname)
{
	int fd;
	struct mconf_fixed_info fi;
	
	fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);
	if(fd == -1) {
		debug_lr(MCONF_DEBUG, "Error opening new file\n");
		//perror("");
		return fd;
	}

	bzero(&fi, MCONF_FIXED_SIZE);
	if(write(fd, &fi, MCONF_FIXED_SIZE) != MCONF_FIXED_SIZE)
	{
		debug_lr(MCONF_DEBUG, "Error: parital write\n");
		close(fd);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	return fd;
}

/** Transofrom the enum guest type to a human readable string
 */
/*static char *guest_type_str(guest_t gt)
{
	static char guest_str[50];
	switch(gt) {
	    case GRID_IS_GUEST:
		    sprintf(guest_str, "Grid is Guest");
		    break;
	    case CLUSTER_IS_GUEST:
		    sprintf(guest_str, "Cluster is Guest");
		    break;
	    default:
		    sprintf(guest_str, "Unknown");
	}
	return guest_str;
}
*/
/** Setting the current kernel configuration according to the map and the
    given parameters. This internal function is used to perform the blocking
    open, use/unuse operations
 @param   mconf           A valid mconf object
 @param   min_guest_dist  Mininal guest distance
 @param   max_guest_dist  Maximal guest distance
 @param   min_mig_dist    Minimal guest distance
 @return 1 on success 0 on error 
 */
static 
int mconf_set_kernel_map(mconf_t mconf, int min_guest_dist, int max_guest_dist,
			 int max_mig_dist)
{
	mapper_t  mapper;
	int       ranges_num;
	struct mosixnet *kernel_map;
	int       fd, error;
	pe_t      my_pe;
	
	if(!mconf)
		return 0;
		
	if((my_pe = msx_read(MOSPE_FILENAME)) == -1)
	{
		debug_lr(MCONF_DEBUG, "Error: Cant get my pe\n");
		return 0;
	}
	if(!(mapper = mapper_init_from_mem(mconf->mapStr, mconf->mapStrLen, my_pe)))
	{
		debug_lr(MCONF_DEBUG, "Error: cant init mapper from mconf\n");
		return 0;
	}
	ranges_num = mapper_get_ranges_num(mapper);
	if(!(kernel_map = malloc(sizeof(struct mosixnet) * ranges_num))) {
		debug_lr(MCONF_DEBUG,
			 "Error: no free memory for allocating map buffer\n");
		return 0;
	}

	ranges_num = mapper_get_kernel_map(mapper, kernel_map, ranges_num,
					   min_guest_dist, max_guest_dist,
					   max_mig_dist);
	fd = open(CONFIG_FILENAME, O_WRONLY);
	error = write(fd, (char *)kernel_map, ranges_num * sizeof(struct mosixnet));
	free(kernel_map);
	if(error < 0) {
		debug_lr(MCONF_DEBUG, "MOSIX refuse to accept the configuration\n");
		close(fd);
		return 0;
	}
	close(fd);
	return 1;
}
/** Setting the reserve level of the current node. This means that
    processes with level (distance) larger the the one set will be
    expelled from the node (if such processes present) and will not be allowd
    to enter. This routine is for the ownerd to perform the reserve algorithm

    If manual_mode is on then 0 is returned and nothing is done.
    If reserve level is:
        0      we perform pblock
	1      we perfrom gblock
	2      we set the max_guest_distance

 @param   mconf            an mconf object
 @param   reserve_level    The reserve level to perform
 @return 1 on success 0 on failure
 */
int mconf_set_reserve_dist(mconf_t mconf, int reserve_level)
{
	if(!mconf) return 0;
	if(reserve_level < 0 || reserve_level > MAX_KNOWN_DIST)
		return 0;
	if(mconf->info->manualPresedenceMode)
		return 0;

	if(reserve_level == 0)
		return mconf_pblock(mconf);

	//if(reserve_level == SAME_CLUSTER_MAX_DIST)
	//return mconf_gblock(mconf);

	// In all other cases we dont perform the gopen but an improved one
	if(!msx_write(MAXGUESTS_FILENAME, mconf->info->maxGuests))
		return 0;
	if(!mconf_set_kernel_map(mconf,  SAME_CLUSTER_MAX_DIST,
				 reserve_level,
				 mconf->info->maxMigDist))
		return 0;
	mconf->info->minGuestDist = SAME_CLUSTER_MAX_DIST;
	mconf->info->maxGuestDist = reserve_level;
	mconf_sync(mconf);
	return 1;
}

/** Getting the current reserve level of the node. This means from which level
    and below processes can enter the node. For example level of
    SAME_PARTITION_MAX_DIST means only partition processes can enter (and should
    be present on the node.

    We check first the max_guests value of the kernel and if it is 0 then it
    means we are in pblock or gblock so level of 0 or 2.
    if max_guests is not 0 then we use the max_guest_dist
@param mconf    Mconf object
@param rl       Reserve level value
@return 1 on success (and then the rl value is valid) or 0 on error
 */ 
int mconf_get_reserve_dist(mconf_t mconf, int *rl) {
	int curr_max_guests;
	
	if(!mconf) return 0;
	if(!msx_readval(MAXGUESTS_FILENAME, &curr_max_guests))
		return 0;
	
	// If pblock then rl = 0
	*rl = mconf_calc_reserve_dist(mconf->info->minGuestDist,
				      mconf->info->maxGuestDist,
				      curr_max_guests);
	return 1;
}

int mconf_calc_reserve_dist(int min_guest_dist, int max_guest_dist,
			    int maxguests)
{
	if(maxguests == 0) {
		if(min_guest_dist == SAME_CLUSTER_MAX_DIST)
			return SAME_PARTITION_MAX_DIST;
	}

	// else if(min_guest_dist == SAME_GRID_MAX_DIST)
	//	return  SAME_CLUSTER_MAX_DIST;
	//else return -1;
	//}	
	return max_guest_dist;
}

int mconf_pblock(mconf_t mconf)
{
	if(!mconf) return 0;
	if(!msx_write(MAXGUESTS_FILENAME, 0))
		return 0;
	if(!mconf_set_kernel_map(mconf, SAME_CLUSTER_MAX_DIST,
				 SAME_GRID_MAX_DIST,
				 mconf->info->maxMigDist))
		return 0;
	mconf->info->minGuestDist = SAME_CLUSTER_MAX_DIST;
	mconf->info->maxGuestDist = SAME_GRID_MAX_DIST;
	
	mconf_sync(mconf);
	return 1;
}

int mconf_gblock(mconf_t mconf)
{
	if(!mconf) return 0;
	if(!msx_write(MAXGUESTS_FILENAME, mconf->info->maxGuests ))
		return 0;
	if(!mconf_set_kernel_map(mconf, SAME_GRID_MAX_DIST,
				 SAME_GRID_MAX_DIST,
				 mconf->info->maxMigDist))
		return 0;
	mconf->info->minGuestDist = SAME_CLUSTER_MAX_DIST;
	mconf->info->maxGuestDist = SAME_CLUSTER_MAX_DIST;
	mconf_sync(mconf);
	return 1;
}
int mconf_gopen(mconf_t mconf)
{
	if(!mconf) return 0;
	if(!msx_write(MAXGUESTS_FILENAME, mconf->info->maxGuests))
		return 0;
	if(!mconf_set_kernel_map(mconf, SAME_CLUSTER_MAX_DIST,
				 SAME_CLUSTER_MAX_DIST,
				 mconf->info->maxMigDist))
		return 0;
	mconf->info->minGuestDist = SAME_CLUSTER_MAX_DIST;
	mconf->info->maxGuestDist = SAME_GRID_MAX_DIST;
	mconf_sync(mconf);
	return 1;
}

/** Using the grid, which means letting local processes go to other
    clusters/grids. This operation write 0 to the dontgo field of
    the grid nodes in the kernel map
 * @param   mconf    An mconf object
 * @return 1 if the operation was succuesful, 0 on failure
 */
int mconf_grid_use(mconf_t mconf)
{
	if(!mconf) return 0;
	if(!mconf_set_kernel_map(mconf, mconf->info->minGuestDist,
				 mconf->info->maxGuestDist,
				 SAME_GRID_MAX_DIST))
		return 0;
	mconf->info->maxMigDist = SAME_GRID_MAX_DIST;
	mconf_sync(mconf);
	return 1;
}

int mconf_grid_unuse(mconf_t mconf)
{
	if(!mconf) return 0;
	if(!mconf_set_kernel_map(mconf, mconf->info->minGuestDist,
				 mconf->info->maxGuestDist,
				 SAME_PARTITION_MAX_DIST))
		return 0;
	mconf->info->maxMigDist = SAME_PARTITION_MAX_DIST;
	mconf_sync(mconf);
	return 1;
}

int mconf_grid_connect(mconf_t mconf)
{
	if(!mconf) return 0;
	
	if(!mconf_gopen(mconf) || !mconf_grid_use(mconf))
		return 0;
	mconf->info->manualPresedenceMode = 0;
	return 1;
}	

int mconf_grid_dissconnect(mconf_t mconf) {
	if(!mconf) return 0;
	if(!mconf_gblock(mconf) || !mconf_grid_unuse(mconf))
		return 0;
	mconf->info->manualPresedenceMode = 1;
	return 1;
}


/** Getting the minimum distance a guest have
 * @param   mconf    An mconf object
 * @param  *dist     Pointer to the resulting distance       
 * @return 1 if the operation was succuesful, 0 on failure
 */
int mconf_get_min_guest_dist(mconf_t mconf, int *dist) {
	if(!mconf)
		return 0;
	*dist = mconf->info->minGuestDist;
	return 1;
}

/** Getting the max distance a guest have
 * @param   mconf    An mconf object
 * @param  *dist     Pointer to the resulting distance       
 * @return 1 if the operation was succuesful, 0 on failure
 */
int mconf_get_max_guest_dist(mconf_t mconf, int *dist) {
	if(!mconf)
		return 0;
	*dist = mconf->info->maxGuestDist;
	return 1;
}

/** Getting the maximum distance a local process can migrate to
 * @param   mconf    An mconf object
 * @param  *dist     Pointer to the resulting distance       
 * @return 1 if the operation was succuesful, 0 on failure
 */
int mconf_get_max_mig_dist(mconf_t mconf, int *dist) {
	if(!mconf)
		return 0;
	*dist = mconf->info->maxMigDist;
	return 1;
}


int mconf_set_ownerd_status(mconf_t mconf, int status)
{
	if(can_not_use_mconf(mconf))
		return 0;
	mconf->info->ownerdStatus = status;
	mconf_sync(mconf);
	return 1;
}
int mconf_get_ownerd_status(mconf_t mconf, int *status)
{
	if(can_not_use_mconf(mconf))
		return 0;
	*status = mconf->info->ownerdStatus;
	return 1;
}
