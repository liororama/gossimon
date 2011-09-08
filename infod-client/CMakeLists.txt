#=============================================================================
# gossimon - Gossip based resource usage monitoring for Linux clusters
# Copyright 2010 Cluster Logic Ltd (www.clusterlogic.net)
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright-ClusterLogic.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

###############
# General     #
###############

set(CMAKE_INCLUDE_CURRENT_DIR ON)

###################
# infod-client    #
###################
include_directories("/usr/include/glib-2.0/")
include_directories("/usr/lib/glib-2.0/include")

add_executable(infod-client infod_client.c)
target_link_libraries(infod-client info util glib-2.0)

add_custom_command(TARGET infod-client POST_BUILD COMMAND cp -f infod-client ../bin)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/infod-client;infod-client.1.gz") 

add_custom_command(TARGET infod-client POST_BUILD COMMAND gzip -c infod-client.man | cat > infod-client.1.gz )

############
# Install  #
############
install(TARGETS infod-client
        RUNTIME DESTINATION bin)

install(FILES infod-client.1.gz
  DESTINATION share/man/man1)

