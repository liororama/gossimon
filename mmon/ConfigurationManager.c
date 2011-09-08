/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "ConfFileReader.h"
#include "ConfigurationManager.h"
#include "curses.h"
#include "mmon.h"
#include "parse_helper.h"

void initConfig();
void parseLine(const char* line, int len);
void parseConfigParams(const char* var,
		       const char* foregroundColor,
		       const char* backgroundColor,
		       const char* flags);

int getFlagsInt(const char* flags);
int getColorInt(const char* colorStr);

void parseValueString(char *var, const char* line);
void setPluginsProperties(char *varName, char *varValue);
int detectPlugins();


int isConfigLoaded = 0;

static Configurator config;

//Configurator* loadConfig(const char* fileName, int* isDefault, int getDefault)
//loads color configuration:
//  1 - default colors
//  2 - black and white
//  3 - configuration file
int loadConfig_old(Configurator** newconfig, const char* fileName, int color_mode)
{
  initConfig(color_mode);
  *newconfig = &config;    
  
  int i=0;
  int foreground;
  int background;
  
  for (foreground=COLOR_BLACK; foreground<=COLOR_WHITE; foreground++)
    for (background=COLOR_BLACK; background<=COLOR_WHITE; background++)
      init_pair(i++, foreground, background);
 
  if (color_mode != 3)
  return 1;  	    
  
  FILE* hFile = fopen(fileName, "r");
  if (!hFile)
    return 0;
 
  static const int  MaxLineLength = 1024;
  char currentLine[MaxLineLength+1];
  char tmpLine[MaxLineLength+1];
  
  int lineLen;
  
  while (fgets(currentLine,MaxLineLength,hFile) != NULL)
    {
      lineLen = strlen(currentLine);
      if (lineLen>0)
        if (currentLine[0]!='#')
          {
            strcpy(tmpLine, currentLine);
            parseLine(tmpLine, lineLen);
          }
    }
  
  fclose(hFile);
  return 1;
}

// Valid variables in config file
static char  *validVarNames[] =
{ "Plugins.Dir",
  "Plugins.Names",
  "VerticalTableChar",             
  "HorizTableChar"
  "MemswapGraphChar"
  "FrameCurrentOperation"
  "ChartColor"
  "NodeCaption"
  "CopyrightsCaption"
  "Multicluster"
  "NodeNameGridMode",
  "VertNodeName",
  "HorizNodeName",
  "IdleCaption",
  "StringMB",
  "DeadCaption",
  "LoadCaption",
  "FrozenCaption",
  "UtilCaption",
  "NettotalCaption",
  "SpeedCaption",
  "SwapCaption",
  "MemswapCaption",
  "GridCaption",
  "reserveCaption",
  "UptimeStatus",
  "StatusNodeName",
  "StatusLetter",
  "BottomStatusbar",
  "CurrentLoad",
  NULL};


//loads color configuration:
//  1 - default colors
//  2 - black and white
//  3 - configuration file
int loadConfig(Configurator** newconfig, const char* fileName, int color_mode)
{
  initConfig(color_mode);
  *newconfig = &config;    
  
  int i=0;
  int foreground;
  int background;
  
  // Init all the combination of colors
  for (foreground=COLOR_BLACK; foreground<=COLOR_WHITE; foreground++)
    for (background=COLOR_BLACK; background<=COLOR_WHITE; background++)
      init_pair(i++, foreground, background);
 
  if (color_mode != 3)
  return 1;  	    
  
  conf_file_t cf;
  
  if(!(cf = cf_new(validVarNames))) {
    fprintf(stderr, "Error generating conf file object\n");
    return 0;
  }
  fprintf(stderr, "\n\rTrying to load [%s] ...   ", fileName);
  if(access(fileName, R_OK) != 0) {
    //fprintf(stderr, "Error: configuration files [%s] does not exists or is not readable\n", fileName);
    fprintf(stderr, "not found/not readable\n");
    return 0;
  }
  
  if(!cf_parseFile(cf, (char *)fileName)) {
       fprintf(stderr, "error parsing\n");
       return 0;
  }
  
  conf_var_t ent;
  int        intVal;

  char *var;
  for(i=0 ; validVarNames[i] ; i++) {
       if(cf_getVar(cf, validVarNames[i], &ent)) {
	    if(strcasecmp(validVarNames[i], "Plugins.Names") == 0 ||
	       strcasecmp(validVarNames[i], "Plugins.Dir") == 0) {
		 setPluginsProperties(validVarNames[i], ent.varValue);
	    }
	    else {
		 parseValueString(validVarNames[i], ent.varValue);
	    }
       }
  }

  
  fprintf(stderr, "ok\n");
  return 1;
}


