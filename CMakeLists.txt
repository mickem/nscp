cmake_minimum_required(VERSION 2.6)
	
PROJECT(NSCP)

SET(VERSION_SERIES 0)
SET(VERSION_MAJOR 4)
SET(VERSION_MINOR 0)


INCLUDE(cmake/SetMultiThread.cmake)
# ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ###
#
# Setup section
# An option to override this is to set them in the build.cmake file
#
# ### ### ### ###
# Include local build enviornment
include(build.cmake)
# ### ### ### ###
# Generic includes
# TODO
# ### ### ### ###
# NSCP Includes
SET(NSCP_LIBRARYDIR "${INC_NSCP_LIBRARYDIR}" CACHE PATH "Generic path to all libraries")
SET(NSCP_INCLUDEDIR "${NSCP_SOURCE_DIR}/include" CACHE PATH "Generic include path (for NSCP include files amongst others)")
# ### ### ### ###
# Boost setup
SET(Boost_USE_STATIC_LIBS CACHE BOOL ON "Link boost statically (generally a good idea)")
SET(BOOST_INCLUDEDIR "${INC_BOOST_INCLUDEDIR}" CACHE PATH "Path to boost includes")
SET(BOOST_LIBRARYDIR "${NSCP_LIBRARYDIR}" CACHE PATH "Path to boost libraries")
# ### ### ### ###
# Open SSL
SET(OPENSSL_INCLUDE_DIR "${INC_OPENSSL_INCLUDEDIR}" CACHE PATH "Path to openssl includes")
# ### ### ### ###
# Google Protocol buffer
SET(PROTOBUF_DIR "${INC_PROTOBUF_DIR}" CACHE PATH "Path to google protocol buffer root")
IF(WIN32 AND MSVC)
	SET(PROTOBUF_INCLUDE_DIR "${PROTOBUF_DIR}/src" CACHE PATH "Path to google protocol buffer source")
	SET(PROTOBUF_LIBRARYDIR "${PROTOBUF_DIR}/vsprojects/Debug")
	SET(PROTOBUF_BINARYDIR "${PROTOBUF_DIR}/vsprojects/Debug")
ELSE(WIN32 AND MSVC)
# TODO
ENDIF(WIN32 AND MSVC)
# ### ### ### ###
# Crypto++
SET(CRYPTOPP_DIR "${INC_CRYPTOPP_DIR}" CACHE PATH "Path to crypto++ root folder")
# ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ###

IF(APPLE)
	SET_TARGET_PROPERTIES(${TARGET} PROPERTIES COMPILE_FLAGS "-fPIC -w -DOSX  -DNDEBUG -finline-functions -O3 -DCRYPTOPP_DISABLE_ASM")
ELSEIF(UNIX AND NOT APPLE)
	IF (ARCH_X86_64 EQUAL 1)
		MESSAGE(STATUS, "Adding FPIC")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
	ELSE(ARCH_X86_64 EQUAL 1)
		MESSAGE(STATUS, "Adding FPIC (noia64)")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
	ENDIF (ARCH_X86_64 EQUAL 1)
	#SET_TARGET_PROPERTIES(${TARGET} PROPERTIES COMPILE_FLAGS "-fPIC -w -pipe -DNDEBUG -DLINUX -finline-functions -O3")
ELSEIF(WIN32)
	IF(NOT MSVC)
		SET_TARGET_PROPERTIES(${TARGET} PROPERTIES COMPILE_FLAGS "-mthreads -w -finline-functions -O3 -DNDEBUG -D__MINGW__ -DWIN32 -D_WIN32_WINNT=0x0501 -D_WIN32")
	ENDIF(NOT MSVC)
ENDIF(APPLE)

SET_MULTITHREAD()

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

INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR})
SET(CMAKE_LIBRARY_PATH "${NSCP_LIBRARYDIR}" CACHE PATH "Path to generic libraries")
#SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} CACHE PATH "Look for libraries here")

IF(WIN32)
  SET(LIB_INSTALL_DIR "lib" CACHE STRING "Default path for libraries within ${CMAKE_INSTALL_PREFIX}")
  SET(RUNDIR "c:/nscp" CACHE PATH "Not really used in WIN32")


  #SET(BOOST_VERSION "1_34_1" CACHE STRING "Use boost version in BOOST_DIR")
  #OPTION(BOOST_DYNAMIC "Link to boost DLLs (OFF means static link)" OFF)

ELSE(WIN32)
  SET(USERLIB_ROOT /usr CACHE PATH "Other installation prefix for dependent libraries")
  SET(LIB_INSTALL_DIR "lib" CACHE STRING "Default path for libraries within ${CMAKE_INSTALL_PREFIX}")
  SET(RUNDIR "/usr/wt/run" CACHE PATH "Default path for wt session management (only used by FCGI connector; not relative to CMAKE_INSTALL_PREFIX)")
  SET(CONFIGURATION /etc/nscp/boot.ini CACHE PATH "Path to the boot configuration file")
  SET(MAIN_CONFIGURATION /etc/nscp/nscp.xml CACHE PATH "Path for the client configurtion file")
  SET(DEPLOYROOT /usr/bin CACHE PATH "Path to deploy examples into)")
ENDIF(WIN32)


FIND_PACKAGE(OpenSSL)
if(OPENSSL_FOUND)
	if(WIN32)
		FIND_LIBRARY(LIB_EAY_DEBUG NAMES libeay32MDd libeay32)
		FIND_LIBRARY(LIB_EAY_RELEASE NAMES libeay32MD libeay32)
      IF(LIB_EAY_DEBUG AND LIB_EAY_RELEASE)
         SET(OPENSSL_LIBRARIES ${OPENSSL_LIBRARIES} optimized ${LIB_EAY_RELEASE} debug ${LIB_EAY_DEBUG})
      ELSE(LIB_EAY_DEBUG AND LIB_EAY_RELEASE)
		SET(OPENSSL_FOUND FALSE)
         SET(OPENSSL_LIBRARIES NOTFOUND)
         MESSAGE(STATUS "Could not find the debug and release version of openssl")
      ENDIF(LIB_EAY_DEBUG AND LIB_EAY_RELEASE)
	endif(WIN32)
