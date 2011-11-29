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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include <glib-2.0/glib.h>
#include <string.h>
#include <term.h>

#include "ModuleLogger.h"

#define MLOG_MAX_MODULES    (256)
#define MLOG_MAX_CHANNELS   (20)
#define MLOG_MAX_LEVEL      (LOG_DEBUG + 5)

typedef struct moduleInfo {
    char *name;
    char *desc;
    char *shortStr;

    int index;
    int level;  // A possible per module level of debugging.

} moduleInfo_t;

typedef enum _mlogChannelType {
            channel_fp,
            channel_file,
            channel_tty,
            channel_fd,
            channel_syslog,
} mlogChannelType;


typedef struct mlogChannel {
    mlogChannelType type;
    char *path;
    FILE *fp;
    int   fd;

    int   doColor;
    int   doDate;
} mlogChannel_t;



struct mlogInfo {
    GHashTable              *nameHash;
    moduleInfo_t            mArr[MLOG_MAX_MODULES];
    int                     nr_modules;

    mlogChannel_t           channelArr[MLOG_MAX_CHANNELS];
    int                     nr_channel;

    color_formatter_func_t  color_formater;

    GHashTable              *pendingEnable;
    GHashTable              *pendingShortEnable;

    int                     defaultLevel;
    char                   *levelStr[MLOG_MAX_LEVEL + 3];
};


// The number of currently enabled levels. This is for fast checking if we need to
// log anything at all
int mlog_modulesInDebug = 0;

// The global mlog struct (we have a singleton here)
struct mlogInfo mlogInfo;
struct mlogInfo *_mlog;

char mlogBuff[1024];
char mlogBuff2[1024];
char mlogBuff3[1024];

int noColor(color_t color, char *str, char *colorStr, ...);
void mlog_print(int moduleIndex, int level, color_t color, char *buff);
int mlog_calcModulesInDebug();

int mlog_init()
{
    mlog_modulesInDebug = 0;
    _mlog = &mlogInfo;

    _mlog->nameHash = g_hash_table_new(g_str_hash, g_str_equal);
    if (!_mlog->nameHash) {
        fprintf(stderr, "Error initializing hash table\n");
        return 0;
    }
    _mlog->nr_modules = 0;
    memset(_mlog->mArr, 0, MLOG_MAX_MODULES * sizeof (int));

    _mlog->nr_channel = 0;
    memset(_mlog->channelArr, 0, MLOG_MAX_CHANNELS * sizeof (int));

    _mlog->color_formater = noColor;

    _mlog->pendingEnable = g_hash_table_new(g_str_hash, g_str_equal);
    _mlog->pendingShortEnable = g_hash_table_new(g_str_hash, g_str_equal);
    if (!_mlog->pendingEnable || !_mlog->pendingShortEnable) {
        fprintf(stderr, "Error initializing hash table\n");
        return 0;
    }

    _mlog->defaultLevel = MLOG_DEFAULT_LEVEL;

    _mlog->levelStr[LOG_EMERG]   = strdup("emerg");
    _mlog->levelStr[LOG_ALERT]   = strdup("alert");
    _mlog->levelStr[LOG_CRIT]    = strdup("crit");
    _mlog->levelStr[LOG_ERR]     = strdup("error");
    _mlog->levelStr[LOG_WARNING] = strdup("warn");
    _mlog->levelStr[LOG_NOTICE]  = strdup("notice");
    _mlog->levelStr[LOG_INFO]    = strdup("info");
    _mlog->levelStr[LOG_DEBUG]   = strdup("debug");
    _mlog->levelStr[LOG_DEBUG+1] = strdup("debug+1");
    _mlog->levelStr[LOG_DEBUG+2] = strdup("debug+2");
    _mlog->levelStr[LOG_DEBUG+3] = strdup("debug+3");
    _mlog->levelStr[LOG_DEBUG+4] = strdup("debug+4");
    _mlog->levelStr[LOG_DEBUG+5] = strdup("debug+5");
    _mlog->levelStr[LOG_DEBUG+6] = strdup("level not defined");

    return 1;
}

