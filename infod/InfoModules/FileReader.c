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


#include "StringUtil.h"

#include <msx_error.h>
#include <msx_debug.h>
#include <ConfFileReader.h>
#include "infoitems.h"

#include "ProviderInfoModule.h"

/***************************************************
 * HELPERS
 ***************************************************/

// normalization factor
static float NORM_FACTOR;
//static int   TOPPROCS = 10;

// invalidate proc
void invalidateProc (gpointer key, gpointer val, gpointer user_date) {
    ((struct proc*)val)->_valid = 0;
}

// is this a valid process ?
gboolean isProcInvalid (gpointer key, gpointer val, gpointer user_data) {
    return ((struct proc*)val) -> _valid == 0;
}

// fill proc data in array
void fillProcData (gpointer key, gpointer val, gpointer user_data) {
    struct procsort ** proc = (struct procsort **)user_data;
    (*proc)->_pid = ((struct proc *)val)->_pid;
    (*proc)->_data = (struct proc *)val;
    (*proc)++;
}

// compare cpu usages for sorting
int pcpucomp(const void *proc1, const void *proc2) {
    return (int)((((struct procsort*)proc2)->_data->_pcpu -
                  ((struct procsort*)proc1)->_data->_pcpu) * 100);
}

// read a file into a buffer
static int file2str(const char *filename, char *ret) {
    int fd, num_read;

    // open file
    fd = open(filename, O_RDONLY, 0);
    if(fd == -1)
        return -1;

    // read file
    num_read = read(fd, ret, 1023);

    // close file
    close(fd);

    // did we have a read error?
    if(num_read <= 0)
        return -1;

    // null terminate
    ret[num_read] = '\0';

    // return text
    return num_read;
}

inline char *skipFields(char *buff, int count) {
  for(int i=0 ; i < count ; i++) {
    while ( *(buff) != ' ' && *(buff) != '\n' && *(buff) != '\t')
      ++buff;
    ++buff;
  }
  return buff;
}

// read infomation for the given pid from /proc/pid/stat
int readProcInfo(struct proc *procInfo, char * pid) {
  //    struct stat sb;
  char statfile[80], statusfile[80], memfile[80], *buff, *tmp;
  int i;
  
  // assemble filename
  sprintf(statfile, "/proc/%s/stat", pid);
  sprintf(statusfile, "/proc/%s/status", pid);
  sprintf(memfile, "/proc/%s/statm", pid);
  
  // read the file
  tmp = buff = (char *)malloc(1024);
  
  // first read the tgid
  if (file2str(statusfile, buff) < 0) {
    free (tmp);
    procInfo->_pcpu = -1;
    return -1;
  }
  
  // skip fields
  buff = skipFields(buff, 1);
  sscanf(buff, "%s", procInfo->_name);
  buff= skipFields(buff, 5);
  sscanf(buff, "%hu", &procInfo->_tgid);
  
  if (procInfo->_tgid != procInfo->_pid) {
    procInfo->_pcpu = -1;
    free (tmp);
    return -1;
  }
  
  // skip fields
  buff = skipFields(buff, 8);
  
  // now read uid
  sscanf(buff, "%hu", &procInfo->_uid);
  
  buff = tmp;    
  
  // now read stat
  if (file2str(statfile, buff) < 0) {
    procInfo->_pcpu = -1;
    return -1;
  }
  
  // mark that this pid is OK!
  procInfo->_pcpu = 0;
  
  // skip fields
  for (i=0; i<13; ++i)
    while ( *(buff++) != ' ');
  
  procInfo->_last_utime = procInfo->_utime;
  procInfo->_last_stime = procInfo->_stime;
  
  // read times
  sscanf(buff, "%lu %lu", &procInfo->_utime, &procInfo->_stime);
  
  
  //---- Read memory
  if (file2str(memfile, buff) < 0) {
    procInfo->_pcpu = -1;
    return -1;
  }
  int tmpInt;
  sscanf(buff, "%d %d", &tmpInt, &procInfo->_mem);
  float memFloat = procInfo->_mem;
  memFloat *= getpagesize();
  memFloat /= (1024*1024);
  procInfo->_mem  = (int) memFloat;

 
  // free buffer
  free (tmp);
  
  return 0;
}