//Configurator* loadConfigEx(const char* dirName, const char* fileName, int* isDefault, int getDefault)
int loadConfigEx(Configurator** newconfig, const char* dirName, const char* fileName)
{
  int temp =  (sizeof(char)*
               (strlen(dirName)+1+
                strlen(fileName)+1));
  char* fullName = (char*)malloc(temp);

  strcpy(fullName, dirName);
  strcat(fullName, "/");
  strcat(fullName, fileName);
  temp = loadConfig(newconfig, fullName, 3); //always use conf. file mode
  free(fullName);
  return temp;
}

void initConfig(int color_mode)
     //initiates the config data structure in BW or color.
{
    // External modules
    config.Plugins.pluginsDir = MMON_DEF_EXTERNAL_MODULE_DIR;
    config.Plugins.pluginsNum = 0;
    

    // Characters
    config.Appearance._verticalTableChar = '|';
    config.Appearance._horizTableChar = '-';
    config.Appearance._memswapGraphChar = '+';
    config.Appearance._seperatorChar = '\'';

  switch (color_mode)
    {
	 // Default colors
    case 1:

      config.Colors._frameCurrentOperation.color_pair = (COLOR_YELLOW*8) + COLOR_BLACK;
      config.Colors._frameCurrentOperation.attr_flags = A_BOLD;

      config.Colors._chartColor.color_pair = (COLOR_CYAN*8) + COLOR_BLACK;
      config.Colors._chartColor.attr_flags = A_BOLD;
      
      config.Colors._nodeCaption.color_pair = (COLOR_BLACK*8) + COLOR_GREEN;
      config.Colors._nodeCaption.attr_flags = A_BOLD;
      
      config.Colors._copyrightsCaption.color_pair = (COLOR_YELLOW*8) + COLOR_BLACK;
      config.Colors._copyrightsCaption.attr_flags = A_BOLD | A_UNDERLINE;

      config.Colors._multicluster.color_pair = (COLOR_BLACK*8) + COLOR_CYAN;
      config.Colors._multicluster.attr_flags = A_NORMAL;
      
      config.Colors._nodeNameGridMode.color_pair = (COLOR_BLACK*8) + COLOR_CYAN;
      config.Colors._nodeNameGridMode.attr_flags = A_NORMAL;
      
      config.Colors._vertNodeName.color_pair = (COLOR_MAGENTA*8) + COLOR_BLACK;
      config.Colors._vertNodeName.attr_flags = A_NORMAL;
    
      config.Colors._horizNodeName.color_pair = (COLOR_MAGENTA*8) + COLOR_BLACK;
      config.Colors._horizNodeName.attr_flags = A_BOLD;
      
      config.Colors._idleCaption.color_pair = (COLOR_GREEN*8) + COLOR_BLACK;
      config.Colors._idleCaption.attr_flags = A_NORMAL;
      
      config.Colors._stringMB.color_pair = (COLOR_BLACK*8) + COLOR_YELLOW;
      config.Colors._stringMB.attr_flags = A_NORMAL;
      
      config.Colors._deadCaption.color_pair = (COLOR_RED*8) + COLOR_BLACK;
      config.Colors._deadCaption.attr_flags = A_BOLD;

      config.Colors._loadCaption.color_pair = (COLOR_MAGENTA*8) + COLOR_BLACK;
      config.Colors._loadCaption.attr_flags = A_NORMAL;
      
      config.Colors._frozenCaption.color_pair = (COLOR_BLUE*8) + COLOR_BLACK;
      config.Colors._frozenCaption.attr_flags = A_NORMAL;
      
      config.Colors._utilCaption.color_pair = (COLOR_MAGENTA*8) + COLOR_BLACK;
      config.Colors._utilCaption.attr_flags = A_NORMAL;

      config.Colors._nettotalCaption.color_pair = (COLOR_BLACK*8) + COLOR_GREEN;
      config.Colors._nettotalCaption.attr_flags = A_NORMAL;

      config.Colors._speedCaption.color_pair = (COLOR_BLACK*8) + COLOR_YELLOW;
      config.Colors._speedCaption.attr_flags = A_NORMAL;
 
      config.Colors._swapCaption.color_pair = (COLOR_BLUE*8) + COLOR_BLACK;
      config.Colors._swapCaption.attr_flags = A_BOLD;

      config.Colors._memswapCaption.color_pair = (COLOR_BLUE*8) + COLOR_BLACK;
      config.Colors._memswapCaption.attr_flags = A_BOLD;

      config.Colors._gridCaption.color_pair = (COLOR_WHITE*8) + COLOR_BLUE;
      config.Colors._gridCaption.attr_flags = A_DIM;

      config.Colors._reserveCaption.color_pair = (COLOR_WHITE*8) + COLOR_BLUE;
      config.Colors._reserveCaption.attr_flags = A_DIM;

      config.Colors._uptimeStatus.color_pair = (COLOR_GREEN*8) + COLOR_BLACK;
      config.Colors._uptimeStatus.attr_flags = A_BOLD;

      config.Colors._statusNodeName.color_pair = (COLOR_GREEN*8) + COLOR_BLACK;
      config.Colors._statusNodeName.attr_flags = A_BOLD;

      config.Colors._statusLetter.color_pair = (COLOR_RED*8) + COLOR_BLACK;
      config.Colors._statusLetter.attr_flags = A_BOLD;
      
      config.Colors._bottomStatusbar.color_pair = (COLOR_GREEN*8) + COLOR_BLACK;
      config.Colors._bottomStatusbar.attr_flags = A_BOLD;

      config.Colors._currentLoad.color_pair = (COLOR_GREEN*8) + COLOR_BLACK;
      config.Colors._currentLoad.attr_flags = A_BOLD | A_UNDERLINE;

      config.Colors._errorStr.color_pair = (COLOR_RED*8) + COLOR_BLACK;
      config.Colors._errorStr.attr_flags = A_NORMAL;

      config.Colors._fairShareInfo.color_pair = (COLOR_YELLOW*8) + COLOR_BLACK;
      config.Colors._fairShareInfo.attr_flags = A_NORMAL;

      config.Colors._seperatorBar.color_pair = (COLOR_GREEN*8) + COLOR_BLACK;
      config.Colors._seperatorBar.attr_flags = A_NORMAL;


      break;

      // Black and White
    case 2:

      config.Colors._frameCurrentOperation.color_pair = -1;
      config.Colors._frameCurrentOperation.attr_flags = 0;

      config.Colors._chartColor.color_pair = -1;
      config.Colors._chartColor.attr_flags = 0;
      
      config.Colors._nodeCaption.color_pair = -1;
      config.Colors._nodeCaption.attr_flags = 0;
      
      config.Colors._copyrightsCaption.color_pair = -1;
      config.Colors._copyrightsCaption.attr_flags = 0;

      config.Colors._multicluster.color_pair = -1;
      config.Colors._multicluster.attr_flags = 0;
      
      config.Colors._nodeNameGridMode.color_pair = -1;
      config.Colors._nodeNameGridMode.attr_flags = 0;
      
      config.Colors._vertNodeName.color_pair = -1;
      config.Colors._vertNodeName.attr_flags = 0;
    
      config.Colors._horizNodeName.color_pair = -1;
      config.Colors._horizNodeName.attr_flags = 0;
      
      config.Colors._idleCaption.color_pair = -1;
      config.Colors._idleCaption.attr_flags = 0;
      
      config.Colors._stringMB.color_pair = -1;
      config.Colors._stringMB.attr_flags = 0;
      
      config.Colors._deadCaption.color_pair = -1;
      config.Colors._deadCaption.attr_flags = 0;

      config.Colors._loadCaption.color_pair = -1;
      config.Colors._loadCaption.attr_flags = 0;
      
      config.Colors._frozenCaption.color_pair = -1;
      config.Colors._frozenCaption.attr_flags = 0;
      
      config.Colors._utilCaption.color_pair = -1;
      config.Colors._utilCaption.attr_flags = 0;

      config.Colors._nettotalCaption.color_pair = -1;
      config.Colors._nettotalCaption.attr_flags = 0;

      config.Colors._speedCaption.color_pair = -1;
      config.Colors._speedCaption.attr_flags = 0;
 
      config.Colors._swapCaption.color_pair = -1;
      config.Colors._swapCaption.attr_flags = 0;

      config.Colors._memswapCaption.color_pair = -1;
      config.Colors._memswapCaption.attr_flags = 0;

      config.Colors._gridCaption.color_pair = -1;
      config.Colors._gridCaption.attr_flags = 0;

      config.Colors._reserveCaption.color_pair = -1;
      config.Colors._reserveCaption.attr_flags = 0;

      config.Colors._uptimeStatus.color_pair = -1;
      config.Colors._uptimeStatus.attr_flags = 0;

      config.Colors._statusNodeName.color_pair = -1;
      config.Colors._statusNodeName.attr_flags = 0;

      config.Colors._statusLetter.color_pair = -1;
      config.Colors._statusLetter.attr_flags = 0;
      
      config.Colors._bottomStatusbar.color_pair = -1;
      config.Colors._bottomStatusbar.attr_flags = 0;

      config.Colors._currentLoad.color_pair = -1;
      config.Colors._currentLoad.attr_flags = 0;

      config.Colors._errorStr.color_pair = -1;
      config.Colors._errorStr.attr_flags = 0;

      config.Colors._fairShareInfo.color_pair = -1;
      config.Colors._fairShareInfo.attr_flags = 0;

      config.Colors._seperatorBar.color_pair = -1;
      config.Colors._seperatorBar.attr_flags = 0;

      break;
      
    default:
      break;
    }
}



