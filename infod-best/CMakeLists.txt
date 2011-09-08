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
add_executable(infod-best infod_best.c)
target_link_libraries(infod-best info loadconstraints util glib-2.0)

add_custom_command(TARGET infod-best POST_BUILD COMMAND cp -f infod-best ../bin)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/infod-best;infod-best.1.gz") 

add_custom_command(TARGET infod-best POST_BUILD COMMAND gzip -c infod-best.man | cat > infod-best.1.gz )

############
# Install  #
############
install(
  TARGETS infod-best
  RUNTIME 
  DESTINATION bin
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE SETUID)

install(FILES infod-best.1.gz
  DESTINATION share/man/man1)




