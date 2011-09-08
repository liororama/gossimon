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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE ")

add_executable(testload test_load.c)

add_custom_command(TARGET testload POST_BUILD COMMAND cp -f testload ../bin)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/testload;testload.1.gz;repeat-bg.1.gz") 

add_custom_command(TARGET testload POST_BUILD COMMAND gzip -c testload.man | cat > testload.1.gz )

add_custom_command(TARGET testload POST_BUILD COMMAND gzip -c repeat-bg.man | cat > repeat-bg.1.gz )

############
# Install  #
############
install(TARGETS testload
        RUNTIME DESTINATION bin)


install(
  PROGRAMS repeat-bg
  DESTINATION bin
)

install(FILES testload.1.gz repeat-bg.1.gz
  DESTINATION share/man/man1)



