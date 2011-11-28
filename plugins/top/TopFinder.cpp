/* 
 * File:   TopFinder.cpp
 * Author: lior
 * 
 * Created on October 9, 2011, 7:40 AM
 */

#include <readproc.h>

#include <ModuleLogger.h>
#include "TopFinder.h"
#include "ProcessWatchInfo.h"

#include <string.h>
#include <sys/types.h>
#include <dirent.h>

TopFinder::TopFinder(string procDir) {
    _procDir = procDir;
    _xmlTopResult = "test 12345";
}

TopFinder::TopFinder(const TopFinder& orig) {
}

TopFinder::~TopFinder() {
}

void TopFinder::getTopXML(char *buff, int *len) {
    memcpy(buff, _xmlTopResult.c_str(), _xmlTopResult.length());
    *len = _xmlTopResult.length();
}

void TopFinder::update() {

    mlog_dg(_mlogId, "update\n");
    DIR *procDirHndl;

    procDirHndl = opendir(_procDir.c_str());
    if(!procDirHndl) {
        mlog_error(_mlogId, (const char *)"Error opening dir [%s]\n", _procDir.c_str());
        return;
    }

    struct dirent *p_dir;
    int entries = 0;
    int procNum = 0;
    while(p_dir = readdir(procDirHndl)) {
        entries ++;
        if(p_dir->d_name[0] >= '0' && p_dir->d_name[0] < '9') {
            procNum++;
            ProcessStatusInfo pi;
            string processProcDir = _procDir + "/" + p_dir->d_name;
            readProcessStatus(processProcDir, &pi);
        }
    }
    closedir(procDirHndl);
    mlog_dy(_mlogId, "Scanned: %d    Procs: %d\n", entries, procNum);
}

bool TopFinder::readProcessStatus(string processProcDir, ProcessStatusInfo *pi) {

    proc_entry_t pe;
    int res = read_process_stat(processProcDir.c_str(), &pe);
    if(!res)
        return false;
    
    res = read_process_status(processProcDir.c_str(), &pe);
    if(!res)
        return false;
    
    pi->_command = pe.name;
    pi->_memoryMB = (pe.rss_sz * ((float)getpagesize() / (1024.0 * 1024.0)));
    pi->_pid = pe.pid;
    pi->_stime = pe.stime;
    pi->_uid = pe.utime;
    pi->_uid = pe.uid;

    mlog_dg(_mlogId, "Proc %5d Comm: %15s mem %5.2f\n", pi->_pid, pi->_command.c_str(), pi->_memoryMB);
    return true;
}
