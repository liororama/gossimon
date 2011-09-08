/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include "mmon.h"


char* head_str[] = 
  {
    "",
    " WELCOME TO MOSIX MONitor (MMON) ",
    "",
    " BASIC OPERATIONS",    
    " -multiple options - type the same letter.",
    "",
    (char*)NULL, //must be last value of this array...
  };

char* help_str[] = 
  {
    "",
    " MONITOR CONTROL",    
    "",
    " h - Help (this menu, type 'h' to return). ",
    " n - Open cluster dialog screen. ",
    " y - Yardstick selection panel. ",
    " t - Display number of active nodes and CPUs. ",
    " w - Split the monitoring window. ",
    " e - Close a monitoring window. ",
    " Tab - Rotate between windows.",
    " UP/DOWN key - switch between clusters in a window.",
    " Left/Right arrow to shift the display. ",
    " PageUp/Down for faster shift. ",
    " d - Dispaly dead nodes. ",
    " q - Quit with confirmation.",
    " Q - Quit without confirmation",
    " S - Save curret windows setup",
    "",
    " ADVANCED",
    "",
    " z - Toggle side window (Node Info, Info Legend, Statistics)",
    "  ",
    " Node-Info mode: ",
    " C - Select node to view (only in Node-Info mode)",
    " [ - Decrease selected node to view (only in Node-Info mode)",
    " ] - Increase selected node to view (only in Node-Iinfo mode)",
    " ",
    " Info Legend mode: ",
    " In the Info legegnd mode, any combination of display types ",
    " can be displayed simultaniously.",
    " The display type key binding remain the same, ",
    " the space bar can be used to pad with spaces ",
    " and the backspace - to remove last (rightmost) type. ",
    " ",
    " Statistics Mode:",
    " Show statistics on all cluster nodes",
    "",
    (char*)NULL, //must be last value of this array...
  };

//The usage string is printed upon incorrect paramater(s)
char* usage_str = 
  "mmon a real time monitor for clusters. mmon gets information from\n"
  "gossimon daemon in local or remote nodes.\n"
  "\n"
  "mmon version " GOSSIMON_VERSION_STR "\n"
  "\n"
  "USAGE: mmon [-w|-v|-V] [-d] [-t] [-h <host>| -c <host list>| -f <filename>]\n"
  "            [-l|-m|-u|-s]\n"
  "            [--defaults|--conf <filename>] \n"
  "            [--winfile <file>]\n"
  //  "            [--uid <uid>|--username <name>]\n"
  "            [--nodes n1..N] \n"
  "            [--debug-mode m1,m2,...] [-o <stream|filename>]"
  "\n"
  "OPTIONS\n"
  "\n"
  " -w                Makes the machine tabs wide enough for its number to fit.\n"
  " -v                Makes the machine tabs 2 charachters in width.\n"
  " -V                Makes the machine tabs 1 charachter wide.\n"
  "\n"
  " -h <host>         Draws displayed data from Infod on given host.\n"
  " -c <host list>    Draws displayed data from Infods on given hosts.\n"
  " -f <filename>     Draws displayed data from Infods on hosts given in the file.\n"
  "\n"
  " -l                Starts with the load display of the cluster.\n"
  " -m                Starts with the memory usage display of the cluster.\n"
  " -u                Starts with the utilization display of the cluster.\n"
  " -s                Starts with the status display of the cluster.\n"
  "\n"
  " --defaults        Use default color configuration.\n"
  " --bw              Use default black and white configuration.\n"
  " --conf <filename> Use color configuration given in the file.\n"
  "\n"
  " --win WIN_CONF    Configure the windows according to the given configuration\n"
  "                   string. To see all options use the\n"
  " --winfile <file>  Load the windows configurations from the given file\n"
  "\n"
  " --fresh\n"
  " --skeep-winfile   Do not load the default windows setup file\n"
  "\n"
  " --debug-mode m1,m2,..  \n"
  "                   Set debug modes\n"
  "  -o FILE          Set output debug file to FILE (implies debug mode)\n"
  " --help            Show this help screen\n"
  " --copyright       Show copyright message\n"
  " --list-modules    List the name of information modules supported.\n"
  "                   Those name can be used in the --win option to specify\n"
  "                   the desired disply using disp=mod1,mod2,...\n"
  "\n"
  " --nodes RANGE     Specify a node range to display on screen. RANGE is in the\n"
  "                   format of n1..N (e.g. rusty1..30). It can include mutliple\n"
  "                   ranges seperated by commas.\n"

  //  " --uid <uid>       Show machines occupied by this user, by id.\n"
  //" --username <name> Show machines occupied by this user, by name.\n"
  "\n"
  "Examples:\n"
  "\n"
  "> mmon -h mos1\n"
  "\n"
  "> mmon --win \"host=mos2 disp=mem,space,load mode=dead,wide\n"
  "       --win \"host=mos3 disp=swap,rio  mode=dead,wide\n"
  "\n"
  " Note: press 'h' at any time to toggle help screen.\n";
 


void printUsage(FILE *fp) {
  fprintf(fp,"%s", usage_str);
}

void printCopyright(FILE *fp) {
  fprintf(fp, "mmon version " GOSSIMON_VERSION_STR "\n" GOSSIMON_COPYRIGHT_STR "\n\n");
}
