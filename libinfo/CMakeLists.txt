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
# libinfo.a   #
###################
set(info_SOURCES  infolib.c infoxml.c info_reader.c info_iter.c)

add_library(info STATIC ${info_SOURCES})
add_library(gossimon_client SHARED ${info_SOURCES})
target_link_libraries(gossimon_client gossimon_util)

add_custom_command(TARGET info POST_BUILD COMMAND cp -f libinfo.a ../bin)
add_custom_command(TARGET gossimon_client POST_BUILD COMMAND cp -f libgossimon_client.so ../bin)

SET(extra_files ../bin/libinfo.a ../bin/libgossimon_client.so)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${extra_files}" ) 




file(GLOB unit_tests_SRCS  "./tests/test_*.c" )
message("Detected unit tests: ${unit_tests_SRCS}")


enable_testing()

FOREACH(test_file ${unit_tests_SRCS})
  GET_FILENAME_COMPONENT(TestName ${test_file} NAME_WE)
  MESSAGE("  o   Adding test: [${TestName}]")

  add_executable(${TestName} EXCLUDE_FROM_ALL ${test_file})
  set_target_properties(${TestName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ./tests/)
  target_link_libraries(${TestName} info util check)
#  add_test(NAME ${TestName} COMMAND ${CMAKE_COMMAND} -E chdir tests ${TestName})
  add_test(NAME ${TestName} COMMAND ${CMAKE_COMMAND} -E chdir tests ./${TestName})
  add_dependencies(check1 ${TestName})

ENDFOREACH(test_file)

### INSTALL
install(TARGETS gossimon_client DESTINATION lib/)


