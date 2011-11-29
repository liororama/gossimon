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
#include <sstream>
#include <iomanip>

std::string ProcessStatusInfo::getProcessXML() {
    std::stringstream xmlStream;
    xmlStream << std::fixed;
    xmlStream << std::setprecision(1);
    
    xmlStream << "<process>\n";
    xmlStream << "\t<pid>" << _pid << "</pid>\n";
    xmlStream << "\t<name>" << _command << "</name>\n";
    xmlStream << "\t<uid>" << _uid << "</uid>\n";
    xmlStream << "\t<mem>" <<   _memoryMB << "</mem>\n";
    xmlStream << "\t<memPercent>" << _memPercent << "</memPercent>\n";
    xmlStream << "\t<cpuPercent>" << _cpuPercent << "</cpuPercent>\n";
    xmlStream << "</process>\n";
    
    return xmlStream.str();
}
    


TopFinder::TopFinder(std::string procDir) {
    _procDir = procDir;
    _xmlTopResult = "test 12345";
    _minCPUPercent = 20;
    _minMemPercent = 20;
    _totalMemMB = 1024; // Just a default of 1GB...
    _updateCount = 0;
}

TopFinder::TopFinder(const TopFinder& orig) {
}

TopFinder::~TopFinder() {
}

bool TopFinder::getTopXML(char *buff, int *len) {
    
    if((int)_xmlTopResult.length() > *len) {
        mlog_error(_mlogId, "Error in TopFinder::getTopXML length of buffer is smaller than length of top xml string\n");
        return false;
    }
    
    memcpy(buff, _xmlTopResult.c_str(), _xmlTopResult.length());
    buff[_xmlTopResult.length()] = '\0';
    *len = _xmlTopResult.length() + 1;
    return true;
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

    clearProcessesFlg();
    gettimeofday(&_currTime, NULL);
    _clockTicksPerSec = sysconf(_SC_CLK_TCK);
    
    while((p_dir = readdir(procDirHndl))) {
        entries ++;
        // Taking only the /proc entries that starts with a digit
        if(p_dir->d_name[0] >= '0' && p_dir->d_name[0] < '9') {
            procNum++;
            ProcessStatusInfo pi;
            std::string processProcDir = _procDir + "/" + p_dir->d_name;
            
            // The readProcessStatus must return true (the process can die in between)
            if(readProcessStatus(processProcDir, &pi)) {
                updateProcessStatus(pi);
            }
            
        }
    }
    closedir(procDirHndl);
    removeOldProcesses();
    
    updateTopProcessesList();
    updateTopProcessesXML();
    _totalProcs = procNum;
    _updateCount ++;
    mlog_dy(_mlogId, "Scanned: %d    Procs: %d   Updates: %d\n", entries, procNum, _updateCount);
          
}

inline float timeDiffFloat(struct timeval *start, struct timeval *end) {
     float secDiff = end->tv_sec - start->tv_sec;
     float usecDiff = end->tv_usec - start->tv_usec;
     if(usecDiff < 0) {
          secDiff -= 1;
          usecDiff += 1000000;
     }
     return secDiff + usecDiff/1000000;
}

void TopFinder::mergeProcessStatus(ProcessStatusInfo& pi) {
    ProcessStatusInfo *piOrig = &(_procHash[pi._pid]);

    // The following fields may change
    piOrig->_uid = pi._uid;
    piOrig->_memoryMB = pi._memoryMB; 
    
    
    // Updating times
    piOrig->_prevTime = piOrig->_currTime;
    piOrig->_currTime = pi._currTime;
    
    float timeDiff = timeDiffFloat(&piOrig->_prevTime, &piOrig->_currTime);
    float utimeClockTicksDiff = pi._utime - piOrig->_utime;
    
    piOrig->_cpuPercent = ((utimeClockTicksDiff/(float)_clockTicksPerSec) / timeDiff) * 100;
    
    piOrig->_stime = pi._stime;
    piOrig->_utime = pi._utime;
    
    if(_totalMemMB > 0) {
        piOrig->_memPercent = (piOrig->_memoryMB / _totalMemMB) * 100;
    }
    
}

bool TopFinder::updateProcessStatus(ProcessStatusInfo &pi) {
    if(_procHash.count(pi._pid) == 0) {
        mlog_db(_mlogId, "New process %d  \n", pi._pid);
        
        _procHash[pi._pid] = pi;
    } 
    else {
        // There is a previous reading so we can update the %cpu usage
        mergeProcessStatus(pi);
    }
    _procHash[pi._pid]._flg = 1;
    return true;
}
    
void TopFinder::clearProcessesFlg() {
    std::unordered_map<int, ProcessStatusInfo>::const_iterator iter;
    for(iter = _procHash.begin() ; iter != _procHash.end() ; iter++) {
        const ProcessStatusInfo *ps = &(iter->second);
        _procHash[ps->_pid]._flg = 0;
    }
}
bool TopFinder::removeOldProcesses() {
    mlog_dy(_mlogId, "Removing old processes\n");
    
    std::unordered_map<int, ProcessStatusInfo>::const_iterator iter;
    for(iter = _procHash.begin() ; iter != _procHash.end() ; iter++) {
        const ProcessStatusInfo *ps = &(iter->second);
        //mlog_dy(_mlogId, "Checking %d\n", ps->_pid);
        
        if(!ps->_flg) {
                mlog_dg(_mlogId, "Removing %d  \n", ps->_pid);
                _procHash.erase(ps->_pid);
        }
    }
    mlog_dy(_mlogId, "=================================\n");

    return true;
}
    
bool TopFinder::readProcessStatus(std::string processProcDir, ProcessStatusInfo *pi) {

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
    pi->_utime = pe.utime;
    pi->_uid = pe.uid;
    pi->_currTime = _currTime;
    mlog_dg(_mlogId2, "Proc %5d Comm: %15s mem %5.2f\n", pi->_pid, pi->_command.c_str(), pi->_memoryMB);
    return true;
}


void TopFinder::updateTopProcessesList() {
    
    _topProcessesVec.clear();
    
    std::unordered_map<int, ProcessStatusInfo>::const_iterator iter;
    for(iter = _procHash.begin() ; iter != _procHash.end() ; iter++) {
        const ProcessStatusInfo *ps = &(iter->second);
        if(ps->_cpuPercent > _minCPUPercent) {
            _topProcessesVec.push_back(ps->_pid);
        }

    
    }
    
}
    
void TopFinder::updateTopProcessesXML() {
    
    std::vector<int>::iterator iter;
    std::stringstream ss;
    
    _xmlTopResult.clear();
    _xmlTopResult = "<top>\n";
    
    // Internal module info
    ss << "<total_processes>" << _totalProcs << "</total_processes>\n";
    ss << "<update_count>" << _updateCount << "</update_count>\n"; 
    _xmlTopResult += ss.str();
    
    // Each process info
    for(iter = _topProcessesVec.begin() ; iter != _topProcessesVec.end() ; iter++) {
        int pid = *iter;
        ProcessStatusInfo *pi = &_procHash[pid];
        _xmlTopResult += pi->getProcessXML();
        
    }
    _xmlTopResult += "</top>\n";
    
}
    