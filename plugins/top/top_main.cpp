#include <iostream>

#include <ModuleLogger.h>
#include <TopPIM.h>

char buff[8192];

int main(int argc, char **argv) 
{
    void *data;
    mlog_init();
    
    // Debugging if argc > 1
    if(argc > 1) {

        mlog_registerColorFormatter((color_formatter_func_t)sprintf_color);
        mlog_addOutputFile((char *)"/dev/tty", 1);
        mlog_setModuleLevel((char *)"top", LOG_DEBUG);
    }
    // init
    im_init(&data, NULL);
    
    // sleep
    sleep(1);
    
    while(1) {
        // update
        im_update(data);
        
        int i = 8192;
        im_get(data, buff, &i);
        //system("clear");
        printf("%s", buff);
        printf("\n");
        sleep(1);
    }
    
    // free
    im_free(&data);
    
    free(buff);
    return 0;
}       

