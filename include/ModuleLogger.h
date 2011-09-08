/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2010 Cluster Logic Ltd

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright-ClusterLogic.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

/******************************************************************************
 *
 * Author(s): Amar Lior
 *
 * Created on November 14, 2010, 9:49 AM
 *
 ******************************************************************************/

#ifndef COLORLEVELLOG_H
#define	COLORLEVELLOG_H

#ifdef	__cplusplus
extern "C" {
#endif
#include "ColorPrint.h"
#include <syslog.h>

    // Default log level for all modules
#define MLOG_DEFAULT_LEVEL   LOG_ERR
    extern int mlog_modulesInDebug;

    int mlog_init();
    int mlog_registerModule(char *name, char *desc, char *shortName);
    int mlog_getIndex(char *name, int *index);

    //void mlog_diableAllModules();
    //int    mlog_disableModule(char *mode);
    //int    mlog_enableModule(char *mode);
    //int    mlog_enableModuleFromStr(char *str);


    // Set all modules levels (including future registered modules)
    int mlog_setLevel(int level);
    // Set a specific module level
    int mlog_setModuleLevel(char *name, int level);
    int mlog_setModuleLevelFromStr(char *str, int level);

    char *mlog_getAllModulesDesc(char *buff);

    void mlog(int moduleIndex, int level, int color, char *fmt, ...);
    void mlog_byName(char *moduleName, int level, int color, char *fmt, ...);

// Macros for error
#define mlog_error(index,fmt,args...)\
    do{ mlog(index,LOG_ERR,color_red,fmt, ## args);} while(0)

#define mlog_bn_error(name,fmt,args...)\
    do{ mlog_byName(name,LOG_ERR,color_red,fmt, ## args);} while(0)


// Macros for warning
#define mlog_warn(index,fmt,args...)\
    do{ mlog(index,LOG_WARNING,color_red,fmt, ## args);} while(0)

#define mlog_bn_warn(name,fmt,args...)\
    do{ mlog_byName(name,LOG_WARNING,color_red,fmt, ## args);} while(0)


// Macros for info
#define mlog_info(index,fmt,args...)\
    do{ mlog(index,LOG_INFO,color_green,fmt, ## args);} while(0)

#define mlog_info_color(index,color,fmt,args...)\
    do{ mlog(index,LOG_INFO,color,fmt, ## args);} while(0)

#define mlog_bn_info(name,fmt,args...)\
    do{ mlog_byName(name,LOG_INFO,color_green,fmt, ## args);} while(0)

#define mlog_bn_info_color(name,color,fmt,args...)\
    do{ mlog_byName(name,LOG_INFO,color,fmt, ## args);} while(0)



// Macros for debug
#define mlog_dbg(index,color,fmt,args...)\
    do{ mlog(index,LOG_DEBUG,color,fmt, ## args);} while(0)

#define mlog_bn_dbg(name,color,fmt,args...)\
    do{ mlog_byName(name,LOG_DEBUG,color,fmt, ## args);} while(0)

// Defines for index  and specific colors
#define mlog_dn(index,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_dbg(index,color_normal,fmt, ## args);} while(0)

#define mlog_dr(index,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_dbg(index,color_red,fmt, ## args);} while(0)

#define mlog_dg(index,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_dbg(index,color_green,fmt, ## args);} while(0)

#define mlog_dy(index,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_dbg(index,color_yellow,fmt, ## args);} while(0)

#define mlog_db(index,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_dbg(index,color_blue,fmt, ## args);} while(0)


    // Defining macros for by name and specific colors
#define mlog_bn_dn(name,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_bn_dbg(name,color_normal,fmt, ## args);} while(0)

#define mlog_bn_dr(name,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_bn_dbg(name,color_red,fmt, ## args);} while(0)

#define mlog_bn_dg(name,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_bn_dbg(name,color_green,fmt, ## args);} while(0)

#define mlog_bn_dy(name,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_bn_dbg(name,color_yellow,fmt, ## args);} while(0)

#define mlog_bn_db(name,fmt,args...) \
    do{ if(mlog_modulesInDebug) mlog_bn_dbg(name,color_blue,fmt, ## args);} while(0)


    int mlog_addOutputFile(char *path, int doColor);
    int mlog_addOutputTTY(char *path, int doColor);
    int mlog_addOutputFilePointer(FILE *fp, int doColor);
    int mlog_addOutputFileDescriptor(int fd, int doColor);
    int mlog_addOutputSyslog(char *prefix, int options, int facility);
    
    // The color formatter shoud cat the color the output buffer and the input buffer
    typedef int (*color_formatter_func_t) (color_t, char *, char *);
    int mlog_registerColorFormatter(color_formatter_func_t colorFormatter);

#ifdef	__cplusplus
}
#endif

#endif	/* COLORLEVELLOG_H */

