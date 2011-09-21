/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2011 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

/******************************************************************************
 *
 * Author(s): Amar Lior
 *
 *****************************************************************************/
#include <mmon.h>
#include <host_list.h>

int parseNodesList(mmon_data_t *md);

int parseCommandLine(mmon_data_t *md, int argc, char **argv, mon_disp_prop_t *prop)
{
    int c;
    //struct passwd *pw;

    while (1) {
        int option_index = 0;

        static struct option long_options[] = {
            {"defaults", 0, 0, 0},
            {"bw", 0, 0, 0},
            {"conf", 1, 0, 0},
            {"debug-mode", 1, 0, 0},
            {"win", 1, 0, 0},
            {"winfile", 1, 0, 0},
            {"fresh", 0, 0, 0},
            {"skeep-winfile", 0, 0, 0},
            {"help", 0, 0, 0},
            {"copyright", 0, 0, 0},
            {"list-modules", 0, 0, 0},
            {"nodes", 1, 0, 0},
            //{"uid",           1, 0, 0},
            //{"username",      1, 0, 0},

            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "VvWwTtdmsluf:c:h:o:",
                long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
        case 0:
            // Long options
            if (strcmp(long_options[option_index].name, "help") == 0) {
                printUsage(stdout);
                exit(0);
            }
            if (strcmp(long_options[option_index].name, "copyright") == 0) {
                printCopyright(stdout);
                exit(0);
            }

            if (strcmp(long_options[option_index].name, "nodes") == 0) {
                 md->nodesArgStr = strdup(optarg);
                 if(!parseNodesList(md)) {
                      fprintf(stderr, "Error: nodes list is not in correct format\n");
                      exit(1);
                 }
                 md->filterNodesByName = 1;
                 displaySetNodesToDisplay(prop, &md->hostList);
            }


            if (strcmp(long_options[option_index].name, "list-modules") == 0) {
                listInfoDisplays(stdout);
                exit(0);
            }

            if (strcmp(long_options[option_index].name, "defaults") == 0) {
                md->colorMode = 1;
            }
            if (strcmp(long_options[option_index].name, "bw") == 0) {
                md->colorMode = 2;
            }
            if (strcmp(long_options[option_index].name, "conf") == 0) {
                 if ((strlen(optarg) < MAX_CONF_FILE_LEN)) {
                      strcpy(md->confFileName, optarg);
                 }
                 md->colorMode = 3;
            }
            if (strcmp(long_options[option_index].name, "win") == 0) {
                 int i = 0;
                 while (md->startWinStrArr[i] && i < MAX_START_WIN) {
                      i++;
                 };
                 if (i == MAX_START_WIN) {
                      fprintf(stderr, "Error: can not support more than %d --startwin args\n",
                              MAX_START_WIN);
                    exit(1);
                 }
                 md->startWinStrArr[i] = strdup(optarg);
                 md->startWinFromCmdline = 1;
                 printf("Got startwin: optarg \n");
            }

            // Window save file
            if (strcmp(long_options[option_index].name, "winfile") == 0) {
                md->winSaveFile = strdup(optarg);
            }

            // Skeeping window save file
            if (strcmp(long_options[option_index].name, "fresh") == 0 ||
                    strcmp(long_options[option_index].name, "skeep-winfile") == 0) {
                md->skeepWinfile = 1;
            }

            if (strcmp(long_options[option_index].name, "debug-mode") == 0) {
                char *debugItems[51];
                int maxItems = 50, items = 0;

                items = split_str(optarg, debugItems, maxItems, ",");
                if (items < 1)
                    return 1;

                printf("Handling debug modes:\n");

                for (int i = 0; i < items; i++) {
                    printf("Got debug mode [%s]\n", debugItems[i]);
                    mlog_setModuleLevelFromStr(debugItems[i], LOG_DEBUG);
                }
            }
            break;

            // FIXME see what this uid stuff is (lior 20/5/2009)
            //case 5:
            //current_set.uid = atoi(optarg);
            //if (dbg_flg) fprintf(dbg_fp,"uid %i selected.\n", current_set.uid);
            //break;

            //case 6:
            //pw = getpwnam(optarg);
            //if (pw)
            //  current_set.uid = pw->pw_uid;
            //break;

        case 'V':
            prop->wmode = 1;
            break;

        case 'v':
            prop->wmode = 2;
            break;

        case 'W':
        case 'w':
            prop->wmode = 3;
            break;

        case 'T':
        case 't':
            prop->show_status = 1 - prop->show_status;
            break;

        case 'd':
            prop->show_dead = 1 - prop->show_dead;
            break;

        case 'm':
            //overrun previous display
            if ((prop->legend).legend_size > 0)
                toggle_disp_type(prop, ((prop->legend).head)->data_type, 0);
            toggle_disp_type(prop, dm_getIdByName("mem"), 1);
            break;

        case 's':
            //overrun previous display
            if ((prop->legend).legend_size > 0)
                toggle_disp_type(prop, ((prop->legend).head)->data_type, 0);
            toggle_disp_type(prop, dm_getIdByName("speed"), 1);
            break;

        case 'l':
            //overrun previous display
            if ((prop->legend).legend_size > 0)
                toggle_disp_type(prop, ((prop->legend).head)->data_type, 0);
            toggle_disp_type(prop, dm_getIdByName("load"), 1);
            break;

        case 'u':
            //overrun previous display
            if ((prop->legend).legend_size > 0)
                toggle_disp_type(prop, ((prop->legend).head)->data_type, 0);
            toggle_disp_type(prop, dm_getIdByName("util"), 1);
            break;

        case 'f':
            //read a cluster file
            cl_free(prop->clist);
            if (prop->last_host)
                free(prop->last_host);
            prop->last_host = strdup(optarg);
            prop->need_dest = 1 -
                    cl_set_from_file(prop->clist, optarg);
            init_clusters_list(prop);
            break;

        case 'c':
            //read a cluster list
            cl_free(prop->clist);
            if (prop->last_host)
                free(prop->last_host);
            prop->last_host = strdup(optarg);
            prop->need_dest = 1 -
                    cl_set_from_str(prop->clist, optarg);
            init_clusters_list(prop);
            break;

        case 'h':
            //read a single host
            displaySetHost(prop, optarg);
            break;

        case 'o':
            //output debug printouts
            // FIXME use level debugging instead
            dbg_flg2 = 0;
            dbg_flg = 0;
            //dbg_fp = fopen(optarg, "w");
            printf("dbg file is %s\n", optarg);
            //fprintf(dbg_fp, "\nDebug started.\n");
            mlog_setLevel(LOG_DEBUG);
            mlog_addOutputFile(optarg, 1);
            break;


        default:
            printUsage(stdout);
            exit(0);
            terminate();
        }
    }

    if (optind < argc) {
        printUsage(stdout);
        terminate();
        exit(0);
    }

    //if no cluster list...
    if (cl_size(prop->clist) == 0) {
        cl_free(prop->clist);
        free(prop->clist);
        prop->clist = NULL;
    } else {
        prop->cluster = cl_curr(prop->clist);
    }
    return 1;
}


int parseNodesList(mmon_data_t *md) {
     printf("Parsing Nodes: %s\n", md->nodesArgStr);
     
     int res;
     
     mh_init(&md->hostList);
     
     res = mh_set(&md->hostList, md->nodesArgStr);
     if(!res) {
          return 0;
     }
     mh_print(&md->hostList);
     return 1;
}
