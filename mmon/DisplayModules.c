/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <glib/garray.h>


#include "mmon.h"
#include "DisplayModules.h"
#include "GeneralMD.h"


int infoDisplayModuleNum = 0;
mon_display_module_t mon_displays[MAX_INFO_DISPLAY_MODULES];

int displayModule_init()
{
    memset(mon_displays, 0, sizeof (mon_display_module_t) * MAX_INFO_DISPLAY_MODULES);
    return 1;
}

int displayModule_registerModule(mon_display_module_t *moduleInfo)
{
    mon_display_module_t *md;
    mon_displays[infoDisplayModuleNum] = *moduleInfo;
    md = &mon_displays[infoDisplayModuleNum];
    //printf("Registering: %s %d  pos %d\n", moduleInfo->md_title, moduleInfo->md_item,
    //infoDisplayModuleNum);
    md->md_item = infoDisplayModuleNum;
    infoDisplayModuleNum++;

    md->md_titlength = strlen(md->md_title);
    return 1;
}

mon_display_module_t *getInfoDisplayByName(char *name)
{

    if (!name)
        return NULL;

    for (int i = 0; i < infoDisplayModuleNum; i++) {
        if (mon_displays[i].md_name &&
                strcmp(mon_displays[i].md_name, name) == 0)
            return &mon_displays[i];
    }
    return NULL;
}

int dm_getIdByName(char *name)
{
    if (!name)
        return 0;

    for (int i = 0; i < infoDisplayModuleNum; i++) {
        if (mon_displays[i].md_name &&
                strcmp(mon_displays[i].md_name, name) == 0)
            return mon_displays[i].md_item;
    }
    return -1;
}

mon_display_module_t *getInfoDisplayById(int id)
{
    if (id < 0 || id >= infoDisplayModuleNum)
        return NULL;

    for (int i = 0; i < infoDisplayModuleNum; i++) {
        if (mon_displays[i].md_item == id)
            return &mon_displays[i];
    }
    return NULL;

}

// Getting all display modules with side window enabled
int getMDWithSideWindow(mdPtr *modulePtrArr) {
    int index = 0;
    for (int i = 0; i < infoDisplayModuleNum; i++) {
        if(mon_displays[i].md_side_window_display)
            modulePtrArr[index++] = &mon_displays[i];
    }
    return index;
}


void listInfoDisplays(FILE *fp)
{

    fprintf(fp, "Information Display Modules:\n");
    fprintf(fp, "============================\n");
    for (int i = 0; i < infoDisplayModuleNum; i++) {
        fprintf(fp, "%-15s:", mon_displays[i].md_name);
        char **arr = mon_displays[i].md_help_func();
        // Printing only the first line (the short description)
        if (arr[0]) fprintf(fp, "%s\n", arr[0]);
        else fprintf(fp, "\n");
    }
}

int displayModule_updateInfoDescription(variable_map_t *info_mapping)
{
    for (int i = 0; i < infoDisplayModuleNum; i++) {
        if (mon_displays[i].md_update_info_desc_func) {
            (mon_displays[i].md_update_info_desc_func)(&mon_displays[i], info_mapping);
        }
    }
}

int displayModule_initModules()
{
    for (int i = 0; i < infoDisplayModuleNum; i++) {
        if (mon_displays[i].md_init_func) {
            (mon_displays[i].md_init_func)();
        }
    }
}

int displayModule_registerStandardModules()
{

    displayModule_registerModule(&name_mod_info);
    displayModule_registerModule(&num_mod_info);
    displayModule_registerModule(&status_mod_info);
    displayModule_registerModule(&infod_status_mod_info);
    displayModule_registerModule(&kernel_version_mod_info);
    displayModule_registerModule(&version_mod_info);
    displayModule_registerModule(&uptime_mod_info);
    displayModule_registerModule(&load_mod_info);
    displayModule_registerModule(&load_val_mod_info);
    displayModule_registerModule(&frozen_mod_info);
    displayModule_registerModule(&load2_mod_info);
    displayModule_registerModule(&mem_mod_info);
    displayModule_registerModule(&swap_mod_info);
    displayModule_registerModule(&disk_mod_info);
    displayModule_registerModule(&freeze_space_mod_info);
    displayModule_registerModule(&freeze_space_val_mod_info);
    displayModule_registerModule(&grid_mod_info);
    displayModule_registerModule(&token_mod_info);
    displayModule_registerModule(&speed_mod_info);
    displayModule_registerModule(&util_mod_info);
    displayModule_registerModule(&iowait_mod_info);
   
    displayModule_registerModule(&rio_mod_info);
    displayModule_registerModule(&wio_mod_info);
    displayModule_registerModule(&tio_mod_info);
    displayModule_registerModule(&net_in_mod_info);
    displayModule_registerModule(&net_out_mod_info);
    displayModule_registerModule(&net_total_mod_info);
    displayModule_registerModule(&nfs_rpc_mod_info);
    displayModule_registerModule(&using_clusters_mod_info);
    displayModule_registerModule(&using_users_mod_info);
    displayModule_registerModule(&infod_debug_mod_info);
    displayModule_registerModule(&cluster_id_mod_info);

    displayModule_registerModule(&eco_info_mod_info);
    displayModule_registerModule(&ncpus_mod_info);
    displayModule_registerModule(&seperator_mod_info);
    displayModule_registerModule(&space_mod_info);

    return 1;
}

