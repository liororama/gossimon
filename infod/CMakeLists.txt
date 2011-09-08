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
include_directories("../libgossip")
include_directories("/usr/include/glib-2.0/")
include_directories("/usr/lib/glib-2.0/include")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fPIC -Wall  -D__USE_GNU -D_GNU_SOURCE")

set(pim_DIR InfoModules)
include_directories(${pim_DIR})

set(infovec_SRC infoVec.c)
set(infod_SRC infod.c  
              infodCommandLine.c 
	      infodMisc.c 
	      infodParent.c 
	      ${infovec_SRC}
	      ctl.c 
	      gossipAlgo.c)



set(pim_SRC UsedByPIM.c 
	    ProcessWatchPIM.c
	    FreezeInfoPIM.c 
	    ClusterID_CRC_PIM.c
	    InfodDebugPim.c
	    EconomyPIM.c
	    JMigratePIM.c)

foreach (f ${pim_SRC})
  set(pim_FILES ${pim_FILES} ${pim_DIR}/${f})
endforeach()

set(pim_SRC ${pim_FILES})

set(provider_SRC   provider.c
   	           providerUtil.c
		   prioHeap.c
		   linuxMosixProvider.c
		   infoModuleManager.c
		   ${pim_SRC})


add_executable(infod ${infod_SRC} ${provider_SRC})
target_link_libraries(infod  m info infodctl mapper gossip util glib-2.0 dl)

add_custom_command(TARGET infod POST_BUILD COMMAND cp -f infod ../bin)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/infod;infod.8.gz") 

add_custom_command(TARGET infod POST_BUILD COMMAND gzip -c  infod.man | cat > infod.8.gz)


add_library(dummy MODULE ${pim_DIR}/DummyExternalPIM.c )
set_target_properties(dummy PROPERTIES PREFIX "")


add_library(provider STATIC ${provider_SRC})
add_library(infovec STATIC ${infovec_SRC})

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
  target_link_libraries(${TestName} provider infovec  mapper util check xml2 glib-2.0)


  ADD_TEST(NAME ${TestName} COMMAND ${CMAKE_COMMAND} -E chdir tests ./${TestName})
  add_dependencies(check1 ${TestName})
ENDFOREACH(test_file)



#################
# Install       #
#################

install(TARGETS infod
  RUNTIME DESTINATION sbin)

install(FILES infod.init
  DESTINATION share/doc/${PROJECT_NAME}
  RENAME infod.rc)

install(FILES infod.init
  DESTINATION "${GOSSIMON_INSTALL_ETC}/init.d"
  RENAME infod
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)


install(FILES infod.conf.sample
  DESTINATION share/doc/${PROJECT_NAME})

install(FILES infod.conf.sample
  DESTINATION "${GOSSIMON_INSTALL_ETC}/${PROJECT_NAME}"
  RENAME infod.conf
)

install(FILES infod.map.localhost
  DESTINATION "${GOSSIMON_INSTALL_ETC}/${PROJECT_NAME}"
  RENAME infod.map
)

install(FILES README.plugins-available
  DESTINATION "${GOSSIMON_INSTALL_ETC}/${PROJECT_NAME}/plugins-available"
  RENAME README
  )

install(FILES README.plugins-enabled
  DESTINATION "${GOSSIMON_INSTALL_ETC}/${PROJECT_NAME}/plugins-enabled"
  RENAME README
  )


install(FILES infod.default-args
  DESTINATION share/doc/${PROJECT_NAME})

install(FILES infod.default-args
  DESTINATION "${GOSSIMON_INSTALL_ETC}/default"
  RENAME infod)



install(FILES infod.8.gz
  DESTINATION share/man/man8)

#################
# DEB           #
#################
set( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst" )