int mlog_registerModule(const char *name, const char *desc, const char *shortName)
{
    if (!name) return 0;

    if (_mlog->nr_modules >= MLOG_MAX_MODULES)
        return 0;

    //printf("MLOG register %s\n", name);
    // Looking if the module already exists
    if (g_hash_table_lookup(_mlog->nameHash, name)) {
        fprintf(stderr, "Found module %s in hash\n", name);
        return 0;
    }

    // Setting the module info
    moduleInfo_t *m = &_mlog->mArr[_mlog->nr_modules];
    m->name = strdup(name);
    m->desc = strdup(desc);
    m->shortStr = strdup(shortName);
    m->index = _mlog->nr_modules;

    // In case the module is in one of the pending enabled hashes
    int *level;
    if (level = g_hash_table_lookup(_mlog->pendingEnable, name)) {
        m->level = *level;
        g_hash_table_remove(_mlog->pendingEnable, name);
    } else if (level = g_hash_table_lookup(_mlog->pendingShortEnable, shortName)) {
        m->level = *level;
        g_hash_table_remove(_mlog->pendingShortEnable, shortName);
    }else {
        m->level = _mlog->defaultLevel;
    }

    // Adding the module
    g_hash_table_insert(_mlog->nameHash, (gpointer)name, m);
    _mlog->nr_modules++;
    mlog_modulesInDebug = mlog_calcModulesInDebug();
    return 1;
}

int mlog_getIndex(const char *name, int *index)
{
    moduleInfo_t *m = g_hash_table_lookup(_mlog->nameHash, name);
    if (m) {
        *index = m->index;
        return 1;
    }
    return 0;
}

int mlog_calcModulesInDebug() {
    int c = 0;
    for (int i = 0; i < _mlog->nr_modules; i++) {
        if(_mlog->mArr[i].level >= LOG_DEBUG) c++;
    }
    return c;
}

// Setting the level of all modules
int mlog_setLevel(int level) {
    
    if(level < 0) return 0;
    if(level > MLOG_MAX_LEVEL) level = MLOG_MAX_LEVEL;

    for (int i = 0; i < _mlog->nr_modules; i++) {
        _mlog->mArr[i].level = level;
    }    
    mlog_modulesInDebug = mlog_calcModulesInDebug();
}

int mlog_setModuleLevel(const char *mode, int level)
{
    moduleInfo_t *m;

    if(level < 0) return 0;
    if(level > MLOG_MAX_LEVEL) level = MLOG_MAX_LEVEL;

    m = g_hash_table_lookup(_mlog->nameHash, mode);
    if (m) {
        m->level = level;
        mlog_modulesInDebug = mlog_calcModulesInDebug();
        return 1;
    }// If the module does not exists we save the enable so if the modules is later defined it will be enabled
    else {
        g_hash_table_insert(_mlog->pendingEnable, (gpointer) mode, (gpointer)mode);
    }

    return 0;
}

int mlog_setModuleLevelFromStr(const char *str, int level)
{
    if (level < 0) return 0;
    if (level > MLOG_MAX_LEVEL) level = MLOG_MAX_LEVEL;

    for (int i = 0; i < _mlog->nr_modules; i++) {
        if (strcmp(str, _mlog->mArr[i].shortStr) == 0) {
            _mlog->mArr[i].level = level;
            mlog_modulesInDebug = mlog_calcModulesInDebug();
            return 1;
        }
    }
    g_hash_table_insert(_mlog->pendingShortEnable, (gpointer)str, (gpointer)str);
    mlog_modulesInDebug = mlog_calcModulesInDebug();
    return 0;
}

char *mlog_getAllModulesDesc(char *buff)
{
    char line[256];

    if (!buff)
        buff = malloc(2048);

    *buff = '\0';
    for (int i = 0; i < _mlog->nr_modules; i++) {
        sprintf(line, "%-20s %s\n", _mlog->mArr[i].name, _mlog->mArr[i].desc);
        strcat(buff, line);
    }
    return buff;
}

