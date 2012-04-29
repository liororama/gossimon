#include <iostream>

#include <ModuleLogger.h>
#include <TopPIM.h>
#include <TopSaxParser.h>
#include <string.h>

char buff[16000];

int main(int argc, char **argv) 
{
    void *data;
    mlog_init();
    TopSaxParser parser1;
        
    // Debugging if argc > 1
    if(argc > 1) {

        mlog_registerColorFormatter((color_formatter_func_t)sprintf_color);
        mlog_addOutputFile((char *)"/dev/tty", 1);
        mlog_setModuleLevel((char *)"top", LOG_DEBUG);
        mlog_setModuleLevel((char *)"topparser", LOG_DEBUG);
    
    }
    // init
    im_init(&data, NULL);
    
    // sleep
    sleep(1);
    
    while(1) {
        // update
        im_update(data);
        
        int i = 16000;
        im_get(data, buff, &i);
        //system("clear");
        //printf("%s", buff);
        //printf("\n");

        
        processVecT v;
        TopSaxParser parser;
        parser.parse(std::string(buff), v);
        std::cout << "================= Top Processes: " << v.size() << " ======================" << std::endl;
        for(std::vector<ProcessStatusInfo>::iterator iter = v.begin() ; iter != v.end() ; iter++) {
            ProcessStatusInfo *pi = &(*iter);
            std::cout << "XML:" << std::endl << pi->getProcessXML();
        }
        sleep(3);

    }
    
    // free
    im_free(&data);
    
    free(buff);
    return 0;
}       

