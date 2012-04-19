/* 
 * File:   TopFinder.h
 * Author: lior
 *
 * Created on October 9, 2011, 7:40 AM
 */

#ifndef TOPFINDER_H
#define	TOPFINDER_H

#include <sys/time.h>
#include <string>
#include <vector>
#include <unordered_map>

//using namespace std;

struct ProcessStatusInfo {
    int            _pid;
    std::string    _command;
    int            _uid;
    float          _memoryMB;
    unsigned long  _utime;
    unsigned long  _stime;

    float          _cpuPercent;
    float          _memPercent;
    struct timeval _prevTime;
    struct timeval _currTime;
    
    int            _flg;  // The flg is used when iterating over processes and 
                          // marking the process that were already processed

    std::string getProcessXML();
};

class TopFinder {
public:
    TopFinder(std::string procDir);
    TopFinder(const TopFinder& orig);
    virtual ~TopFinder();

    void set_mlog_id(int id) { _mlog_id = id ; }
    void set_mlog_id2(int id) { _mlog_id2 = id ; }
    
    std::string get_xml() {return _xml_result; }
    bool get_xml(char *buff, int *len);
    void update();

    void setMinCPUPercent(float minCPUPercent) { _min_cpu_percent = minCPUPercent; }
    void setMinMemPercent(float minMemPercent) { _min_mem_percent = minMemPercent; }
    void setTotalMem(float totalMemMB) { _total_mem_mb = totalMemMB; }
    
private:
    bool    readProcessStatus(std::string processProcDir, ProcessStatusInfo *pi);
    bool    updateProcessStatus(ProcessStatusInfo &pi);
    void    mergeProcessStatus(ProcessStatusInfo &pi);
    void    clearProcessesFlg();
    bool    removeOldProcesses();
    void    updateTopProcessesList();
    void    updateTopProcessesXML();        
    
    std::unordered_map<int, ProcessStatusInfo   > _procHash;
    std::vector<int>    _top_processes_vec;
    std::string         _proc_dir;
    std::string         _xml_result;
    int                 _mlog_id;
    int                 _mlog_id2;
    std::string         _err_msg;
    
    struct timeval      _curr_time;
    long                _clock_ticks_per_sec;

    float               _min_cpu_percent;  // The minimal cpu percent processes to add to the result xml
    float               _min_mem_percent;  // The minimal memory percent processes to add to the result xml
    float               _total_mem_mb;

    int                 _total_procs;    // Total number of processes on this node
    int                 _update_count;   // Number of times the main update method was called
};


#endif	/* TOPFINDER_H */