void parseLine(const char* line, int len)
{

  char* tok = NULL;
  char* var = NULL;
  char* foregoundColor = NULL;
  char* backgroundColor = NULL;
  char* flags= NULL;
     
  tok = (char*)strtok((char*)line," ");
  if (tok == NULL)
    return;

  var = tok;
  tok = (char*)strtok(NULL," ");
  if (tok == NULL)
    return;
      
      
  foregoundColor = tok;
  tok = (char*)strtok(NULL," ");
  if (tok == NULL)
  {
       parseConfigParams(var,foregoundColor,"BLACK","");
       return;
  }
      
  backgroundColor = tok;
  tok = (char*)strtok(NULL," ");
  if (tok == NULL)
    {
      parseConfigParams(var,foregoundColor, backgroundColor ,"");
      return;
    }

  flags = tok;
  parseConfigParams(var,foregoundColor, backgroundColor, flags);
}

// Parsing a value string of the form VAL1,VAL1,VAL3 and calling 
// the parseConfigParams 
void parseValueString(char *var, const char* line)
{

  char* tok = NULL;
  char* foregoundColor = NULL;
  char* backgroundColor = NULL;
  char* flags= NULL;

  
  tok = (char*)strtok((char *)line,",");
  if (tok == NULL)
    return;
  foregoundColor = tok;

  tok = (char*)strtok(NULL,",");
  if (tok == NULL)
  {
       parseConfigParams(var,foregoundColor,"BLACK","");
       return;
  }
      
  backgroundColor = tok;
  tok = (char*)strtok(NULL,",");
  if (tok == NULL)
    {
      parseConfigParams(var,foregoundColor, backgroundColor ,"");
      return;
    }

  flags = tok;
  parseConfigParams(var,foregoundColor, backgroundColor, flags);
}


