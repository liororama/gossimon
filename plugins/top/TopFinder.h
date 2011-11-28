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

using namespace std;

struct ProcessStatusInfo {
    int      _pid;
    string   _command;
    int      _uid;
    float      _memoryMB;
    unsigned long _utime;
    unsigned long _stime;

    float _cpuPercent;
    struct timeval prevTime;
    struct timeval currTime;


};

class TopFinder {
public:
    TopFinder(string procDir);
    TopFinder(const TopFinder& orig);
    virtual ~TopFinder();

    void setMlogId(int id) { _mlogId = id ; }
    string getTopXML() {return _xmlTopResult; }
    void   getTopXML(char *buff, int *len);
    void update();

private:
    bool    readProcessStatus(string processProcDir, ProcessStatusInfo *pi);

    std::unordered_map<int, ProcessStatusInfo *> _procHash;
    string   _procDir;
    string   _xmlTopResult;
    int      _mlogId;
    string   _errMsg;
};


#endif	/* TOPFINDER_H */

