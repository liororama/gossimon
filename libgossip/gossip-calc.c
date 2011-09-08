/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

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

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include "parse_helper.h"

#include "gossip.h"

typedef enum {
     OP_NONE = 0,
     OP_ALL,
     OP_CDF,
     OP_WIN_AV,
     OP_WIN_MAX,
     OP_WIN_UPTOAGE,
     
} opType_t;

typedef struct {
     int      nArr[250];
     int      nArrSize;

     double   tArr[250];
     int      tArrSize;

     int      wArr[250];
     int      wArrSize;

     opType_t op;
     double   age;
     int      uptoageEntries;
     char verbose;
     
} gossipData_t;

char *progName;

void usage() {
     
     printf("Usage: %s [OPTIONS]\n"
            "Calculating parameters for infod gossip protocol\n"
            "\n"
            "Options:\n"
            "-n      size1, size2  A comma separated list of possible cluster sizes\n"
            "-t      t1,t2,..      A comma separated list of possible T parameters\n"
            "-w      w1,w1,..      A comma separated list of possible window sizes\n"
            "-a                    Show all calculated information relative to the \n"
            "                      (n,t) or (n,w), this is the default. The calculated\n"
            "                      information includes the average age maximal age...\n"
            "   --cdf MAX-AGE      Show how many entries in the vector are going to be\n"
            "                      below age 1...max-age.\n"
            "\n"
            "   --avgge AGE        Calculate the window size that will produce the given\n"
            "                      average age\n"
            "   --avgmax AGE       Calculate the window size that will produce the given\n"
            "                      average maximal age\n"
            "   --uptoage NUM,AGE  Calculate the window size that will produce NUM entries\n"
            "                      in the vector with age upto AGE    \n"
            "\n"
            "-h, --help            Show this help screen\n"
            , progName);
}

int getIntStrList(int *arr, int *size, char *str) {

     char *items[250];
     int   num = 250;

     num = split_str(str, items, num, ",");
     // Empty lines are ignored
     if(num <= 0) return 0;
     for(int i=0 ; i< num ; i++) {
          arr[i] = atoi(items[i]);
          (*size)++;
          //printf("Item: %d\n", arr[i]);
     }
     
     for(int i=0 ; i < num ; i++) {
          free(items[i]);
          items[i] = NULL;
     }
     
     return *size;
}

int getFloatStrList(double *arr, int *size, char *str) {

     char *items[250];
     int   num = 250;

     num = split_str(str, items, num, ",");
     // Empty lines are ignored
     if(num <= 0) return 0;
     for(int i=0 ; i< num ; i++) {
          arr[i] = atof(items[i]);
          (*size)++;
          //printf("Item: %f\n", arr[i]);
     }
     
     for(int i=0 ; i < num ; i++) {
          free(items[i]);
          items[i] = NULL;
     }
     
     return *size;
}


int parseArgs(gossipData_t *gd, int argc, char **argv) {
     
     int c;
     
     while( 1 ) {
          
          //int this_option_optind = optind ? optind : 1;
          int option_index = 0;
          
          static struct option long_options[] = {
               {"cdf",         1, 0, 0},
               {"avgage",      1, 0, 0},
               {"avgmax",      1, 0, 0},
               {"uptoage",     1, 0, 0},
               {"help",        0, 0, 'h' },
               {0, 0, 0, 0}
          };
          
          c = getopt_long (argc, argv, "n:t:w:ah",
                           long_options, &option_index);
          if (c == -1)
               break;
          
          switch (c) {
              case 0:
                   if(strcmp(long_options[option_index].name, "cdf") == 0) {
                        gd->op = OP_CDF;
                        gd->age = atof(optarg);
                   }
                   if(strcmp(long_options[option_index].name, "avgage") == 0) {
                        gd->op = OP_WIN_AV;
                        gd->age = atof(optarg);
                   }
                   if(strcmp(long_options[option_index].name, "avgmax") == 0) {
                        gd->op = OP_WIN_MAX;
                        gd->age = atof(optarg);
                   }
                   if(strcmp(long_options[option_index].name, "uptoage") == 0) {
                        gd->op = OP_WIN_UPTOAGE;
                        float age;
                        if(sscanf(optarg, "%d,%f", &gd->uptoageEntries, &age ) != 2) {
                             fprintf(stderr, "Error!!! should supply age,entries\n");
                             exit(0);
                        }
                        gd->age = age;
                        printf("Age %f E %d\n", gd->age, gd->uptoageEntries);
                   }
                   break;
                   
              case 'n':
                   if(!getIntStrList(gd->nArr, &gd->nArrSize, optarg)) {
                        fprintf(stderr, "Error parsing cluster sizes list (-n)\n");
                        exit(0);
                   }
                   //for(int i=0 ; i< gd->nArrSize ; i++)
                   //    printf("N %d %d\n", i, gd->nArr[i]);
                   break;
              case 't':
                   if(!getFloatStrList(gd->tArr, &gd->tArrSize, optarg)) {
                        fprintf(stderr, "Error parsing T list (-t)\n");
                        exit(0);
                   }
                   break;

              case 'w':
                   if(!getIntStrList(gd->wArr, &gd->wArrSize, optarg)) {
                        fprintf(stderr, "Error parsing window sizes list (-w)\n");
                        exit(0);
                   }
                   break;
              case 'v':
                   gd->verbose = 1;
                   break;
              case 'h':
              case '?':
                   usage();
                   exit(0);
                   break;
                   
              default:
                   printf ("Getopt returned character code 0%o??\n",
                           c);
          }
     }
     
     if( optind < argc ) {
          printf ("non-option ARGV-elements: ");
          while( optind < argc )
               printf ("%s ", argv[optind++]);
          printf ("\n");
          usage();
          exit( 1 );
     }
     return 1;
}

