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

include_directories("/usr/include/libxml2")
include_directories("/usr/include/glib-2.0")
include_directories("/usr/lib/glib-2.0/include")

###################
# libutil.a   #
###################
file(GLOB util_SOURCES "./*.c" )
file(GLOB util_HEADERS "../include/*.h")
set(util_SOURCES  mem_cache.c  msx_proc.c pe.c 
	   easy_args.c mconf.c  msx_error.c  
	   parse_helper.c  readproc.c comm.c 
	   crc32.c debug_util.c
	   host_list.c  
	   TimeUtil.c 
	   DiskInfo.c 
	   UsedByInfo.c LocalUsageRetriever.c 
	   ProcessWatchInfo.c FreezeInfo.c 
	   InfodDebugInfo.c EcoInfo.c JMigrateInfo.c 
           ConfFileReader.c
           pluginUtil.c
           ColorPrint.c
           ModuleLogger.c
)

set (mapper_SOURCES Mapper.c MapperBuilder.c 
                    UserViewMapBuilder.c PartnersMapBuilder.c 
	            MosixMapBuilder.c)
set (load_constraint_SOURCES LoadConstraints.c)


add_library(util STATIC ${util_SOURCES})
add_library(mapper STATIC ${mapper_SOURCES})
add_library(loadconstraints STATIC ${load_constraint_SOURCES})


add_custom_command(TARGET util POST_BUILD COMMAND cp -f libutil.a ../bin)
add_custom_command(TARGET util POST_BUILD COMMAND cp -f libmapper.a ../bin)
add_custom_command(TARGET util POST_BUILD COMMAND cp -f libloadconstraints.a ../bin)

SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/libutil.a;../bin/libmapper.a;../bin/libloadconstraints.a") 



##################
# Unit Tests     #
##################

enable_testing()

file(GLOB unit_tests_SRCS  "./tests/test_*.c" )
message("Detected unit tests: ${unit_tests_SRCS}")

FOREACH(test_file ${unit_tests_SRCS})
  GET_FILENAME_COMPONENT(TestName ${test_file} NAME_WE)
  MESSAGE("  o   Adding test: ${TestName}")

  add_executable(${TestName} EXCLUDE_FROM_ALL ${test_file}  )
  set_target_properties(${TestName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ./tests/)
  target_link_libraries(${TestName} loadconstraints mapper util check xml2 glib-2.0)


  ADD_TEST(NAME ${TestName} COMMAND ${CMAKE_COMMAND} -E chdir tests ./${TestName})
  add_dependencies(check1 ${TestName})
ENDFOREACH(test_file)



############################################
# Installing the library and include files #
# in the lib and include directories which #
# should be ../../lib and ../../include    #
############################################

#set(CMAKE_INSTALL_PREFIX  ${PROJECT_SOURCE_DIR}/../.. )
#install(
#  FILES ${cbox_util_HEADERS}
#  DESTINATION include/cbox_util
#)

#install(TARGETS cbox_util
#        ARCHIVE DESTINATION lib)




