/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __ConfigurationManager_H_
#define __ConfigurationManager_H_

typedef struct ColorDescriptor_t
{
      short color_pair;
      int  attr_flags;
} ColorDescriptor;


typedef struct Appearance_t
{
     char _verticalTableChar;
     char _horizTableChar;
     char _memswapGraphChar;
     char _seperatorChar;
} Appearance_t;


typedef struct Colors_t
{
     ColorDescriptor _frameCurrentOperation;
     ColorDescriptor _chartColor;
     ColorDescriptor _nodeCaption;
     ColorDescriptor _copyrightsCaption;
     ColorDescriptor _multicluster;
     ColorDescriptor _nodeNameGridMode;
     ColorDescriptor _vertNodeName;
     ColorDescriptor _horizNodeName;
     ColorDescriptor _idleCaption;
     ColorDescriptor _stringMB;
     ColorDescriptor _deadCaption;
     ColorDescriptor _loadCaption;
     ColorDescriptor _frozenCaption;
     ColorDescriptor _utilCaption;
     ColorDescriptor _nettotalCaption;
     ColorDescriptor _speedCaption;
     ColorDescriptor _swapCaption;
     ColorDescriptor _memswapCaption;
     ColorDescriptor _gridCaption;
     ColorDescriptor _reserveCaption;
     ColorDescriptor _uptimeStatus;
     ColorDescriptor _statusNodeName;
     ColorDescriptor _statusLetter;
     ColorDescriptor _bottomStatusbar;
     ColorDescriptor _currentLoad;
     ColorDescriptor _errorStr;
     ColorDescriptor _fairShareInfo;
     ColorDescriptor _seperatorBar;
      
} Colors_t;

typedef struct _ExternalModule {
     char    *pluginsDir;
     int      pluginsNum;
     char   **pluginsList;
} PluginConfig_t;



typedef struct Configurator_t
{
     Colors_t Colors;
     Appearance_t Appearance;
     PluginConfig_t Plugins;
} Configurator;

int loadConfig(Configurator** newconfig, const char* fileName, int color_mode);
int loadConfigEx(Configurator** newconfig, const char* dirName, const char* fileName);

#endif
