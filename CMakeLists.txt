cmake_minimum_required(VERSION 2.6)
	
PROJECT(NSCP)
#SET(CMAKE_MODULE_PATH ${NSCP_SOURCE_DIR})

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

SET(NSCP_INCLUDE_PATH "${NSCP_SOURCE_DIR}/include" CACHE PATH "directory containing NSCP specific includes")
INCLUDE_DIRECTORIES(${NSCP_INCLUDE_PATH})


set(Boost_USE_STATIC_LIBS   ON)
set(Boost_USE_MULTITHREADED ON)


IF(WIN32)
  #SET(USERLIB_ROOT "c:/libraries" CACHE PATH "directory containing the dependency libraries")
  SET(LIB_INSTALL_DIR "lib" CACHE STRING "Default path for libraries within ${CMAKE_INSTALL_PREFIX}")
  SET(RUNDIR "c:/nscp" CACHE PATH "Not really used in WIN32")
  #SET(CONFIGURATION c:/witty/wt_config.xml CACHE PATH "Path for the wt configuration file")
  #SET(DEPLOYROOT c:/nscp/ CACHE PATH "Path to deploy examples into")
  #SET(BOOST_DIR ${USERLIB_ROOT} CACHE PATH "Use boost in directory")

  SET(BOOST_LIBRARYDIR D:/source/lib/x86 CACHE PATH "")
  SET(BOOST_INCLUDEDIR D:/source/include/boost-1_40 CACHE PATH "")
  SET(BOOST_LIB_PREFIX lib CACHE STRING "Look for Boost libraries prefixed with this")
  SET(BOOST_LIB_SUFFIX vc80-mt CACHE STRING "Look for Boost libraries ending with this")

  #SET(BOOST_VERSION "1_34_1" CACHE STRING "Use boost version in BOOST_DIR")
  #OPTION(BOOST_DYNAMIC "Link to boost DLLs (OFF means static link)" OFF)

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

#SET(CMAKE_MODULE_PATH "../cmake/")
FIND_PACKAGE(Boost COMPONENTS system filesystem thread REQUIRED)

#INCLUDE(../cmake/WtFindBoost.txt)
#INCLUDE(../cmake/WtFindAsio.txt)
#INCLUDE(cmake/WtFindSsl.txt)
#INCLUDE(../cmake/FindICU.cmake)



#IF(NOT WIN32)
#	FIND_PACKAGE(ICU REQUIRED)
#	IF(NOT ICU_FOUND)
#		MESSAGE(FATAL_ERROR "ICU package not found..")
#	ELSE(NOT ICU_FOUND)
#		ADD_DEFINITIONS( -DSI_CONVERT_ICU )
#	ENDIF(NOT ICU_FOUND)
#ENDIF(NOT WIN32)

IF(NOT WIN32)
	FIND_PACKAGE(Threads REQUIRED)
	IF (NOT CMAKE_USE_PTHREADS_INIT)
	  message(FATAL_ERROR "Could not find POSIX threads")
	ELSE (NOT CMAKE_USE_PTHREADS_INIT)
	  #add_definitions(-D_REENTRANT)
	ENDIF (NOT CMAKE_USE_PTHREADS_INIT)
	FIND_PACKAGE(Threads REQUIRED)
ENDIF(NOT WIN32)


SET(NSCP_GLOBAL_DEFINES
	-DUNICODE -D_UNICODE
)

IF(CMAKE_HOST_UNIX)
	SET(UNIX_FLAVOUR LINUX CACHE STRING "Choose your unix flavour UNIX, AIX, HPUX" FORCE)
	IF(UNIX_FLAVOUR STREQUAL LINUX)
		SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DLINUX)
	ENDIF(UNIX_FLAVOUR STREQUAL LINUX)
	
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_DL_LIBS})
ENDIF()

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

#SET(RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/xxx/")
#SET(LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/yyy/")
#SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/zzz/")
#SET(MODULE_OUTPUT_DIRECTORY  "${PROJECT_BINARY_DIR}/aaa/")
#SET(ARCHIVE_OUTPUT_DIRECTORY  "${PROJECT_BINARY_DIR}/bbb/")

MESSAGE(STATUS "Build path: ${PROJECT_BINARY_DIR}")
ADD_SUBDIRECTORY("service")

FILE(GLOB ALL_MODULES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "modules/*/CMakeLists.txt")
foreach(CURRENT_MODULE ${ALL_MODULES})
	get_filename_component(CURRENT_MODULE_PATH ${CURRENT_MODULE} PATH)
	message(STATUS "Adding module: ${CURRENT_MODULE_PATH}")
	ADD_SUBDIRECTORY("${CURRENT_MODULE_PATH}")
endforeach(CURRENT_MODULE ${ALL_MODULES})


SET_TARGET_PROPERTIES("nsclient++" "CheckHelpers" PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/modules"
        MODULE_OUTPUT_DIRECTORY  "${PROJECT_BINARY_DIR}/modules"
        COMPILE_DEFINITIONS_DEBUG _DEBUG
)

if (MSVC_IDE)
    # hack to get around the "Debug" and "Release" directories cmake tries to add on Windows
    set_target_properties ("nsclient++" "CheckHelpers" PROPERTIES PREFIX "../")
endif()

message(STATUS "${ALL_MODULES} -- ${CMAKE_CURRENT_SOURCE_DIR}")

INSTALL(FILES ${PROJECT_SOURCE_DIR}/cmake/FindWt.cmake DESTINATION
    ${CMAKE_INSTALL_PREFIX}/${WT_CMAKE_FINDER_INSTALL_DIR} )