void parseConfigParams(const char* var,
                       const char* param2,
                       const char* param3,
                       const char* param4)
{
  int frColor;
  int bkColor;
  int pairIndex;
  int flags;

  if (dbg_flg) fprintf(dbg_fp, "Parsing [%s]  [%s] [%s] [%s]\n", var, param2, param3, param4);


  if (strcasecmp(var, "VerticalTableChar") == 0)
    {
      if (strlen(param2) > 0)
        {
          config.Appearance._verticalTableChar = param2[0];
        }
      return;
    }

  if (strcasecmp(var, "HorizTableChar") == 0)
    {
	    
      if (strlen(param2) > 0)
        {
          config.Appearance._horizTableChar = param2[0];
        }
      return; 
    }

  if (strcasecmp(var, "MemswapGraphChar") == 0)
    {
      if (strlen(param2) > 0)
        {
          config.Appearance._memswapGraphChar = param2[0];
        }
      return; 
    }

  frColor = getColorInt(param2);
  if (frColor==-1)
    frColor = COLOR_WHITE;

  bkColor = getColorInt(param3);
  if (bkColor == -1)
    bkColor = COLOR_BLACK;
	    
  pairIndex = (frColor*8) + bkColor; 
  flags   = getFlagsInt(param4);
    
  if (strcasecmp(var, "FrameCurrentOperation") == 0)
    {
      config.Colors._frameCurrentOperation.color_pair = pairIndex;
      config.Colors._frameCurrentOperation.attr_flags = flags;
      return;
    }

  if (strcasecmp(var, "ChartColor") == 0)
    {
      config.Colors._chartColor.color_pair = pairIndex;
      config.Colors._chartColor.attr_flags = flags;
      return;
    }

  if (strcasecmp(var, "NodeCaption") == 0)
    {
      config.Colors._nodeCaption.color_pair = pairIndex;
      config.Colors._nodeCaption.attr_flags = flags;
      return;
    }

  if (strcasecmp(var, "CopyrightsCaption") == 0)
    {
      config.Colors._copyrightsCaption.color_pair = pairIndex;
      config.Colors._copyrightsCaption.attr_flags = flags;
      return; 
    }
      
  if (strcasecmp(var, "Multicluster") == 0)
    {   
      config.Colors._multicluster.color_pair = pairIndex;
      config.Colors._multicluster.attr_flags = flags;
      return;  
    }  

  if (strcasecmp(var, "NodeNameGridMode") == 0)
    {
      config.Colors._nodeNameGridMode.color_pair = pairIndex;
      config.Colors._nodeNameGridMode.attr_flags = flags;
      return;   
    }
      
  if (strcasecmp(var, "VertNodeName") == 0)
    {
      config.Colors._vertNodeName.color_pair = pairIndex;
      config.Colors._vertNodeName.attr_flags = flags;
      return;    
    }

  if (strcasecmp(var, "HorizNodeName") == 0)
    {
      config.Colors._horizNodeName.color_pair = pairIndex;
      config.Colors._horizNodeName.attr_flags = flags;
      return; 
    }
      
  if (strcasecmp(var, "IdleCaption") == 0)
    { 
      config.Colors._idleCaption.color_pair = pairIndex;
      config.Colors._idleCaption.attr_flags = flags;
      return; 	    
    }

  if (strcasecmp(var, "StringMB") == 0)
    {
      config.Colors._stringMB.color_pair = pairIndex;
      config.Colors._stringMB.attr_flags = flags;
      return; 
    }

  if (strcasecmp(var, "DeadCaption") == 0)
    {
      config.Colors._deadCaption.color_pair = pairIndex;
      config.Colors._deadCaption.attr_flags = flags;
      return;     
    }

  if (strcasecmp(var, "LoadCaption") == 0)
    {
      config.Colors._loadCaption.color_pair = pairIndex;
      config.Colors._loadCaption.attr_flags = flags;
      return;  
    }

  if (strcasecmp(var, "FrozenCaption") == 0)
    {
      config.Colors._frozenCaption.color_pair = pairIndex;
      config.Colors._frozenCaption.attr_flags = flags;
      return;  
    }

   
  if (strcasecmp(var, "UtilCaption") == 0)
    {
      config.Colors._utilCaption.color_pair = pairIndex;
      config.Colors._utilCaption.attr_flags = flags;
      return;  
    }
      
  if (strcasecmp(var, "NettotalCaption") == 0)
    {
      config.Colors._nettotalCaption.color_pair = pairIndex;
      config.Colors._nettotalCaption.attr_flags = flags;
      return;   
    }
      
  if (strcasecmp(var, "SpeedCaption") == 0)
    {
      config.Colors._speedCaption.color_pair = pairIndex;
      config.Colors._speedCaption.attr_flags = flags;
      return; 
    }

  if (strcasecmp(var, "SwapCaption") == 0)
    {
      config.Colors._swapCaption.color_pair = pairIndex;
      config.Colors._swapCaption.attr_flags = flags;
      return;  
    }

  if (strcasecmp(var, "MemswapCaption") == 0)
    {
      config.Colors._memswapCaption.color_pair = pairIndex;
      config.Colors._memswapCaption.attr_flags = flags;
      return;  
    }

  if (strcasecmp(var, "GridCaption") == 0)
    {
      config.Colors._gridCaption.color_pair = pairIndex;
      config.Colors._gridCaption.attr_flags = flags;
      return;   	    
    }

  if (strcasecmp(var, "reserveCaption") == 0)
    {
      config.Colors._reserveCaption.color_pair = pairIndex;
      config.Colors._reserveCaption.attr_flags = flags;
      return;  
    }

  if (strcasecmp(var, "UptimeStatus") == 0)
    {
      config.Colors._uptimeStatus.color_pair = pairIndex;
      config.Colors._uptimeStatus.attr_flags = flags;
      return;  
    }

  if (strcasecmp(var, "StatusNodeName") == 0)
    {
      config.Colors._statusNodeName.color_pair = pairIndex;
      config.Colors._statusNodeName.attr_flags = flags;
      return;  
    }
      
  if (strcasecmp(var, "StatusLetter") == 0)
    {
      config.Colors._statusLetter.color_pair = pairIndex;
      config.Colors._statusLetter.attr_flags = flags;
      return; 
    }

  if (strcasecmp(var, "BottomStatusbar") == 0)
    {
      config.Colors._bottomStatusbar.color_pair = pairIndex;
      config.Colors._bottomStatusbar.attr_flags = flags;
      return;  
    }

  if (strcasecmp(var, "CurrentLoad") == 0)
    {
      config.Colors._currentLoad.color_pair = pairIndex;
      config.Colors._currentLoad.attr_flags = flags;
      return;  
    }

  if (strcasecmp(var, "ErrorStr") == 0)
    {
      config.Colors._errorStr.color_pair = pairIndex;
      config.Colors._errorStr.attr_flags = flags;
      return;  
    }

  if (strcasecmp(var, "FairShare") == 0)
    {
      config.Colors._fairShareInfo.color_pair = pairIndex;
      config.Colors._fairShareInfo.attr_flags = flags;
      return;  
    }  

  if (strcasecmp(var, "SeperatorBar") == 0)
    {
      config.Colors._seperatorBar.color_pair = pairIndex;
      config.Colors._seperatorBar.attr_flags = flags;
      return;  
    }  

}

