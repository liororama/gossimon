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
#include <sys/sysinfo.h>


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
    _proc_dir = procDir;
    _xml_result = "test 12345";
    _min_cpu_percent = 10;
    _min_mem_percent = 10;
    _update_count = 0;

    struct sysinfo si;
    sysinfo(&si);
    
    _total_mem_mb = ((float)si.totalram * (float)si.mem_unit)/(1024.0*1024.0);

}

TopFinder::TopFinder(const TopFinder& orig) {
}

TopFinder::~TopFinder() {
}

std::string TopFinder::get_info() {
	std::stringstream ss;

	ss << "Proc Dir:        " << _proc_dir << std::endl;	
	ss << "Min cpu percent: " << _min_cpu_percent <<std::endl;
	ss << "Min mem percent: " << _min_mem_percent <<std::endl;
	ss << "Total Ram MB:    " << _total_mem_mb <<std::endl;
	ss << "Update count:    " << _update_count << std::endl;
	return ss.str();
}

bool TopFinder::get_xml(char *buff, int *len) {
    
    if((int)_xml_result.length() > *len) {
        mlog_error(_mlog_id, "Error in TopFinder::getTopXML length of buffer is smaller than length of top xml string (%d : %d) \n", (int)_xml_result.length() , *len);
	//fprintf(stderr, _xml_result.c_str());
        return false;
    }
    
    memcpy(buff, _xml_result.c_str(), _xml_result.length());
    buff[_xml_result.length()] = '\0';
    *len = _xml_result.length() + 1;
    return true;
}


void TopFinder::update() {

    mlog_dg(_mlog_id, "update %d\n", _update_count);
    DIR *procDirHndl;

    procDirHndl = opendir(_proc_dir.c_str());
    if(!procDirHndl) {
        mlog_error(_mlog_id, (const char *)"Error opening dir [%s]\n", _proc_dir.c_str());
        return;
    }

    struct dirent *p_dir;
    int entries = 0;
    int procNum = 0;

    clearProcessesFlg();
    gettimeofday(&_curr_time, NULL);
    _clock_ticks_per_sec = sysconf(_SC_CLK_TCK);
    
    while((p_dir = readdir(procDirHndl))) {
        entries ++;
        // Taking only the /proc entries that starts with a digit
        if(p_dir->d_name[0] >= '0' && p_dir->d_name[0] < '9') {
            procNum++;
            ProcessStatusInfo pi;
            std::string processProcDir = _proc_dir + "/" + p_dir->d_name;
            
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
    _total_procs = procNum;
    _update_count ++;
    mlog_dy(_mlog_id, "Scanned: %d    Procs: %d   Updates: %d\n", entries, procNum, _update_count);
          
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
    
    // The first cycle is ignored - since there is not enough info collected
    if(_update_count > 1) 
	    piOrig->_cpuPercent = ((utimeClockTicksDiff/(float)_clock_ticks_per_sec) / timeDiff) * 100;
    else 
    	    piOrig->_cpuPercent = 0;	    
    piOrig->_stime = pi._stime;
    piOrig->_utime = pi._utime;
    
    if(_total_mem_mb > 0) {
        piOrig->_memPercent = (piOrig->_memoryMB / _total_mem_mb) * 100;
    }
    
}

bool TopFinder::updateProcessStatus(ProcessStatusInfo &pi) {
    if(_procHash.count(pi._pid) == 0) {
        mlog_db(_mlog_id, "New process %d  \n", pi._pid);
        
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
    mlog_dy(_mlog_id, "Removing old processes [%d] proc in map\n", _procHash.size());
    
    std::unordered_map<int, ProcessStatusInfo>::const_iterator iter;
    std::vector<int> vec_proc_to_remove;
    for(iter = _procHash.begin() ; iter != _procHash.end() ; iter++) {
        const ProcessStatusInfo *ps = &(iter->second);
        //mlog_dy(_mlog_id, "Checking %d\n", ps->_pid);
        
        if(!ps->_flg) {
                mlog_dg(_mlog_id, "Removing %d  \n", ps->_pid);
		vec_proc_to_remove.push_back(ps->_pid);
	}
    }
    for(unsigned int i=0 ; i < vec_proc_to_remove.size() ; i++) {
    	_procHash.erase(vec_proc_to_remove[i]);
    }

    mlog_dy(_mlog_id, "=========== Done removing ===============\n");

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
    pi->_currTime = _curr_time;
    mlog_dg(_mlog_id2, "Proc %5d Comm: %15s mem %5.2f\n", pi->_pid, pi->_command.c_str(), pi->_memoryMB);
    return true;
}


void TopFinder::updateTopProcessesList() {
    
    _top_processes_vec.clear();
    
    std::unordered_map<int, ProcessStatusInfo>::const_iterator iter;
    for(iter = _procHash.begin() ; iter != _procHash.end() ; iter++) {
        const ProcessStatusInfo *ps = &(iter->second);
        mlog_dy(_mlog_id2, "Proc %f %f %d\n", ps->_cpuPercent, _min_cpu_percent, _update_count);
	if(ps->_cpuPercent > _min_cpu_percent) {
            _top_processes_vec.push_back(ps->_pid);
        }

    
    }
    
}
    
void TopFinder::updateTopProcessesXML() {
    
    std::vector<int>::iterator iter;
    std::stringstream ss;
    
    _xml_result.clear();
    _xml_result = "<top>\n";
    
    // Internal module info
    ss << "<total_processes>" << _total_procs << "</total_processes>\n";
    ss << "<update_count>" << _update_count << "</update_count>\n"; 
    _xml_result += ss.str();
    
    // Each process info
    for(iter = _top_processes_vec.begin() ; iter != _top_processes_vec.end() ; iter++) {
        int pid = *iter;
        ProcessStatusInfo *pi = &_procHash[pid];
        _xml_result += pi->getProcessXML();
        
    }
    _xml_result += "</top>\n";
    
}
    
