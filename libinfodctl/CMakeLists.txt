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
# libinfodctl.a   #
###################
set(infodctl_SOURCES  infodctl.c)

add_library(infodctl STATIC ${infodctl_SOURCES})
add_custom_command(TARGET infodctl POST_BUILD COMMAND cp -f libinfodctl.a ../bin)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/libinfodctl.a") 




