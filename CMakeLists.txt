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

project(gossimon)
cmake_minimum_required(VERSION 2.8)
enable_testing()

set (GOSSIMON_VERSION_MAJOR  1)
set (GOSSIMON_VERSION_MINOR  8)
set (GOSSIMON_VERSION_PATCH  13)

#message("Command: ${gossimon_SOURCE_DIR}/prep/prep-helper/detect-version.pl ${gossimon_SOURCE_DIR}/CHANGELOG")

#execute_process(
#  COMMAND "${gossimon_SOURCE_DIR}/prep/prep-helper/detect-version.pl" "${gossimon_SOURCE_DIR}/CHANGELOG"
#  RESULT_VARIABLE  ver_detect_res
#  OUTPUT_VARIABLE  ver_detect_out
#)
#message("Detected version result: ${ver_detect_res}")
#message("Detected version: [${ver_detect_out}]")

#if(${ver_detect_res} EQUAL 0) 
#  message("Successfuly detected version ")
#  string(REGEX MATCH "[0-9]+" ver_major "${ver_detect_out}")
#  string(REGEX REPLACE "[0-9]+\\.([0-9]+)\\.[0-9]+" "\\1" ver_minor "${ver_detect_out}")
#  string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" ver_patch "${ver_detect_out}")

# set(ver_minotr ${CMAKE_MATCH_(0)})
  #string(REGEX MATCH "[0-9]+" ver_major "${ver_detect_out}")
#  message("Major: ${ver_major}  Minor: ${ver_minor} Patch: ${ver_patch}")
#  set (GOSSIMON_VERSION_MAJOR  ${ver_major})
#  set (GOSSIMON_VERSION_MINOR  ${ver_minor})
#  set (GOSSIMON_VERSION_PATCH  ${ver_patch})
#else ()
#  message(WARNING "Could not detect version - using internal")
#endif()

include("CMakeCheckPrerequisites.txt")


set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories("include")
include_directories("mmon")
include_directories(${LibXml2_INCLUDE_DIR})
include_directories(${GTK2_INCLUDE_DIRS})
include_directories("/usr/include/glib-2.0")
include_directories("/usr/lib/glib-2.0/include")
include_directories("/usr/libxml++-2.6/")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/bin")

# The CL_UTIL_PREFIX specifies the location to look for the libclutil installation
#set(CL_UTIL_PREFIX "/usr/local" CACHE PATH "libclutil installation prefix path")
#include_directories("${CL_UTIL_PREFIX}/include")
#link_directories("${CL_UTIL_PREFIX}/lib")

# Verbose in make
#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_C_FLAGS "-g -std=gnu99 -fgnu89-inline")

############################################################
# Generating the gossimon_config.h file from the .in file
############################################################
set(GOSSIMON_INSTALL_ETC "${CMAKE_INSTALL_PREFIX}/etc" 
  CACHE PATH "Default location of etc files")

set(GOSSIMON_CONFIG_DIR "${GOSSIMON_INSTALL_ETC}/gossimon")
 
#set( GOSSIMON_EXTERNAL_INFO_MODULE_DIR "${CMAKE_INSTALL_PREFIX}/share/gossimon/plugins" CACHE PATH "Path where the information plugins are to be looked for")

#set( GOSSIMOM_EXTERNAL_DISPLAY_MODULE_DIR "${CMAKE_INSTALL_PREFIX}/share/gossimon/plugins" CACHE PATH "Path where the display plugins are to be looked for")

set( GOSSIMON_PLUGINS_DIR "${CMAKE_INSTALL_PREFIX}/share/gossimon/plugins" CACHE PATH "Path where the gossimon information and display plugins are to be looked for")

set( GOSSIMON_TMP_DIR "/tmp" CACHE PATH "Path where the gossimon tmporary files will be saved")


# Reading the Copyright.txt file into a variable
set(cat_prog cat)
exec_program(${cat_prog} ARGS Copyright.txt OUTPUT_VARIABLE GOSSIMON_COPYRIGHT_STR)
file(READ Copyright.txt GOSSIMON_COPYRIGHT_STR)
string(REGEX REPLACE "\n" "\\\\n" GOSSIMON_COPYRIGHT_STR ${GOSSIMON_COPYRIGHT_STR})
string(REGEX REPLACE "\"" "\\\\\"" GOSSIMON_COPYRIGHT_STR ${GOSSIMON_COPYRIGHT_STR})

#MESSAGE("Content of file is: ${GOSSIMON_COPYRIGHT_STR}")

configure_file (
  "./include/gossimon_config.h.in"
  "./include/gossimon_config.h"
)

#####################
# Extra Targets     #
#####################
add_custom_target(check1 COMMAND ${CMAKE_CTEST_COMMAND} )


######################
# Subdirs            #
######################
add_subdirectory(libutil)
add_subdirectory(libgossip)
add_subdirectory(libinfo)
add_subdirectory(libinfodctl)

add_subdirectory(infod-ctl)
add_subdirectory(infod-client)
add_subdirectory(infod-best)
add_subdirectory(mmon)
add_subdirectory(testload)
add_subdirectory(infod)

add_subdirectory(plugins/top)
add_subdirectory(plugins/meminfo)

include("CMakeInstallGossimon.txt")


#############################################
# Handling the documentation via doxygen    #
#############################################

#find_package(Doxygen)
#if (DOXYGEN_FOUND STREQUAL "NO")
#    message(FATAL_ERROR "Doxygen not found. Please get a copy http://www.doxygen.org")
#endif (DOXYGEN_FOUND STREQUAL "NO")


#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doxygen.in ${CMAKE_CURRENT_BINARY_DIR}/doxygen.conf)
#add_custom_target(doxygen ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doxygen.conf)

# cleanup $build/api-doc on "make clean"
#set_property(DIRECTORY APPEND PROPERTY
#	     ADDITIONAL_MAKE_CLEAN_FILES api-doc)

# add doxygen as dependency to doc-target
#get_target_property(DOC_TARGET doc TYPE)
#if(NOT DOC_TARGET)
#	add_custom_target(doc)
#endif()
#add_dependencies(doc doxygen)
