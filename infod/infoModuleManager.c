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

/******************************************************************************
 * File: ProviderInfoModule.c Implementing an infrastructure for general info
 *       modules
 *
 * An info module is a collection of function + data which are responsible for
 * collecting information. The ProviderInfoModule is responsible for
 * registering the various modules and activting the periodicaly to collect
 * information 
 *
 *****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <dlfcn.h>


// Project includes (libutil)
#include <info.h>
#include <msx_error.h>
#include <msx_debug.h>
#include <msx_proc.h>
#include <ConfFileReader.h>
#include <parse_helper.h>
#include <pluginUtil.h>
#include <ModuleLogger.h>

// Infod includes
#include <infod.h>
#include <provider.h>
#include <infoModuleManager.h>

static inline
unsigned long milli_time_diff(struct timeval *start, struct timeval *end)
{
    unsigned long time_diff;

    time_diff = 1000 * (end->tv_sec - start->tv_sec);
    if (time_diff == 0)
        time_diff = end->tv_usec / 1000 - start->tv_usec / 1000;
    else
        time_diff += end->tv_usec / 1000 + (1000 - start->tv_usec / 1000);
    return time_diff;
}

// Setting the initial argument (data) of the module

int pim_setInitData(pim_entry_t *pimArr, char *pimName, void *data)
{
    pim_entry_t *ptr = pimArr;
    int found = 0;

    if (!pimArr || !pimName)
        return 0;

    while (ptr->name != NULL) {
        if (strcmp(ptr->name, pimName) == 0) {
            ptr->init_data = data;
            found = 1;
            break;
        }
        ptr++;
    }
    return found;
}

int init_pim_entry(pim_t pim, pim_entry_t *ent, pim_entry_t *src)
{
    if (!ent || !src) return 0;

    bzero(ent, sizeof (pim_entry_t));

    ent->name = strdup(src->name);
    ent->init_func = src->init_func;
    ent->free_func = src->free_func;
    ent->update_func = src->update_func;
    ent->get_func = src->get_func;
    ent->desc_func = src->desc_func;
    ent->period = src->period;
    ent->debug = src->debug;
    ent->init_data = src->init_data;


    debug_lg(INFOD_DBG, "Initializing pim: %s [period: %d]\n", ent->name, ent->period);
    // Setting the configuration file. If the specific pim had its own we use it
    // else we use the general config file, else NULL
    if (src->config_file) {
        ent->config_file = src->config_file;
    } else if (pim->configFile) {
        ent->config_file = pim->configFile;
    } else {
        ent->config_file = NULL;
    }

    void *data;
    int res;

    pim_init_data_t pim_init_data;
    pim_init_data.init_data = ent->init_data;
    pim_init_data.config_file = ent->config_file;

    // Calling the module init func
    res = (*ent->init_func)(&data, &pim_init_data);
    if (!res) {
        if (ent->name)
            free(ent->name);
        return 0;
    }
    ent->private_data = data;
    ent->init_data = src->init_data;
    return 1;
}

static char *validVarNames[] = {
    "plugins.names",
    "plugins.dir",
    "plugins.enabled-dir",
    NULL
};

int get_external_modules(char *configFile, char ***externalNames, int *externalNum, char *externalModulePath)
{

    conf_file_t cf;

    if (!(cf = cf_load(configFile, validVarNames))) {
        mlog_bn_error("pim", "Error generating conf file object\n");
        return 0;
    }

    conf_var_t varEnt;

    strncpy(externalModulePath, GOSSIMON_PLUGINS_DIR, PIM_MAX_PATH_LEN);
    if (cf_getVar(cf, "plugins.dir", &varEnt)) {
        mlog_bn_dr("pim", "Got external modules path: %s\n", varEnt.varValue);
        strncpy(externalModulePath, varEnt.varValue, PIM_MAX_PATH_LEN);
    }
    mlog_bn_info("pim", "External module path: %s\n", externalModulePath);

    GPtrArray *arr = g_ptr_array_new();

    // Getting the modules from the plugins.names variable
    if (cf_getListVar(cf, "plugins.names", ", ", arr) && arr->len > 0) {
        mlog_bn_info("pim", "Found plugins.names variable\n");
    }
    // Scanning the plugins-enabled dir for *.conf files to get modules to load
    else{
        if(!getFilesInDir(GOSSIMON_ENABLED_PLUGINS_DIR, &arr, ".conf")) {
            mlog_bn_error("pim", "Error getting files in directory %s\n", GOSSIMON_ENABLED_PLUGINS_DIR);
            return 0;
        }
        mlog_bn_info("pim", "Found files in %d plugins-enabled dir\n", arr->len);
    }
    if(arr->len > 0) {
        *externalNum = arr->len;
        *externalNames = malloc(sizeof (char *) * arr->len + 1);
        for (int i = 0, j = 0; i < arr->len; i++) {
            mlog_bn_dg("pim", "-----> Module file: [%s]\n", g_ptr_array_index(arr, i));
            if (strchr(g_ptr_array_index(arr, i), '/')) {
                mlog_bn_warn("pim", "External module name [%s] containes / skeeping\n");
                continue;
            }
            (*externalNames)[j] = g_ptr_array_index(arr, i);
            // Taking the name without the .conf
            char *t = strrchr(((*externalNames)[j]), '.');
            mlog_bn_dg("pim", "Found . at [%s]\n", t);
            *t = '\0';
            j++;
        }
    }
    g_ptr_array_free(arr, FALSE);

    cf_free(cf);
    return 1;
}

int load_symbol(void *hndl, char *name, void **address)
{
    char *error;

    dlerror(); // To clear previous errors if any ..

    *address = dlsym(hndl, name);
    error = dlerror();
    if (error != NULL) {
        mlog_bn_error("pim", "Error: loading %s\nError:%s\n", name, error);
        return 0;
    }
    return 1;
}

int load_external_module(pim_entry_t *ent, char *path, char *name)
{

    void *hndl;

    char *buff = malloc(PIM_MAX_PATH_LEN);
    if (!buff) return 0;

    sprintf(buff, "%s/%s.so", path, name);

    mlog_bn_dg("pim", "Trying to load module: [%s]\n", buff);

    /* open the file holding the funcs */
    if (!(hndl = dlopen(buff, RTLD_LAZY))) {
        mlog_bn_error("pim", "Error dlopen:: %s\n", dlerror());
        return 0;
    }
    // Clearing the entry before starting
    memset(ent, 0, sizeof (pim_entry_t));

    // Note that  the correct way to check error on dlsym is to
    // check the return value of dlerror(). See man dlsym.
    if (!load_symbol(hndl, IM_INIT_FUNC_NAME, (void **) &(ent->init_func)) |
            !load_symbol(hndl, IM_FREE_FUNC_NAME, (void **) &(ent->free_func)) |
            !load_symbol(hndl, IM_UPDATE_FUNC_NAME, (void **) &(ent->update_func)) |
            !load_symbol(hndl, IM_GET_FUNC_NAME, (void **) &(ent->get_func)) |
            !load_symbol(hndl, IM_DESCRIPTION_FUNC_NAME, (void **) &(ent->desc_func)) |
            !load_symbol(hndl, IM_NAME_SYMBOL_NAME, (void **) &(ent->name)) |
            !load_symbol(hndl, IM_DEBUG_SYMBOL_NAME, (void **) &(ent->debug))) {

        mlog_bn_error("pim", "Failed loading a required function/symbol\n");
        return 0;
    }
    int *periodAddr;
    if (load_symbol(hndl, IM_PERIOD_SYMBOL_NAME, (void**) &(periodAddr))) {
        ent->period = *periodAddr;
    } else {
        ent->period = PIM_DEF_PERIOD;
    }
    mlog_bn_db("pim", "External module %s period %d\n", ent->name, ent->period);

    // Setting the config file for the module
    sprintf(buff, "%s/%s.conf", GOSSIMON_ENABLED_PLUGINS_DIR, name);
    if(access(buff, R_OK) == 0) {
        ent->config_file = strdup(buff);
    }
    return 1;
}