int load_symbol(void *hndl, char *name, void **address)
{
    char *error;

    dlerror(); // To clear previous errors if any ..

    *address = dlsym(hndl, name);
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "Error: loading %s\nError:%s\n", name, error);
        return 0;
    }
    return 1;
}

int load_external_md_module(char *file, mon_display_module_t **mdm)
{

    void *hndl;

    /* open the file holding the funcs */
    if (!(hndl = dlopen(file, RTLD_LAZY))) {
        fprintf(stderr, "Error: %s\n", dlerror());
        return 0;
    }

    // Note that  the correct way to check error on dlsym is to
    // check the return value of dlerror(). See man dlsym.
    void *ptr;
    if (!load_symbol(hndl, "mon_display_arr", (void **) &ptr)) {
        fprintf(stderr, "Failed loading a required function/symbol\n");
        return 0;
    }
    //fprintf(stderr, "Pointer is %p\n", ptr);
    *mdm = ptr;
    return 1;
}

int displayModule_detectExternalPlugins(PluginConfig_t *p)
{
    mlog_bn_info("plugins", "Detecting external plugins at:[%s]\n", GOSSIMON_ENABLED_PLUGINS_DIR);

    GPtrArray *arr = g_ptr_array_new();

    // Getting the modules from the plugins.names variable
    if (!getFilesInDir(GOSSIMON_ENABLED_PLUGINS_DIR, &arr, ".conf")) {
         mlog_bn_error("plugins", "Error getting files in directory [%s]\n", GOSSIMON_ENABLED_PLUGINS_DIR);
         return 0;
    }
    mlog_bn_info("plugins", "Found %d files in plugins-enabled dir\n", arr->len);

    if (arr->len > 0) {
        p->pluginsNum = arr->len;
        p->pluginsList = malloc(sizeof (char *) * arr->len + 1);
        for (int i = 0, j = 0; i < arr->len; i++) {
            char *ptr = g_ptr_array_index(arr, i);
            mlog_bn_info("plugins", "Got plugin config file: %s\n", ptr);
            if (strchr(ptr, '/')) {
                mlog_bn_warn("plugins", "External module name [%s] containes / skeeping\n", ptr);
                continue;
            }
            p->pluginsList[j] = ptr;
            // Taking the name without the .conf
            char *t = strrchr(ptr, '.');
            mlog_bn_dy("plugins", "Found . at [%s]\n", t);
            *t = '\0';
            j++;
        }
        p->pluginsList[arr->len] = NULL;
    }
    g_ptr_array_free(arr, FALSE);
}

int displayModule_registerExternalPlugins(PluginConfig_t *p)
{
    mlog_bn_info("plugins", "Registering %d plugins: %s \n", p->pluginsNum, p->pluginsDir);

    int i = 0;
    char *buff = malloc(2048);

    if (!p->pluginsList) return 0;

    while (p->pluginsList[i]) {
        sprintf(buff, "%s/%s.so", p->pluginsDir, p->pluginsList[i]);
        mlog_bn_info_color("plugins", color_blue, "Registring external plugin: %s\n", buff);
        i++;

        mon_display_module_t *mdmPtr;
        if (!load_external_md_module(buff, &mdmPtr)) {
            mlog_bn_warn("plugins", "Error loading module from %s\n", buff);
            continue;
        }
        void *ptr = mdmPtr;
        mon_display_module_t **ptr2 = (mon_display_module_t **) ptr;

        int i = 0;
        mon_display_module_t *tmp = ptr2[i];
        while (tmp) {
            mlog_bn_info("plugins", "Detected display module %p [%s]\n", tmp, tmp->md_name);
            displayModule_registerModule(tmp);
            i++;
            tmp = ptr2[i];
        }

    }


    sleep(1);
    return 1;
}

