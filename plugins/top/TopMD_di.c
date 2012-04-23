#include <DisplayModules.h>
#include <TopMD.h>
#ifdef  __cplusplus
extern "C" {
#endif
mon_display_module_t top_md_info = {
        md_name                 : "top",
        md_iscalar              : 0,
        md_canrev               : 1,
        md_title                : "Top",
        md_titlength            : 3,
        md_shortTitle           : "Top",
        md_format               : "%5.0f",
        md_key                  : 'T',

        md_init_func            : top_init,
        md_length_func          : top_get_length,
        md_scalar_func          : NULL,
        md_new_func             : top_new_item,
        md_del_func             : top_del_item,
        md_disp_func            : top_display_item,
        md_help_func            : top_display_help,

        md_update_info_desc_func : top_update_info_desc,
        md_text_info_func       : top_text_info,
        md_side_window_display  : 1

    };



mon_display_module_t *mon_display_arr[] = {&top_md_info, NULL};
#ifdef  __cplusplus
extern "C" {
#endif