// calc %cpu for a given process
void getpcpu (struct proc *procInfo) {
    float passed;

    if (procInfo->_pcpu < 0)
        return;
    
    // how much time had actually passed?
    passed = ((procInfo->_tv.tv_sec * 1000000 + procInfo->_tv.tv_usec) -
              (procInfo->_oldtv.tv_sec * 1000000 + procInfo->_oldtv.tv_usec));
    

    // pcpu = [total process time for interval] * 100 / [interval]
    procInfo->_pcpu =
        (float)((procInfo->_stime + procInfo->_utime) -
                (procInfo->_last_stime + procInfo->_last_utime)) /
        passed * NORM_FACTOR;

    // cap at 100%
    if (procInfo->_pcpu > 100)
        procInfo->_pcpu = 100;
}

int parseArgStr(topusage_module_t *tu, char *str) {
  
	return 1;
}

static char  *validVarNames[] =
{
	"topusage.minpcpu",
	"topusage.maxproc",
	NULL
};



int parseConfigFile(topusage_module_t *tu, char *configFile)
{
     conf_file_t cf;
    
     if(!(cf = cf_new(validVarNames))) {
	  fprintf(stderr, "Error generating conf file object\n");
	  return 0;
     }
     debug_lb(INFOD_DBG, "Parsing configuration file [%s]\n", configFile);
     if(access(configFile, R_OK) != 0) {
	  debug_lr(INFOD_DEBUG, "Error accessing configuration files [%s]\n",
		   configFile);
	  return 0;
     }
     
     if(!cf_parseFile(cf, configFile)) {
	  debug_lr(INFOD_DEBUG, "Error parsing configuration file\n");
	  return 0;
     }

     int        intVal;
     if(cf_getIntVar(cf, "topusage.minpcpu", &intVal)) {
	  tu->_minPCPU = intVal;
	  debug_lr(INFOD_DEBUG, "Setting topusage.minpcpu to %d\n", intVal);
     }

     if(cf_getIntVar(cf, "topusage.maxproc", &intVal)) {
	  tu->_maxProc2Show = intVal;
	  debug_lr(INFOD_DEBUG, "Setting topusage.maxproc to %d\n", intVal);
     }

     cf_free(cf);
	
     return 1;
}


/*****************************************
 * API
 *****************************************/
int topusage_pim_init(void **module_data, void *module_init_data) {

    int res;
    
    // init normalization factor
    NORM_FACTOR = 1000000.0 / (float)sysconf(_SC_CLK_TCK) * 100.0;

    // init module data
    *module_data = (struct inf_module_data *)malloc(sizeof(struct inf_module_data));
    if (!*module_data)
        return 0;

    pim_init_data_t *pim_init = (pim_init_data_t *) module_init_data;
    debug_lb(INFOD_DBG, "Config file: [%s]\n", pim_init->config_file);
    topusage_module_t *tu = (topusage_module_t *)*module_data;

    // init process hash table
    tu->_procsHash = 
	    g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
    
    tu->_pagesize = getpagesize();
    tu->_minPCPU = TOPUSAGE_MIN_PCPU;
    tu->_maxProc2Show = TOPUSAGE_MAX_PROCS;

    parseConfigFile(tu, pim_init->config_file);
    //debug_lg(INFOD_DBG, "Pagesize: %d\n", getpagesize());
    // read the proc table a first time
    res = topusage_pim_update(*module_data);
 
    return 1;
}

int topusage_pim_free(void **module_data) {

    // free memory
    g_hash_table_remove_all(((struct inf_module_data*)*module_data)->_procsHash);
    g_hash_table_destroy(((struct inf_module_data*)*module_data)->_procsHash);
    free (*module_data);

    return 1;
}

