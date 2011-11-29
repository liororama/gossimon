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

    void setMlogId(int id) { _mlogId = id ; }
    void setMlogId2(int id) { _mlogId2 = id ; }
    
    std::string getTopXML() {return _xmlTopResult; }
    bool getTopXML(char *buff, int *len);
    void update();

    void setMinCPUPercent(float minCPUPercent) { _minCPUPercent = minCPUPercent; }
    void setMinMemPercent(float minMemPercent) { _minMemPercent = minMemPercent; }
    void setTotalMem(float totalMemMB) { _totalMemMB = _totalMemMB; }
    
private:
    bool    readProcessStatus(std::string processProcDir, ProcessStatusInfo *pi);
    bool    updateProcessStatus(ProcessStatusInfo &pi);
    void    mergeProcessStatus(ProcessStatusInfo &pi);
    void    clearProcessesFlg();
    bool    removeOldProcesses();
    void    updateTopProcessesList();
    void    updateTopProcessesXML();        
    
    std::unordered_map<int, ProcessStatusInfo   > _procHash;
    std::vector<int>    _topProcessesVec;
    std::string         _procDir;
    std::string         _xmlTopResult;
    int                 _mlogId;
    int                 _mlogId2;
    std::string         _errMsg;
    
    struct timeval      _currTime;
    long                _clockTicksPerSec;

    float               _minCPUPercent;  // The minimal cpu percent processes to add to the result xml
    float               _minMemPercent;  // The minimal memory percent processes to add to the result xml
    float               _totalMemMB;

    int                 _totalProcs;    // Total number of processes on this node
    int                 _updateCount;   // Number of times the main update method was called
};


#endif	/* TOPFINDER_H */

