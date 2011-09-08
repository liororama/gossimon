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
add_executable(infod-ctl infod_ctl.c)
target_link_libraries(infod-ctl util infodctl)
add_custom_command(TARGET infod-ctl POST_BUILD COMMAND cp -f infod-ctl ../bin)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/infod-ctl;infod-ctl.8.gz") 

add_custom_command(TARGET infod-ctl POST_BUILD COMMAND gzip -c infod-ctl.man | cat > infod-ctl.8.gz )

############
# Install  #
############
install(TARGETS infod-ctl
        RUNTIME DESTINATION sbin)

install(FILES infod-ctl.8.gz
  DESTINATION share/man/man8)



