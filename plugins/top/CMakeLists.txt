#=============================================================================
# TopPIM - Finding cpu intensive processes plugin for gossimon
# Copyright 2011 Lior Amar (liororama@gmail.com)
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright-lior.txt for details.
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
include_directories("/usr/include/libxml++-2.6")

include_directories("/usr/include/libxml++-2.6") 
include_directories("/usr/lib/libxml++-2.6/include") 
include_directories("/usr/include/libxml2") 
include_directories("/usr/include/glibmm-2.4") 
include_directories("/usr/lib/glibmm-2.4/include") 
include_directories("/usr/include/sigc++-2.0") 
include_directories("/usr/lib/sigc++-2.0/include") 
include_directories("/usr/include/glib-2.0") 
include_directories("/usr/lib/glib-2.0/include")  

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -std=c++0x -g -fPIC -Wall  -D __USE_GNU -D _GNU_SOURCE -Wl,--whole-archive -L../  -lxml++-2.6 -lxml2 -Wl,--no-whole-archive")
set(CMAKE_C_FLAGS "-g -std=gnu99 -fgnu89-inline -Wl,--whole-archive -L../  -lxml2 -Wl,--no-whole-archive ")


set(PLUGIN_NAME "top")
set(PLUGIN_LIB_NAME "toplib")
set(PLUGIN_MAIN "${PLUGIN_NAME}_main")
set(PLUGIN_CONF "top.conf")

#set(pim_DIR InfoModules)
#include_directories(${pim_DIR})

set(plugin_SRC TopPIM.cpp TopFinder.cpp TopSaxParser.cpp TopMD.cpp TopMD_di.c UidToUser.cpp)
set(plugin_main_SRC top_main.cpp)

add_executable(${PLUGIN_MAIN}  ${plugin_main_SRC} TopPIM.cpp TopFinder.cpp TopSaxParser.cpp )
target_link_libraries(${PLUGIN_MAIN} util glib-2.0 dl pthread xml++-2.6 xml2 glibmm-2.4 gobject-2.0 sigc-2.0 gthread-2.0 rt  )

#add_custom_command(TARGET infod POST_BUILD COMMAND cp -f infod ../bin)
#SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "../bin/infod;infod.8.gz") 

add_library( ${PLUGIN_NAME} MODULE ${plugin_SRC} )
set_target_properties( ${PLUGIN_NAME} PROPERTIES PREFIX "")

add_library( ${PLUGIN_LIB_NAME} STATIC ${plugin_SRC} )
#set_target_properties( ${PLUGIN_LIB_NAME} PROPERTIES PREFIX "")

# ##################
# # Unit Tests     #
# ##################

enable_testing()

file(GLOB unit_tests_src  "./tests/*Test.cpp" )
message("Detected [${PLUGIN_NAME}] unit tests: ${unit_tests_src}")

include_directories("./tests")
ADD_EXECUTABLE(unit_tester ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRunner.cpp ${unit_tests_src})
target_link_libraries(unit_tester ${PLUGIN_LIB_NAME} cppunit util glib-2.0 dl pthread xml++-2.6 xml2 glibmm-2.4 gobject-2.0 sigc-2.0 gthread-2.0 rt  )


FOREACH(test_file ${unit_tests_src})
   GET_FILENAME_COMPONENT(TestName ${test_file} NAME_WE)
   MESSAGE("  o   Adding test: ${TestName}")

   ADD_TEST(NAME ${TestName} COMMAND unit_tester ${TestName})
#   add_dependencies(check1 ${TestName})
ENDFOREACH(test_file)



# #################
# # Install       #
# #################
install(TARGETS ${PLUGIN_NAME}
        DESTINATION share/gossimon/plugins)

install(FILES ${PLUGIN_CONF}
        DESTINATION ${GOSSIMON_INSTALL_ETC}/gossimon/plugins-available/)

# install(TARGETS infod
#   RUNTIME DESTINATION sbin)

# install(FILES infod.init
#   DESTINATION share/doc/${PROJECT_NAME}
#   RENAME infod.rc)

# install(FILES infod.init
#   DESTINATION "${GOSSIMON_INSTALL_ETC}/init.d"
#   RENAME infod
#   PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
# )


# install(FILES infod.conf.sample
#   DESTINATION share/doc/${PROJECT_NAME})

# install(FILES infod.conf.sample
#   DESTINATION "${GOSSIMON_INSTALL_ETC}/${PROJECT_NAME}"
#   RENAME infod.conf
# )

# install(FILES infod.map.localhost
#   DESTINATION "${GOSSIMON_INSTALL_ETC}/${PROJECT_NAME}"
#   RENAME infod.map
# )

# install(FILES README.plugins-available
#   DESTINATION "${GOSSIMON_INSTALL_ETC}/${PROJECT_NAME}/plugins-available"
#   RENAME README
#   )

# install(FILES README.plugins-enabled
#   DESTINATION "${GOSSIMON_INSTALL_ETC}/${PROJECT_NAME}/plugins-enabled"
#   RENAME README
#   )


# install(FILES infod.default-args
#   DESTINATION share/doc/${PROJECT_NAME})

# install(FILES infod.default-args
#   DESTINATION "${GOSSIMON_INSTALL_ETC}/default"
#   RENAME infod)



# install(FILES infod.8.gz
#   DESTINATION share/man/man8)

# #################
# # DEB           #
# #################
# set( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst" )
