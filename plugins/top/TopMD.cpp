/* 
 * File:   TopMD.cpp
 * Author: lior
 * 
 * Created on April 19, 2011,13:35 AM
 */
#include <mmon.h>
#include "DisplayModules.h"



#include <TopFinder.h>
#include <TopSaxParser.h>
#include <UidToUser.h>
#include "ModuleLogger.h"
extern "C" {
    
    static mon_display_module_t *top_dm;
    static int mlog_id;
    static int mlog_id2;
    static TopSaxParser top_parser;
    static UidToUser u2u;
    
    char* top_desc[] = {
        " Top plugin, show top cpu users  ",
        (char*) NULL
    };

    int top_init() {
        mlog_registerModule("top", "Top information module", "top");
        mlog_registerModule("top2", "Top information module debug2", "top2");
        
        mlog_getIndex("top", &mlog_id);
        mlog_getIndex("top2", &mlog_id2);
        
       return 1;
    }

    int top_get_length() {
        return sizeof (char *);
    }

    char** top_display_help() {
        return top_desc;
    }

    void top_new_item(mmon_info_mapping_t* iMap, const void* source,
            void* dest, settings_t* setup) {

        getVlenItem(((idata_entry_t*) source)->data,
                top_dm->md_info_var_map, (char **)dest);

        mlog_dg(mlog_id2, "xml from new_item: [%s]", *(char **)dest);
    }

    void top_del_item(void* address) {
        if (*((char**) address))
            free(*((char**) address));
    }
    
int get_uids_from_vec(processVecT &proc_vec, std::vector<int> &uid_vec) {
    std::map<int, int> uid_map;
    uid_vec.clear();
            
    for( unsigned int i=0 ; i< proc_vec.size() ; i++) {
        uid_map[proc_vec[i]._uid] = 1;
    }
    
    for( std::map<int, int>::iterator it=uid_map.begin() ; it != uid_map.end() ; it++) {
        uid_vec.push_back((*it).first);
    }
    return uid_vec.size();
}    
    
void get_users_from_uids(std::vector<int> &uid_vec, std::vector<std::string> &users_vec) {
    users_vec.clear();
            
    for( unsigned int i=0 ; i< uid_vec.size() ; i++) {
        users_vec.push_back(u2u.get_user(uid_vec[i]));
    }
}

void print_users_bar(std::vector<std::string> &users_vec, int max_len, std::string &bar_str)
{
    int tmp_len = 0;
    int users_to_print = 0;
    for( int i=0 ; i< users_vec.size() ; i++) {
        if(tmp_len + users_vec[i].size() > bar_str.size() - 3)
            break;
        users_to_print ++;
        tmp_len += users_vec[i].size() + 1;
    }
    for( int i=0 ; i< users_to_print ; i++) {
        bar_str += users_vec[i] + " ";
    }
    if(users_to_print < users_vec.size())
        bar_str += "..";
}
        

void top_display_item(WINDOW* graph, Configurator* pConfigurator, const void* source,
            int base_row, int min_row, int col,
            const double max, int width) {

        std::string xml_str(*(char **) source);
        
        processVecT proc_vec;
        top_parser.parse(xml_str, proc_vec);
        mlog_dg(mlog_id2, "top:==== %d proc =====\n[%s]\n max %f\n", proc_vec.size(), xml_str.c_str(), max);

        std::vector<int> uid_vec;
        get_uids_from_vec(proc_vec, uid_vec);
        std::vector<std::string> users_vec;
        get_users_from_uids(uid_vec, users_vec);
        
        std::string bar_str;
        print_users_bar(users_vec, base_row - min_row, bar_str);
        mlog_db(mlog_id2, "%d users - [%s]\n", users_vec.size(), bar_str.c_str());
        display_rtr(graph, &(pConfigurator->Colors._statusNodeName),
                base_row - 1, col, (char*)bar_str.c_str(), bar_str.size(), 0);
    }

    int top_text_info(void *source, char *buff, int *size, int width) {

        std::string xml_str(*(char **) source);
        
        processVecT proc_vec;
        if( !top_parser.parse(xml_str, proc_vec)) {
            sprintf(buff, "Error: %s", top_parser.get_error().c_str());
            mlog_dg(mlog_id, "XML: [%s]\n", xml_str.c_str());
            *size = strlen(buff);
            return *size;
        }
        
        char *tmp_buff = buff;
        int len;
        
        len = sprintf(tmp_buff, "%-6s %-8s %-5s %-5s %s\n", "PID", "USER", "CPU", "MEM", "COMM");
        tmp_buff += len;
        //len = sprintf(tmp_buff, "size %d\n", proc_vec.size());
        //tmp_buff += len;
        
        for( int i=0 ; i< proc_vec.size() ; i++) {
            len = sprintf(tmp_buff, "%-6d %-8s %-5.0f %-5.1f %s\n", 
                    proc_vec[i]._pid,
                    u2u.get_user(proc_vec[i]._uid), 
                    proc_vec[i]._cpuPercent, 
                    proc_vec[i]._memoryMB,
                    proc_vec[i]._command.c_str());
            
            tmp_buff += len;
        }
        *size = tmp_buff - buff;
        return *size;
    }

   
    void top_update_info_desc(void *d, variable_map_t *glob_info_var_mapping) {
        char plugin_info_name[] = "top";
        mon_display_module_t *md = (mon_display_module_t *) d;

        top_dm = md;

        md->md_info_var_map = get_var_desc(glob_info_var_mapping, plugin_info_name);
        return;
    }

}