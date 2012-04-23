/* 
 * File:   TopFinder.cpp
 * Author: lior
 * 
 * Created on October 9, 2011, 7:40 AM
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <readproc.h>
#include <parse_helper.h>

#include <ModuleLogger.h>
#include "Meminfo.h"
#include "ProcessWatchInfo.h"

#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sstream>
#include <iomanip>

std::string Meminfo::get_xml(std::string root_name) {
    std::stringstream xmlStream;
    xmlStream << std::fixed;
    xmlStream << std::setprecision(1);
    
    xmlStream << "<" << root_name << ">\n";
    xmlStream << "\t<total>" << _total_kb << "</total>\n";
    xmlStream << "\t<free>" << _free_kb << "</free>\n";
    xmlStream << "\t<buffers>" << _buffers_kb << "</buffers>\n";
    xmlStream << "\t<cached>" <<   _cached_kb << "</cached>\n";
    xmlStream << "\t<dirty>" << _dirty_kb << "</dirty>\n";
    xmlStream << "\t<mlocked>" << _mlocked_kb << "</mlocked>\n";
    xmlStream << "</"<< root_name << ">\n";
    
    return xmlStream.str();
}
    


MeminfoDetector::MeminfoDetector(std::string procDir) {
    _proc_dir = procDir;
    _meminfo_file = procDir + std::string("/") + std::string("meminfo");
    _xml_result = "test 12345";
    _update_count = 0;
    _tmp_buff = NULL;
    _tmp_buff_len = 0;
    
}

MeminfoDetector::MeminfoDetector(const MeminfoDetector& orig) {
}

MeminfoDetector::~MeminfoDetector() {
}

bool MeminfoDetector::get_xml(char *buff, int *len) {
    
    if((int)_xml_result.length() > *len) {
        mlog_error(_mlog_id, "Error in TopFinder::getTopXML length of buffer is smaller than length of top xml string\n");
        return false;
    }
    
    memcpy(buff, _xml_result.c_str(), _xml_result.length());
    buff[_xml_result.length()] = '\0';
    *len = _xml_result.length() + 1;
    mlog_dg(_mlog_id, "xml: [%s]\n", buff);
    return true;
}

/*
 * Reading the entire content of a file to a buffer. Allocating the buffer
 * as required.
 * Return 1 on success and then buff contains the buffer and buff_len the length
 * Return 0 on failure and the content of  buff and buff_len is unknown
 */
int read_all_file(const char *file, char **buff, int *buff_len, char *err) {
     int res;
     int fd;
     int initial_buff_size = 8192;
     
     if( *buff_len < 0) {  
         sprintf(err, "Error, buf length is < 0");
         return 0;
     }
     
     // Initial allocation of proc_file buff (called only once)
     if(*buff == NULL) {
         if(*buff_len > 0) {
             sprintf(err, "Error, buff = NULL and buff_len > 0");
             return 0;
         }
         *buff = (char *)malloc(initial_buff_size);
          if(!*buff) {
                sprintf(err, "Error allocating memory");
                return 0;
          }
          *buff_len = initial_buff_size;
     }

     /* open the file holding cpu information */ 
     if( ( fd = open( file, O_RDONLY )) <0 ){
	  sprintf(err, "Error: Opening file [%s]\n%s\n", file, strerror(errno));
	  return 0;
     }

     char *ptr = *buff;
     int size_left = *buff_len;
     int data_size = 0;
     while( (res = read( fd, ptr, size_left )) > 0 ){
	  ptr += res;
	  size_left -= res;
          data_size += res;
          
          // Buffer is full - trying to increase
          if(size_left == 0) {
               int new_size = *buff_len * 2;
              
               *buff = (char*) realloc(*buff, new_size);
               if(!buff) {
                    sprintf(err, "Error: increasing buffer from %d to %d", *buff_len, new_size);
                    return 0;
               }
               
               size_left = *buff_len;
               *buff_len = new_size;
               // Setting ptr again since realloc might change base address
               ptr = *buff + data_size;
          }
     }

     close(fd);
     if(res == 0) {
	 *buff_len = data_size;
         return 1;
     }
     if(res == -1) {
	  sprintf(err, "Error: Reading proc file [%s] \n%s\n", file, strerror(errno)) ;
	  return 0;
     }
     return 0;
}

bool MeminfoDetector::parse_meminfo_file() {

    char *buff_curr_pos = (char*) _tmp_buff;
    int line_buff_size = 256;
    char line_buff[256];
    int items_found = 0;
    int items_to_find = 6;
    int errors_detected = 0;
    
    while (buff_curr_pos) {
        int lineLen;
        char *newPos;

        newPos = buff_get_line(line_buff, line_buff_size, buff_curr_pos);

        if (!newPos)
            break;
        lineLen = newPos - buff_curr_pos;
        mlog_dy(_mlog_id, "Processing line (%d): %s\n", lineLen, line_buff);

        buff_curr_pos = newPos;
        
        if (strncmp("MemTotal:", line_buff, 9) == 0) {
            if (sscanf(line_buff + 9, "%ld", &_meminfo._total_kb) != 1)
                errors_detected++;
            items_found++;
        }
        if (strncmp("MemFree:", line_buff, 8) == 0) {
            if (sscanf(line_buff + 8, "%ld", &_meminfo._free_kb) != 1)
                errors_detected++;
            items_found++;
        }
        if (strncmp("Buffers:", line_buff, 8) == 0) {
            if (sscanf(line_buff + 8, "%ld", &_meminfo._buffers_kb) != 1)
                errors_detected++;
            items_found++;
        }
        if (strncmp("Cached:", line_buff, 7) == 0) {
            if (sscanf(line_buff + 7, "%ld", &_meminfo._cached_kb) != 1)
                errors_detected++;
            items_found++;
        }
        if (strncmp("Dirty:", line_buff, 6) == 0) {
            if (sscanf(line_buff + 6, "%ld", &_meminfo._dirty_kb) != 1)
                errors_detected++;
            items_found++;
        }
        if (strncmp("Mlocked:", line_buff, 8) == 0) {
            if (sscanf(line_buff + 8, "%ld", &_meminfo._mlocked_kb) != 1)
                errors_detected++;
            items_found++;
        }

        if (items_found == items_to_find)
            break;
    }

    return true;
}

    

void MeminfoDetector::update() {
    char err_str[256];
    
    mlog_dg(_mlog_id, "update\n");

    mlog_dg(_mlog_id, "Getting content of file %s\n", _meminfo_file.c_str());
    int res = read_all_file(_meminfo_file.c_str(), &_tmp_buff, &_tmp_buff_len, err_str);
    if(!res) {
        mlog_error(_mlog_id, err_str);
        return;
    }
    mlog_db(_mlog_id2, "%s", _tmp_buff);
    parse_meminfo_file();
    
    _xml_result = _meminfo.get_xml();
    
    _update_count ++;
    
    
          
}