int load_external_modules(pim_entry_t *arr, char *externalPath, char **externalNames, int externalNum)
{
    int goodModules = 0;

    for (int i = 0; i < externalNum; i++) {
        int res = load_external_module(arr, externalPath, externalNames[i]);
        if (!res) {
            mlog_bn_warn("pim", "Loading of module %s faild\n", externalNames[i]);
        } else {
            mlog_bn_info("pim", "Loaded module %s\n", externalNames[i]);
            arr++;
            goodModules++;
        }
    }

    return goodModules;
}

pim_t pim_init(pim_entry_t *internalArr, char *configFile)
{
    mlog_registerModule("pim", "Povider Info Module", "pim");
            
    pim_t pim;
    pim = malloc(sizeof (struct provider_info_modules));
    if (!pim)
        return NULL;
    memset(pim, 0, sizeof(struct provider_info_modules) );

    // Counting the modules in the array. The last module should be with name = NULL
    int internalNum = 0;
    pim_entry_t *ptr = internalArr;
    while (ptr->name) {
        internalNum++;
        ptr++;
    }


    // Handling external .so modules
    int externalNum = 0;
    char **externalNames;
    char *externalPath = malloc(PIM_MAX_PATH_LEN);
    if (!get_external_modules(configFile, &externalNames, &externalNum, externalPath)) {
        mlog_bn_error("pim", "Error getting external modules\n");
        goto failed;
    }
    mlog_bn_dg("pim", "Got %d external modules\n", externalNum);

    // Allocating the allModulesArr which will be later copied into pim
    int allModulesNum = internalNum;
    pim_entry_t *allModulesArr = malloc(sizeof (pim_entry_t) * (internalNum + externalNum + 2));
    for (int i = 0; i < internalNum; i++) allModulesArr[i] = internalArr[i];


    // Loading the external modules
    pim_entry_t *tmp = &allModulesArr[internalNum];
    int externalGood = 0;
    if (externalNum > 0) {
        externalGood = load_external_modules(tmp, externalPath, externalNames, externalNum);
        // Only successfuly loaded modules are added
        allModulesNum += externalGood;
        mlog_bn_dy("pim", "Added %d external modules to list of info modules\n", externalGood);
    }


    // Setting up the pim structure
    pim->configFile = configFile;
    pim->pimArr = malloc(sizeof (pim_entry_t) * allModulesNum + 50);
    pim->validPim = malloc(allModulesNum + 100);
    pim->lastUpdateTime = malloc(sizeof (struct timeval) * allModulesNum + 50);
    if (!pim->pimArr || !pim->validPim || !pim->lastUpdateTime)
        return NULL;
    pim->pimArrMaxSize = allModulesNum + 50;
    pim->pimArrSize = 0;

    bzero(pim->validPim, allModulesNum + 50);
    for (int i = 0; i < allModulesNum; i++) {
        int res = init_pim_entry(pim, &pim->pimArr[i], &allModulesArr[i]);
        if (res) {
            pim->validPim[i] = 1;

        } else {
            pim->validPim[i] = 0;
        }
        mlog_bn_dy("pim", "Info module   [%s] init status %d\n", pim->pimArr[i].name, res);
        // Setting the last update time to 0;
        timerclear(&(pim->lastUpdateTime[i]));

        pim->pimArrSize++;
    }

    pim->tmpBuff = malloc(4096);
    if (!pim->tmpBuff)
        goto failed;
    return pim;
failed:
    pim_free(pim);
    return NULL;
}

