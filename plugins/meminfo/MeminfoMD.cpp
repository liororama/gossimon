/* 
 * File:   MeminfoMD.cpp
 * Author: lior
 * 
 * Created on April 18, 2011,8:30 AM
 */
#include <mmon.h>
#include "DisplayModules.h"



#include <Meminfo.h>
#include <MeminfoSaxParser.h>

#include "ModuleLogger.h"
extern "C" {
    
    static mon_display_module_t *meminfo_dm;
    static int mlog_id;
    static int mlog_id2;
    static MeminfoSaxParser meminfo_parser;
    static char *vlen_buff;
        
    
    char* mem_desc[] = {
        (char *)" Used Memory plugin, showing also cached and buffers memory  ",
        (char*) NULL
    };

    int meminfo_init() {
        mlog_registerModule("meminfo", "Memory information module", "meminfo");
        mlog_registerModule("meminfo2", "Memory information module debug2", "meminfo2");
        
        mlog_getIndex("meminfo", &mlog_id);
        mlog_getIndex("meminfo2", &mlog_id2);
        
        vlen_buff = (char *)malloc(1024);
        if(!vlen_buff) {
            mlog_error(mlog_id, "Error allocating memory for vlen buff\n");
            return 0;
        }
        return 1;
    }

    int meminfo_get_length() {
        return sizeof (Meminfo);

    }

    char** meminfo_display_help() {
        return mem_desc;
    }

    void meminfo_new_item(mmon_info_mapping_t* iMap, const void* source,
            void* dest, settings_t* setup) {


        getVlenItem(((idata_entry_t*) source)->data,
                meminfo_dm->md_info_var_map, &vlen_buff);


        mlog_dg(mlog_id2, "vlen_buff: [%s]", vlen_buff);
        
        std::string xml_str(vlen_buff);
        Meminfo mi;
        meminfo_parser.parse(std::string(xml_str), mi);
        *(Meminfo *)(dest) = mi;
    }

    void meminfo_del_item(void* address) {
       // if (*((char**) address))
       //     free(*((char**) address));
    }

    void meminfo_display_item(WINDOW* graph, Configurator* pConfigurator, const void* source, 
                              int base_row, int min_row, int col, 
            const double max, int width) {
        Meminfo mi = *(Meminfo *) source;
        mlog_dg(mlog_id2, "meminfo:==== [%s]\n max %f", mi.get_xml().c_str(), max);
        
        int bar_len = base_row - min_row;
        
        float used_mb = (mi._total_kb - mi._free_kb - mi._cached_kb - mi._buffers_kb)/1024.0;
        int used_len = bar_len * used_mb / max; 
        
        float buff_cached_mb = (mi._cached_kb + mi._buffers_kb)/1024.0;
        int   buff_cached_len = bar_len * buff_cached_mb / max;
        
        float free_mb = mi._free_kb / 1024.0;
        int   free_len = bar_len * free_mb/ max;
        
        
        //display_rtr(graph, &(pConfigurator->Colors._statusNodeName),
       //         base_row - 1, col, ss, strlen(ss), 0);
        char used_ch, buff_cached_ch, free_ch;
        int rev;
        
                
        // Case of error
        if(max <= 0) {
                char ss[50];
                sprintf(ss, "Error");
                display_rtr(graph, &(pConfigurator->Colors._statusNodeName),
                            base_row - 1, col, ss, strlen(ss), 0);
                return;
        }

        if (width == 1) {
            used_ch = '|';
            buff_cached_ch = '*';
            free_ch = '+';
            rev = 0;
            
        } else {
            used_ch = ' ';
            buff_cached_ch = 176;
            free_ch = '+';
            rev = 1;
        }
        display_bar(graph, base_row, col, used_len, used_ch,
                &(pConfigurator->Colors._swapCaption), rev);
        display_bar(graph, base_row - used_len, col, buff_cached_len, buff_cached_ch,
                &(pConfigurator->Colors._swapCaption), 0);
        display_bar(graph, base_row - used_len - buff_cached_len, col, free_len, free_ch,
                &(pConfigurator->Colors._swapCaption), 0);
    }

    int meminfo_text_info(void *source, char *buff, int *size, int width) {

        Meminfo mi = *(Meminfo *) source;

        float total_gb = mi._total_kb / (1024.0*1024.0);
        float used_gb = (mi._total_kb - mi._free_kb - mi._cached_kb - mi._buffers_kb) / (1024.0*1024.0);
        float buffer_gb = mi._buffers_kb / (1024.0*1024.0);
        float cached_gb = mi._cached_kb / (1024.0*1024.0);
        float free_gb = mi._free_kb / (1024.0*1024.0);

        *size = sprintf(buff,
                "Total:  %7.2f GB\n"
                "Free:   %7.2f GB\n"
                "Used:   %7.2f GB\n"
                "Buff:   %7.2f GB\n"
                "Cached: %7.2f GB\n"
                "Dirty:  %7ld KB\n"
                "Locked: %7ld KB",
                total_gb, free_gb, used_gb, buffer_gb, cached_gb, mi._dirty_kb, mi._mlocked_kb);
        return *size;
    }

    double meminfo_scalar_div_x(const void* item, double x) {
        
        Meminfo *mi = (Meminfo *) item;
        float total_mb = (float)mi->_total_kb / 1024.0;
        return (double) (total_mb) / x;
        //we return the maximal total value
    }

    void meminfo_update_info_desc(void *d, variable_map_t *glob_info_var_mapping) {
        char plugin_info_name[] = "meminfo";
        mon_display_module_t *md = (mon_display_module_t *) d;

        meminfo_dm = md;

        md->md_info_var_map = get_var_desc(glob_info_var_mapping, plugin_info_name);
        return;
    }

}