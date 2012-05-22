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
# libgossip.a   #
###################
set(gossip_SOURCES  gossiplib.c)

add_library(gossip STATIC ${gossip_SOURCES})
add_executable(gossip-calc gossip-calc.c)
target_link_libraries(gossip-calc util)
target_link_libraries(gossip-calc gossip)
target_link_libraries(gossip-calc m)

add_custom_command(TARGET gossip POST_BUILD COMMAND cp -f libgossip.a ../bin)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/libgossip.a") 