static void free_all_modules(pim_t pim)
{
    if (!pim) return;
    if (!pim->validPim) return;
    
    for (int i = 0; i < pim->pimArrSize; i++) {
        // Non valid modules are not freed
        if (!pim->validPim[i])
            continue;

        pim_entry_t *ent = &pim->pimArr[i];
        free(ent->name);
        (*ent->free_func)(ent->private_data);
        ent->private_data = NULL;
    }
}

void pim_free(pim_t pim)
{

    if (!pim) return;
    if (pim->pimArr) {
        free_all_modules(pim);
        free(pim->pimArr);
    }
    if (pim->validPim)
        free(pim->validPim);

    if (pim->tmpBuff)
        free(pim->tmpBuff);
    free(pim);
}

// Simply calling all the modules update func

int pim_update(pim_t pim, struct timeval *currTime)
{
    for (int i = 0; i < pim->pimArrSize; i++) {
        if (!pim->validPim[i])
            continue;
        pim_entry_t *ent = &pim->pimArr[i];

        unsigned long timeDiff;
        timeDiff = milli_time_diff(&(pim->lastUpdateTime[i]), currTime);
        if (timeDiff >= ent->period * 1000) {
            pim->lastUpdateTime[i] = *currTime;
            (*ent->update_func)(ent->private_data);
        }
    }
    return 1;
}

