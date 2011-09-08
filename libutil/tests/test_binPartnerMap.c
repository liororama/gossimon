/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#include <common.h>
#include <msx_error.h>
#include <msx_debug.h>

#include <pe.h>
#include <Mapper.h>
#include <MapperBuilder.h>

int main() {

   mapper_t map;
   char *srcDirStr = "/etc/mosix/.orig-partners2";
   char *dstDirStr = "/etc/mosix/grid";

   
   msx_set_debug(MAP_DEBUG);
   map = BuildPartnersMap(srcDirStr, PARTNER_SRC_TEXT);
   mapperPrint(map, PRINT_TXT_PRETTY);

   WriteBinPartnerMap(map, dstDirStr);
   system("/sbin/setcl");

   mapperDone(map);

}
