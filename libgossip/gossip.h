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

#ifndef __LIB_GOSSIP_
#define __LIB_GOSSIP_

// Functions for calculating the information age properties when using push
// based gossip protocol


double calcXT(int n, double T);
double calcTGivenW(int n, int w);

double calcAv(int n, double T);
double calcEntriesUptoAgeT(int n, double T, double t);
double calcMaxAge(int t, double T);
int calcWinsizeGivenAv(int n, double desiredAv);
int calcWinsizeGivenMax(int n, double desiredMax);
int calcWinsizeGivenUptoageEntries(int n, double desiredAge, int desiredEntriesNum);



#endif