int printAllInfo(gossipData_t *gd) {
     if(!gd->nArrSize) {
          fprintf(stderr, "Must supply cluster sizes\n");
          return 0;
     }

     if(!gd->tArrSize && !gd->wArrSize) {
          fprintf(stderr, "Must supply either T list or W list\n");
          return 0;
     }
     
     for(int nIndex = 0 ; nIndex < gd->nArrSize ; nIndex++) {
          int n = gd->nArr[nIndex];
          printf("Cluster size: %d\n", n);
          if(gd->tArrSize) {
               for(int i=0 ; i < gd->tArrSize ; i++) {
                    double T = gd->tArr[i];
                    double Xt   = calcXT(n, T);
                    double Av   = calcAv(n, T);
                    double Max  = calcMaxAge(n, T);
                    int    w    = calcWinsizeGivenAv(n, Av);
                    int    wMax = calcWinsizeGivenMax(n, Max);
                    printf("N=%-5d T=%-5.2f  Xt= %-8.3f  AV= %-8.3f Max= %-8.3f w= %-4d wMax= %-4d\n",
                           n, T, Xt, Av, Max, w, wMax);
               }
          }
          else if(gd->wArrSize) {
               for(int i=0 ; i < gd->wArrSize ; i++) {
                    int    w = gd->wArr[i];
                    if(w >= n) {
                         printf("Error, w >= n  (%d >= %d)\n", w, n);
                         continue;
                    }
                    double T = calcTGivenW(n, w);
                    double Xt   = calcXT(n, T);
                    double Av   = calcAv(n, T);
                    double Max  = calcMaxAge(n, T);
                    int    wAv  = calcWinsizeGivenAv(n, Av);
                    int    wMax = calcWinsizeGivenMax(n, Max);
                    printf("N=%-5d w= %3d T=%-5.2f  Xt= %-8.3f  AV= %-8.3f Max= %-8.3f w= %-4d wMax= %-4d\n",
                           n, w, T, Xt, Av, Max, wAv, wMax);
               }
          }
     }

     return 1;
}

int printCdf(gossipData_t *gd) {

     // When printing the cdf using only the first n and the first t or w
     int n = gd->nArr[0];

     double T;
     if(gd->tArrSize) {
          T = gd->tArr[0];
     } else if(gd->wArrSize) {
          T = calcTGivenW(n, gd->wArr[0]);
     }

     for(int i=0; i<gd->age ; i++) {
          double val = calcEntriesUptoAgeT(n, T, (double)i);
          printf("Uptoage %-4d: %5.3f\n", i, val);
     }
     return 1;
}

int printWinAv(gossipData_t *gd) {

     for(int i=0; i< gd->nArrSize ; i++) {
          int n = gd->nArr[i];
          int w = calcWinsizeGivenAv(n, gd->age);
          printf("N=%-6d Desired Av: %-8.3f     Win= %-4d\n", n, gd->age, w);
     }
}

int printWinMax(gossipData_t *gd) {

     for(int i=0; i< gd->nArrSize ; i++) {
          int n = gd->nArr[i];
          int w = calcWinsizeGivenMax(n, gd->age);
          printf("N=%-6d Desired AvMax: %-8.3f     Win= %-4d\n", n, gd->age, w);
     }
}

int printWinUptoage(gossipData_t *gd) {
     for(int i=0; i< gd->nArrSize ; i++) {
          int n = gd->nArr[i];
          int w = calcWinsizeGivenUptoageEntries(n, gd->age, gd->uptoageEntries);
          if(w == 0) {
               fprintf(stderr, "Error calculating window size for case age=%f entries %d\n",
                       gd->age, gd->uptoageEntries);
               continue;
          }
          printf("N=%-6d Desired entries: %-5d uptoage %-8.3f      Win= %-4d\n",
                 n, gd->uptoageEntries, gd->age, w);
     }
     return 1;
}


int main(int argc, char **argv) {

     gossipData_t gd;
     memset(&gd, 0, sizeof(gossipData_t));
     gd.op = OP_ALL;
     
     progName = strdup(argv[0]);
     parseArgs(&gd, argc, argv);

     switch(gd.op) {
         case OP_ALL:
              printAllInfo(&gd);
              break;
         case OP_CDF:
              printCdf(&gd);
              break;
         case OP_WIN_AV:
              printWinAv(&gd);
              break;
         case OP_WIN_MAX:
              printWinMax(&gd);
              break;
         case OP_WIN_UPTOAGE:
              printWinUptoage(&gd);
              break;
         default:
              ;
     }
     return 0;
}




