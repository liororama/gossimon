/* 
 * File:   TopMD.h
 * Author: lior
 *
 * Created on April 19, 2012, 1:33 PM
 */

#ifndef TOP_MD_H
#define	TOP_MD_H

#ifdef	__cplusplus
extern "C" {
#endif

int top_init();
int top_get_length ();
char** top_display_help();
void top_new_item(mmon_info_mapping_t* iMap, const void* source,
        void* dest, settings_t* setup);
    
void top_del_item (void* address);
void top_display_item(WINDOW* graph, Configurator* pConfigurator,
        const void* source, int base_row,
        int min_row, int col, const double max, int width);
int top_text_info(void *source, char *buff, int *size, int width);
double top_scalar_div_x (const void* item, double x);
void top_update_info_desc(void *d, variable_map_t *glob_info_var_mapping);


#ifdef	__cplusplus
}
#endif

#endif	/* topMD_H */

