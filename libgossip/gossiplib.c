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

#define __USE_GNU
#include <math.h>

double calcXT(int n, double T) {
     double N = n;
     double tmp = pow(M_El, N*T/(N-1.0));

     return N*tmp/(N - 1.0 + tmp);
}

double calcAw(int n, double T) {
     double N = n;
     double Xt = calcXT(n, T);
     double tmp = pow(M_El, N*T/(N-1.0));

     
     return T - ((N-1)/Xt) * (log(N - 1.0 + tmp) - log(N));
}


double calcAv(int n, double T) {
     double N  = n;
     double Xt = calcXT(n, T);
     double Aw = calcAw(n, T);
     
     return (1/(1 - pow(1 - 1/(N-1.0), Xt))) + Aw;
}


double calcEntriesUptoAgeT(int n, double T, double t) {
     double N = n;
     double Xt = calcXT(n, T);
     
     if( t <= T)
          return Xt;

     double Aw = calcAw(n, T);
     return N * ( 1 - pow( 1 - 1/(N-1.0), Xt * ( t - Aw)));
}

double calcMaxAge(int n, double T) {
     double N = n;
     double Xt = calcXT(n, T);

     return -1 * ( (log(N) + 0.577) / ( Xt  * log(1 - 1/(N-1.0)) ) );
     
}

double calcTGivenW(int n, int w) {
     double N = n;
     double W = w;

     return log(W*(N-1.0)/(N-W))*(N-1.0)/N;
}

int calcWinsizeGivenAv(int n, double desiredAv) {
     double N = n;

     if(n<=0) return 0;
     if(desiredAv <= 0) return 0;
     
     int maxW = n+1;
     int w = 1;
     for( ; w<maxW ; w++) {
          double T = calcTGivenW(n, (double)w);
          double Av = calcAv(n, T);
          if(Av < desiredAv)
               break;
     }
     // Error
     if(w == maxW) {
          return 0;
     }
     return w;
}

int calcWinsizeGivenMax(int n, double desiredMax) {
     double N = n;

     if(n<=0) return 0;
     if(desiredMax <= 0) return 0;
     
     int maxW = n+1;
     int w = 1;
     for( ; w<maxW ; w++) {
          double T = calcTGivenW(n, (double)w);
          double Max = calcMaxAge(n, T);
          if(Max < desiredMax)
               break;
     }
     // Error
     if(w == maxW) {
          return 0;
     }
     return w;
}

int calcWinsizeGivenUptoageEntries(int n, double desiredAge, int desiredEntriesNum)
{
     if(n<=0) return 0;
     if(desiredAge <= 0) return 0;
     if(desiredEntriesNum >= n)
          return 0;
     
     double N = n;
     
     int maxW = n+1;
     int w = 1;
     for( ; w<maxW ; w++) {
          double T = calcTGivenW(n, (double)w);
          double e = calcEntriesUptoAgeT(n, T, desiredAge);

          //printf("E %f W %d\n", e, w);
          if(e >= desiredEntriesNum)
               break;
     }
     
     // Error
     if(w == maxW) {
          return 0;
     }
     return w;
}
