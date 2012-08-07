/* 
 * File:   TopFinder.h
 * Author: lior
 *
 * Created on October 9, 2011, 7:40 AM
 */

#ifndef MEMINFO_H
#define	MEMINFO_H

#include <sys/time.h>
#include <string>
#include <vector>
#include <unordered_map>

//using namespace std;

struct Meminfo {
    long                _total_kb;
    long                _free_kb;
    long                _buffers_kb;
    long                _cached_kb;
    long                _dirty_kb;
    long                _mlocked_kb;
    long                _swap_total_kb;
    long                _swap_free_kb;
    
    std::string get_xml(std::string root_name = "meminfo");

};


class MeminfoDetector {
public:
    MeminfoDetector(std::string procDir);
    MeminfoDetector(const MeminfoDetector& orig);
    virtual ~MeminfoDetector();

    void set_mlog_id(int id) { _mlog_id = id ; }
    void set_mlog_id2(int id) { _mlog_id2 = id ; }
    
    std::string get_xml() {return _xml_result; }
    bool get_xml(char *buff, int *len);
    void update();

    
private:
    
    bool                parse_meminfo_file();
    
    std::string         _proc_dir;
    std::string         _meminfo_file;
    std::string         _xml_result;
    int                 _mlog_id;
    int                 _mlog_id2;
    std::string         _err_msg;
    
    struct timeval      _curr_time;
    long                _clock_ticks_per_sec;

    int                 _update_count;   // Number of times the main update method was called
    char               *_tmp_buff;
    int                 _tmp_buff_len;
    Meminfo             _meminfo;
};


#endif	/* TOPFINDER_H */