// update
int topusage_pim_update(void *module_data) {

    struct timezone tz;
    struct inf_module_data *proctab = (struct inf_module_data *)module_data;
    char newproc = 0;

    // mark hash table invalid
    g_hash_table_foreach (proctab->_procsHash, invalidateProc, NULL);

    // go over /proc
    struct dirent *ent;
    DIR *proc = opendir("/proc");
    if (!proc)
        return 0;

    // go over proc
    for (;;) {

        // find the next process
        ent = readdir(proc);
        if(!ent || !ent->d_name) {
            break;
        }        

        // we found a pid entry
        if((*ent->d_name > '0') && (*ent->d_name <= '9') ) {

            newproc = 0;
            char *pidName = strdup(ent->d_name);

            // get a hash entry for this process
            struct proc *curproc = g_hash_table_lookup(proctab->_procsHash,
                                                       pidName);

            // create a new entry and insert it, if needed
            if (!curproc) {
                newproc = 1;
                curproc = (struct proc*)malloc(sizeof(struct proc));
                memset(curproc, 0, sizeof(curproc));
                g_hash_table_insert(proctab->_procsHash,
                                    pidName, curproc);
                curproc->_pid = strtoul(ent->d_name, NULL, 10);
            }
            
            // mark process as alive
            curproc->_valid = 1;

            // save old time
            curproc->_oldtv = curproc->_tv;

            // get current proc stats
            readProcInfo(curproc, pidName);

            // get current time
            gettimeofday(&curproc->_tv, &tz);

            // calc %cpu
            getpcpu(curproc);

            // free the pidName char if we don't reallt need it
            if (newproc == 0) {
                free (pidName);
            }
        }
    }

    closedir(proc);

    // finally, remove dead procs
    g_hash_table_foreach_remove(proctab->_procsHash, isProcInvalid, NULL);

    return 1;
}

/* get info in form:
   <top-usage>
   <proc pid="11111">
       <pcpu>90.00</pcpu>
       <uid>1299</uid>
       <name>testload</name>
       <mem>2300</mem>
   </proc>

   </top-usage>
*/
int topusage_pim_get(void *module_data, void *data, int *size) {

    struct inf_module_data *proctab = (struct inf_module_data*)module_data;
    int procnum = g_hash_table_size(proctab->_procsHash);
    struct procsort procs[procnum];
    struct procsort * procs_tmp = procs; 
    //char **tmp = data;
    char *buff = (char *)data;
    int count;

    
    // let's see if we have a big enough buffer
    if (*size < 348)
        return 0;

    // copy needed data into hash
    g_hash_table_foreach(proctab->_procsHash,
                         fillProcData,
                         &procs_tmp);

    // sort the table
    qsort(procs, procnum, sizeof(struct procsort), pcpucomp);

    // clear string
    memset(buff,0,348);
    
    // now let's create a list
    buff += sprintf(buff,"<%s>\n", ITEM_TOP_USAGE_NAME);
    for (count = 0; count< proctab->_maxProc2Show; ++count) {
      if(procs[count]._data->_pcpu < proctab->_minPCPU) continue;

      buff += sprintf(buff, 
                      "<proc pid=\"%d\">"
                      "  <pcpu>%.2f</pcpu>"
                      "  <uid>%d</uid>"
                      "  <name>%s</name>"
                      "  <mem>%d</mem>"
                      "</proc>\n",
                      procs[count]._pid,
                      procs[count]._data->_pcpu,
                      procs[count]._data->_uid,
                      procs[count]._data->_name,
                      procs[count]._data->_mem);
      
    }
    buff += sprintf(buff, "</%s>", ITEM_TOP_USAGE_NAME);
    *size = strlen((char *)data) + 1;
    debug_lb(INFOD_DBG, "Size: %d\n[[%s]]", *size, (char*)data);
    return 1;
}

int topusage_pim_desc(void *module_data, char *buff) {
  sprintf(buff, "\t<vlen name=\"%s\" type=\"string\" unit=\"xml\" />\n",
          ITEM_TOP_USAGE_NAME);

  return 1;
}


/********
 * MAIN *
 ********/
/* int main (int argc, char** args) { */

/*     void *data; */
/*     char *buff; */
/*     int i; */
    
/*     // init     */
/*     topusage_pim_init(&data, NULL); */

/*     // sleep */
/*     sleep(2); */

/*     // update */
/*     topusage_pim_update(data); */

/*     // sort */
/*     topusage_pim_get(data, &buff, &i); */

/*     printf ("%s %d",buff,getpid()); */

/*     // free */
/*     topusage_pim_free(&data); */

/*     free(buff); */
    
/*     return 0; */
/*}*/

#ifdef __TOP_USAGE_TEST__
int main() {
    void * data;
    char * buff = (char *)malloc(8192);
    int i = 1000;
    int count = 0;

    msx_add_debug(INFOD_DBG);
    // init
    topusage_pim_init(&data, NULL);

   // sleep
   sleep(1);

   while(count++<1000) {
     // update
     topusage_pim_update(data);

     // sort
     i = 8192;
     topusage_pim_get(data, buff, &i);
     //system("clear");
     printf(buff);
     printf("\n");
     sleep(1);
   }

   // free
   topusage_pim_free(&data);

   free(buff);

   return 0;
}       

#endif