void mlog(int moduleIndex, int level, int color, const char *fmt, ...)
{
    if (moduleIndex < 0 || moduleIndex > MLOG_MAX_MODULES) {
        fprintf(stderr, "Error trying to access module out-of-range: %d", moduleIndex);
        return;
    }
    if (level < 0 || level > _mlog->mArr[moduleIndex].level)
        return;

    va_list ap;
    va_start(ap, fmt);
    vsprintf(mlogBuff, fmt, ap);
    va_end(ap);

    mlog_print(moduleIndex, level, color, mlogBuff);
}

void mlog_byName(const char *moduleName, int level, int color, char *fmt, ...)
{
    moduleInfo_t *m = g_hash_table_lookup(_mlog->nameHash, moduleName);
    if (!m) return;

    if (level < 0 || level > m->level)
        return;

    va_list ap;
    va_start(ap, fmt);
    vsprintf(mlogBuff, fmt, ap);
    va_end(ap);

    mlog_print(m->index, level, color, mlogBuff);
}

void mlog_print(int moduleIndex, int level, color_t color, char *buff)
{

    char *levelStr = _mlog->levelStr[level];
    sprintf(mlogBuff2, "%s  %-6s %s", _mlog->mArr[moduleIndex].name, levelStr, buff);
    (*_mlog->color_formater)(color, mlogBuff, mlogBuff2);
    for (int i = 0; i < _mlog->nr_channel; i++) {
        mlogChannel_t *c = &(_mlog->channelArr[i]);
        char *strToPrint = c->doColor ? mlogBuff : mlogBuff2;
        switch (c->type) {
        case channel_fp:
        case channel_file:
        case channel_tty:
            fwrite(strToPrint, strlen(strToPrint), 1, c->fp);
            break;

        case channel_fd:
            write(c->fd, strToPrint, strlen(strToPrint));
            break;

        case channel_syslog:
            if(level > LOG_DEBUG) level = LOG_DEBUG;
            syslog(level, "%s", strToPrint);
        }
    }
}

int mlog_addChannel(mlogChannelType type, int doColor, int doDate)
{
    if (_mlog->nr_channel >= MLOG_MAX_CHANNELS)
        return 0;

    mlogChannel_t *c = &(_mlog->channelArr[_mlog->nr_channel]);

    c->type = type;
    c->doColor = doColor;
    c->doDate = doDate;
    _mlog->nr_channel++;
    return 1;

}

int mlog_addOutputFile(char *path, int doColor)
{
    if (!path)
        return 0;

    FILE *fp = fopen(path, "a");
    if (!fp) {
        fprintf(stderr, "Error opening file: %s : %s", path, strerror(errno));
        return 0;
    }

    if (!mlog_addChannel(channel_file, doColor, 1)) {
        fclose(fp);
        return 0;
    }
    _mlog->channelArr[_mlog->nr_channel - 1].path = strdup(path);
    _mlog->channelArr[_mlog->nr_channel - 1].fp = fp;
    return 1;
}

int mlog_addOutputTTY(char *path, int doColor)
{
    int res = mlog_addOutputFile(path, doColor);
    if (res) {
        _mlog->channelArr[_mlog->nr_channel - 1].type = channel_tty;
    }
}

int mlog_addOutputFilePointer(FILE *fp, int doColor)
{
    if (!fp) return 0;
    if (!mlog_addChannel(channel_fp, doColor, 1)) {
        return 0;
    }
    _mlog->channelArr[_mlog->nr_channel - 1].fp = fp;
    return 1;
}

int mlog_addOutputFileDescriptor(int fd, int doColor)
{
    if (fd < 0) return 0;
    if (!mlog_addChannel(channel_fd, doColor, 1)) {
        return 0;
    }
    _mlog->channelArr[_mlog->nr_channel - 1].fd = fd;
    return 1;
}

int mlog_addOutputSyslog(char *prefix, int options, int facility)
{
    if (!mlog_addChannel(channel_syslog, 0, 0)) {
        return 0;
    }

    openlog(prefix, options, facility);

}

int mlog_registerColorFormatter(color_formatter_func_t colorFormatter)
{
    _mlog->color_formater = colorFormatter;
}

int noColor(color_t color, char *str, char *colorStr, ...)
{
    strcpy(colorStr, str);
    return strlen(str);
}


