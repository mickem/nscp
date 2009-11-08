cmake_minimum_required(VERSION 2.6)
	
PROJECT(NSCP)
SET(CMAKE_MODULE_PATH ${NSCP_SOURCE_DIR})

SET(VERSION_SERIES 0)
SET(VERSION_MAJOR 4)
SET(VERSION_MINOR 0)

IF(NOT SHARED_LIBS)
  IF(WIN32)
    OPTION(SHARED_LIBS "Compile shared libraries" OFF)
  ELSE(WIN32)
    OPTION(SHARED_LIBS "Compile shared libraries" ON)
  ENDIF(WIN32)
ENDIF(NOT SHARED_LIBS)

IF(NOT SHARED_LIBS)
  SET(NSCP_STATIC true)
ENDIF(NOT SHARED_LIBS)

IF(NOT MULTI_THREADED)
  OPTION(MULTI_THREADED "Build multi-threaded httpd deamon (if possible)" ON)
ENDIF(NOT MULTI_THREADED)

SET(BUILD_SHARED_LIBS ${SHARED_LIBS})

SET(NSCP_LOCAL_INCLUDE_PATH "${PROJECT_BINARY_DIR}/../include" CACHE PATH "directory containing NSCP specific includes")

IF(WIN32)
  SET(USERLIB_ROOT "c:/libraries" CACHE PATH "directory containing the dependency libraries")
  SET(LIB_INSTALL_DIR "lib" CACHE STRING "Default path for libraries within ${CMAKE_INSTALL_PREFIX}")
  SET(RUNDIR "c:/nscp" CACHE PATH "Not really used in WIN32")
  SET(CONFIGURATION c:/witty/wt_config.xml CACHE PATH "Path for the wt configuration file")
  SET(DEPLOYROOT c:/nscp/ CACHE PATH "Path to deploy examples into")
  SET(BOOST_DIR ${USERLIB_ROOT} CACHE PATH "Use boost in directory")
  SET(BOOST_VERSION "1_34_1" CACHE STRING "Use boost version in BOOST_DIR")
  OPTION(BOOST_DYNAMIC "Link to boost DLLs (OFF means static link)" OFF)

ELSE(WIN32)

  SET(USERLIB_ROOT /usr CACHE PATH "Other installation prefix for dependent libraries")
  SET(LIB_INSTALL_DIR "lib" CACHE STRING "Default path for libraries within ${CMAKE_INSTALL_PREFIX}")
  SET(RUNDIR "/usr/wt/run" CACHE PATH "Default path for wt session management (only used by FCGI connector; not relative to CMAKE_INSTALL_PREFIX)")
  SET(CONFIGURATION /etc/nscp/boot.ini CACHE PATH "Path to the boot configuration file")
  SET(MAIN_CONFIGURATION /etc/nscp/nscp.xml CACHE PATH "Path for the client configurtion file")
  SET(DEPLOYROOT /usr/bin CACHE PATH "Path to deploy examples into)")
  SET(BOOST_DIR /usr CACHE PATH "Boost installation path prefix")
  SET(BOOST_COMPILER gcc CACHE STRING "Boost libraries compiler signature")
  SET(BOOST_VERSION "1_34_1" CACHE STRING "Boost libraries version signature")
ENDIF(WIN32)

INCLUDE(../cmake/WtFindBoost.txt)
INCLUDE(../cmake/WtFindAsio.txt)
INCLUDE(../cmake/WtFindSsl.txt)

# Boost is used nearly everywhere, so we can put these here
INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})
IF(WIN32)
  IF(BOOST_DYNAMIC)
    ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)
  ENDIF(BOOST_DYNAMIC)
  set (EXTRA_LIBS ${EXTRA_LIBS} Userenv ole32)
ENDIF(WIN32)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

# Compile time constants & make sure our build finds it
CONFIGURE_FILE(
  config.h.in
  xconfig.h
)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${NSCP_LOCAL_INCLUDE_PATH})
INSTALL_FILES(/include FILES ${WCONFIG_H_PATH})

add_definitions(-DUNICODE -D_UNICODE)

include_directories("${PROJECT_BINARY_DIR}")


add_executable (NSCP 
	NSClient++.cpp
	NSCPlugin.cpp
	core_api.cpp
	settings_manager_impl.cpp
	../include/NSCHelper.cpp
	../include/arrayBuffer.cpp
	../include/ServiceCmd.cpp
	../include/b64/b64.c
)
target_link_libraries (NSCP  ${EXTRA_LIBS})


INSTALL(FILES ${PROJECT_SOURCE_DIR}/cmake/FindWt.cmake DESTINATION
    ${CMAKE_INSTALL_PREFIX}/${WT_CMAKE_FINDER_INSTALL_DIR} )