// Calling each module get_func and packing the information to the ninfo node

int pim_packInfo(pim_t pim, node_info_t *ninfo)
{
    for (int i = 0; i < pim->pimArrSize; i++) {
        int size;
        pim_entry_t *ent = &pim->pimArr[i];
        int res;

        if (!pim->validPim[i])
            continue;

        //debug_ly(INFOD_DEBUG, "Adding name %s\n", ent->name);
        size = 4096;
        res = (*ent->get_func)(ent->private_data, (void *) pim->tmpBuff, &size);

        // Adding the info only if the get_func return good status and
        // size > 0 (so there is data to pack
        if (res && size > 0)
            add_vlen_info(ninfo, ent->name, pim->tmpBuff, size);
    }
    return 1;
}


#include <UsedByPIM.h>
#include <ProcessWatchPIM.h>
#include <FreezeInfoPIM.h>
#include <ClusterID_CRC_PIM.h>
#include <InfodDebugPim.h>
#include <EconomyPIM.h>
#include <JMigratePIM.h>


//------ Provider default modules ----
pim_entry_t pim_arr[] = {
    { name : ITEM_FREEZE_INFO_NAME,
        init_func : frzinfo_pim_init,
        free_func : frzinfo_pim_free,
        update_func : frzinfo_pim_update,
        get_func : frzinfo_pim_get,
        desc_func : frzinfo_pim_desc,
        period : 5,
        init_data : FREEZE_CONF_FILENAME ":30",},

    { name : ITEM_USEDBY_NAME,
        init_func : usedby_pim_init,
        free_func : usedby_pim_free,
        update_func : usedby_pim_update,
        get_func : usedby_pim_get,
        desc_func : usedby_pim_desc,
        period : 5,
        init_data : NULL,},
    { name : ITEM_PROC_WATCH_NAME,
        init_func : procwatch_pim_init,
        free_func : procwatch_pim_free,
        update_func : procwatch_pim_update,
        get_func : procwatch_pim_get,
        desc_func : procwatch_pim_desc,
        period : 5,
        init_data : NULL,},
    { name : ITEM_CID_CRC_NAME,
        init_func : cidcrc_pim_init,
        free_func : cidcrc_pim_free,
        update_func : cidcrc_pim_update,
        get_func : cidcrc_pim_get,
        desc_func : cidcrc_pim_desc,
        period : 5,
        init_data : MOSIX_MAP_FILENAME,},
    { name : ITEM_INFOD_DEBUG_NAME,
        init_func : idbg_pim_init,
        free_func : idbg_pim_free,
        update_func : idbg_pim_update,
        get_func : idbg_pim_get,
        desc_func : idbg_pim_desc,
        period : 5,
        init_data : NULL,},
    // Last and empty module
    { name : NULL,
        init_func : NULL,
        free_func : NULL,
        update_func : NULL,
        get_func : NULL,
        period : 0,
        init_data : NULL,},
};

pim_entry_t *pim_getDefaultPIMs()
{
    return pim_arr;
}

void pim_appendDescription(pim_t pim, char *desc)
{
    char pimDesc[512];

    for (int i = 0; i < pim->pimArrSize; i++) {

        if (!pim->validPim[i]) continue;

        pim_entry_t *ent = &pim->pimArr[i];
        mlog_bn_dg("pim", "Getting description from module: [%s]\n", ent->name);
        (*ent->desc_func)(ent->private_data, pimDesc);
        strcat(desc, pimDesc);
    }

}