endif(OPENSSL_FOUND)

if(OPENSSL_FOUND)
	message(STATUS, "Found openssl: ${OPENSSL_LIBRARIES}")
	SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DUSE_SSL)
else(OPENSSL_FOUND)
	message(STATUS, "openssl NOT found (no ssl support)")
endif(OPENSSL_FOUND)

FIND_PACKAGE(Boost COMPONENTS system filesystem thread REQUIRED)

if(Boost_FOUND)
	message(STATUS, "Found boost in: ${Boost_INCLUDE_DIRS}")
else(Boost_FOUND)
	message(FATAL_ERROR, "boost NOT fround: ${Boost_ERROR_REASON}")
endif(Boost_FOUND)

INCLUDE(cmake/NSCPPBuffer.cmake)

if(PROTOBUF_FOUND)
	message(STATUS, "Found proto in: ${PROTOBUF_LIBRARY}")
	INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIR})
else(PROTOBUF_FOUND)
	message(FATAL_ERROR, "Protocol buffers NOT found (${PROTOBUF_LIBRARYDIR}, )")
endif(PROTOBUF_FOUND)


#INCLUDE(cmake/WtFindAsio.txt)
#INCLUDE(cmake/WtFindSsl.txt)
#INCLUDE(cmake/FindICU.cmake)


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


SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0400)

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

SET(NSCP_DEF_PLUGIN_CPP
	${NSCP_INCLUDEDIR}/arrayBuffer.cpp
	${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
	${NSCP_INCLUDEDIR}/b64/b64.c
	${NSCP_INCLUDEDIR}/utils.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_wrapper.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
)

SET(NSCP_DEF_PLUGIN_HPP
	${NSCP_INCLUDEDIR}/strEx.h
	${NSCP_INCLUDEDIR}/config.h
	${NSCP_INCLUDEDIR}/utils.h
	${NSCP_INCLUDEDIR}/NSCAPI.h
	${NSCP_INCLUDEDIR}/nscapi/macros.hpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_wrapper.hpp	
	${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.hpp
)

SET(NSCP_DEF_PLUGIN_LIB
	${Boost_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${EXTRA_LIBS}
	${PROTOBUF_LIBRARY}
	protobuf
)
message(STATUS, "NSCP PLUGIN LIB: ${NSCP_DEF_PLUGIN_LIB}")
message(STATUS, "NSCP PLUGIN CPP: ${NSCP_DEF_PLUGIN_CPP}")

	
SET(ALL_LIB_NAMES)
FILE(GLOB ALL_LIB_PROJECTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "libs/*/CMakeLists.txt")
foreach(CURRENT_LIB ${ALL_LIB_PROJECTS})
	get_filename_component(CURRENT_LIB_PATH ${CURRENT_LIB} PATH)
	get_filename_component(CURRENT_LIB_NAME ${CURRENT_LIB_PATH} NAME)
	message(STATUS "Adding library: ${CURRENT_LIB_PATH} (${CURRENT_LIB_NAME})")
	ADD_SUBDIRECTORY("${CURRENT_LIB_PATH}")
	SET(ALL_LIB_NAMES ${ALL_LIB_NAMES} ${CURRENT_LIB_NAME})
endforeach(CURRENT_LIB ${ALL_LIB_PROJECTS})
	

# Compile time constants & make sure our build finds it
CONFIGURE_FILE(
  ${NSCP_INCLUDEDIR}/config.h.in
  ${NSCP_INCLUDEDIR}/config.h
)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${NSCP_LOCAL_INCLUDE_PATH})
INSTALL_FILES(/include FILES ${WCONFIG_H_PATH})

add_definitions(-DUNICODE -D_UNICODE)

include_directories("${PROJECT_BINARY_DIR}")

MESSAGE(STATUS "Build path: ${PROJECT_BINARY_DIR}")
ADD_SUBDIRECTORY("service")

SET(ALL_MODULE_NAMES)
FILE(GLOB ALL_MODULES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "modules/*/CMakeLists.txt")
foreach(CURRENT_MODULE ${ALL_MODULES})
	get_filename_component(CURRENT_MODULE_PATH ${CURRENT_MODULE} PATH)
	get_filename_component(CURRENT_MODULE_NAME ${CURRENT_MODULE_PATH} NAME)
	message(STATUS "Adding module: ${CURRENT_MODULE_PATH} (${CURRENT_MODULE_NAME})")
	ADD_SUBDIRECTORY("${CURRENT_MODULE_PATH}")
	SET(ALL_MODULE_NAMES ${ALL_MODULE_NAMES} ${CURRENT_MODULE_NAME})
endforeach(CURRENT_MODULE ${ALL_MODULES})


SET_TARGET_PROPERTIES("nsclient++" ${ALL_MODULE_NAMES} PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/modules"
        MODULE_OUTPUT_DIRECTORY  "${PROJECT_BINARY_DIR}/modules"
        COMPILE_DEFINITIONS_DEBUG _DEBUG
)

if (MSVC_IDE)
    # hack to get around the "Debug" and "Release" directories cmake tries to add on Windows
    set_target_properties ("nsclient++" ${ALL_MODULE_NAMES} PROPERTIES PREFIX "../")
endif()

IF(CMAKE_COMPILER_IS_GNUCXX)
	IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
		SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC"  )
		SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG} -fPIC"  )
	ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)