int getColorInt(const char* colorStr)
{
  if (strcasecmp(colorStr,"BLACK")==0)
    return COLOR_BLACK;

  if (strcasecmp(colorStr,"RED")==0)
    return COLOR_RED;

  if (strcasecmp(colorStr,"GREEN")==0)
    return COLOR_GREEN;

  if (strcasecmp(colorStr,"YELLOW")==0)
    return COLOR_YELLOW;

  if (strcasecmp(colorStr,"BLUE")==0)
    return COLOR_BLUE;

  if (strcasecmp(colorStr,"MAGENTA")==0)
    return COLOR_MAGENTA;

  if (strcasecmp(colorStr,"CYAN")==0)
    return COLOR_CYAN;

  if (strcasecmp(colorStr,"WHITE")==0)
    return COLOR_WHITE;

  return -1;
      
}

int getFlagsInt(const char* flags)
{
  int retFlags = 0;

  if (strchr(flags,'N')!=NULL)
    retFlags |= A_NORMAL;

  if (strchr(flags,'S')!=NULL)
    retFlags |= A_STANDOUT;  

  if (strchr(flags,'U')!=NULL)
    retFlags |= A_UNDERLINE;

  if (strchr(flags,'R')!=NULL)	  
    retFlags |= A_REVERSE;

  if (strchr(flags,'B')!=NULL)
    retFlags |= A_BLINK;

  if (strchr(flags,'D')!=NULL)
    retFlags |= A_DIM;  

  if (strchr(flags,'O')!=NULL)
    retFlags |= A_BOLD;

  if (strchr(flags,'P')!=NULL)	  
    retFlags |= A_PROTECT;

  if (strchr(flags,'I')!=NULL)	  
    retFlags |= A_INVIS;

  return retFlags;
}

void setPluginsProperties(char *varName, char *varValue) {

    if (strcasecmp(varName, "Plugins.Names") == 0) {
        int moduleNum;
        if((moduleNum = split_str_alloc(varValue, &(config.Plugins.pluginsList), ", ")) > 0) {
            config.Plugins.pluginsNum = moduleNum;
        }
    }
    else if (strcasecmp(varName, "Plugins.Dir") == 0) {
        config.Plugins.pluginsDir = strdup(varValue);
    }
}

int detectPlugins() {
    printf("Automatically detecting plugins in: %s\n", config.Plugins.pluginsDir);
}