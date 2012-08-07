/* 
 * File:   run_pim_plugin.c
 * Author: lior
 *
 * Created on April 18, 2012, 10:22 AM
 */
#include <dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <infoModuleManager.h>
#include <ModuleLogger.h>
/*
 * 
 */
int main(int argc, char** argv) {

    if(argc< 3) {
        fprintf(stderr, "Usage: %s plugin-dir plugin-name\n\n", argv[0]);
        return 1;
    }
   dlopen(NULL,RTLD_NOW|RTLD_GLOBAL);
    
   mlog_init();
        
   mlog_registerColorFormatter((color_formatter_func_t)sprintf_color);
   mlog_addOutputFile((char *)"/dev/tty", 1);
   mlog_setModuleLevel((char *)"pim", LOG_DEBUG);
   mlog_registerModule("pim", "Povider Info Module", "pim");

    char *plugin_name = strdup(argv[2]);
    char *plugin_dir = strdup(argv[1]);
    pim_entry_t pim_entry;
    
    load_external_module(&pim_entry, plugin_dir, plugin_name);
    
    void *data;
    int res = (pim_entry.init_func)(&data, NULL);
    if(!res) {
        fprintf(stderr, "Error runnin pim_init on module\n");
        return 0;
    }
    pim_entry.private_data = data;

    char *buff = (char *)malloc(80000);
    int size = 80000;
    for(int i=0; i< 10 ; i++) {
        (pim_entry.update_func)(pim_entry.private_data);
         res = (pim_entry.get_func)(pim_entry.private_data, (void *) buff, &size);
         printf("%s", buff);
         sleep(1);
    }
    
    return (EXIT_SUCCESS);
}

