#include <iostream>

#include <ModuleLogger.h>
#include <Meminfo.h>
#include <MeminfoSaxParser.h>
#include <MeminfoPIM.h>
#include <string.h>

char buff[8192];

int main(int argc, char **argv) 
{
    void *data;
    mlog_init();
//    MeminfoSaxParser parser1;
        
    // Debugging if argc > 1
    if(argc > 1) {

        mlog_registerColorFormatter((color_formatter_func_t)sprintf_color);
        mlog_addOutputFile((char *)"/dev/tty", 1);
        mlog_setModuleLevel((char *)"meminfo", LOG_DEBUG);
        mlog_setModuleLevel((char *)"meminfoparser", LOG_DEBUG);
    
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

        
        MeminfoSaxParser parser;
        Meminfo mi;
        parser.parse(std::string(buff), mi);
       
        std::cout << mi.get_xml();
        std::cout << "====================================================" << std::endl << std::endl;
        sleep(1);

    }
    
    // free
    im_free(&data);
    
    free(buff);
    return 0;
}       

