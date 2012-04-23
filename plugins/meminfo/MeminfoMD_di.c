#include <DisplayModules.h>
#include <MeminfoMD.h>
#ifdef  __cplusplus
extern "C" {
#endif
mon_display_module_t meminfo_md_info = {
        md_name                 : "meminfo",
        md_iscalar              : 1,
        md_canrev               : 1,
        md_title                : "Memory Usage (adv)",
        md_titlength            : 18,
        md_shortTitle           : "Mem",
        md_format               : "%5.0f",
        md_key                  : 'm',

        md_init_func            : meminfo_init,
        md_length_func          : meminfo_get_length,
        md_scalar_func          : meminfo_scalar_div_x,
        md_new_func             : meminfo_new_item,
        md_del_func             : meminfo_del_item,
        md_disp_func            : meminfo_display_item,
        md_help_func            : meminfo_display_help,

        md_update_info_desc_func : meminfo_update_info_desc,
        md_text_info_func       : meminfo_text_info,
        md_side_window_display  : 1

    };



mon_display_module_t *mon_display_arr[] = {&meminfo_md_info, NULL};
#ifdef  __cplusplus
extern "C" {
#endif

