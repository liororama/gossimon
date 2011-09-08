/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/



#ifndef __MAPPER_BUILDER_H
#define __MAPPER_BUILDER_H


#include  <Mapper.h>

// Mosix Map
mapper_t BuildMosixMap(const char *buff, int size, map_input_type type);

// Userview Map
mapper_t BuildUserViewMap(const char *buff, int size, map_input_type type);


// Partners Map
typedef enum {
	PARTNER_SRC_TEXT,
	PARTNER_SRC_BIN
} partner_map_t;

mapper_t BuildPartnersMap(const char *partnerDir, partner_map_t type);
int      WritePartnerMap(mapper_t map, const char *partnerDir);
int      WriteBinPartnerMap(mapper_t map, const char *partnerDir);

#endif
