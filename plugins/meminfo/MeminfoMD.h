/* 
 * File:   MeminfoMD.h
 * Author: lior
 *
 * Created on April 18, 2012, 1:50 PM
 */

#ifndef MEMINFOMD_H
#define	MEMINFOMD_H

#ifdef	__cplusplus
extern "C" {
#endif

int meminfo_init();
int meminfo_get_length ();
char** meminfo_display_help();
void meminfo_new_item(mmon_info_mapping_t* iMap, const void* source,
        void* dest, settings_t* setup);
    
void meminfo_del_item (void* address);
void meminfo_display_item(WINDOW* graph, Configurator* pConfigurator,
        const void* source, int base_row,
        int min_row, int col, const double max, int width);
int meminfo_text_info(void *source, char *buff, int *size, int width);
double meminfo_scalar_div_x (const void* item, double x);
void meminfo_update_info_desc(void *d, variable_map_t *glob_info_var_mapping);


#ifdef	__cplusplus
}
#endif

#endif	/* MEMINFOMD_H */

