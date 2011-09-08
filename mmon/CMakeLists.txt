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
include_directories("/usr/include/ncursesw/")

set(DISPLAY_MODULES_DIR DisplayModules)

include_directories(${DISPLAY_MODULES_DIR})
set(DISPLAY_MODULES_SOURCES GeneralMD.c
  SeperatorMD.c 
  EcoInfoMD.c 
  EcoInfoMD2.c
  EcoInfoMD3.c
  EcoInfoMD4.c
  JMigrateMD.c)

foreach (f ${DISPLAY_MODULES_SOURCES})
  set(DM_FILES ${DM_FILES} ${DISPLAY_MODULES_DIR}/${f})
endforeach()

set(MMON_SOURCES mmon.c
  mmonCommandLine.c
  cluster_list.c
  cluster_id.c
  ConfigurationManager.c 
  display.c
  input.c
  dialogs.c
  helpStrings.c
  displaySaveFile.c
  DisplayModules.c
  NodeInfoWin.c
  drawHelper.c
  ${DM_FILES})

add_executable(mmon ${MMON_SOURCES})
target_link_libraries(mmon ncurses m info util xml2 z pthread glib-2.0)

add_custom_command(TARGET mmon POST_BUILD COMMAND cp -f mmon ../bin)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/mmon;mmon.1.gz") 

add_custom_command(TARGET mmon POST_BUILD COMMAND gzip -c mmon.man | cat > mmon.1.gz )

############
# Install  #
############
install(TARGETS mmon
        RUNTIME DESTINATION bin)

install(FILES mmon.conf.sample
        DESTINATION share/doc/${PROJECT_NAME})
install(FILES mmon.conf.sample
        DESTINATION "${GOSSIMON_INSTALL_ETC}/${PROJECT_NAME}"
	RENAME mmon.conf)

 
install(FILES mmon.1.gz
  DESTINATION share/man/man1)



