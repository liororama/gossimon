/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __LOCAL_USAGE_RETRIEVER
#define __LOCAL_USAGE_RETRIEVER


typedef struct _local_usage_retriever *lur_t;

#include <Mapper.h>
#include <UsedByInfo.h>

lur_t  lur_init(char *procDir, mapper_t partnerMap, int maxproc, char *tmpBuff);
void   lur_free(lur_t lur);
void   lur_setVerbose(lur_t lur, int verbose);
void   lur_setDebug(lur_t lur, int debug);
void   lur_setMapper(lur_t lur, mapper_t partnersMap);

int    lur_getLocalUsage(lur_t lur, node_usage_status_t *status,
                         used_by_entry_t *usedByInfo, int *size);

#endif
