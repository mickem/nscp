SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS On)

cmake_minimum_required(VERSION 2.8.0)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build/cmake;${CMAKE_MODULE_PATH}")
	
PROJECT(NSCP CXX C)

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
SET(BOOST_INCLUDEDIR "${INC_BOOST_INCLUDEDIR}" CACHE PATH "Path to boost includes")
SET(BOOST_LIBRARYDIR "${NSCP_LIBRARYDIR}" CACHE PATH "Path to boost libraries")
# ### ### ### ###
# Open SSL
SET(OPENSSL_INCLUDE_DIR "${INC_OPENSSL_INCLUDEDIR}" CACHE PATH "Path to openssl includes")
SET(OPENSSL_LIBRARY_DIR "${NSCP_LIBRARYDIR}" CACHE PATH "Path to openssl libraries")
# ### ### ### ###
# Google Protocol buffer
SET(PROTOBUF_DIR "${INC_PROTOBUF_DIR}" CACHE PATH "Path to google protocol buffer root")
IF(WIN32 AND MSVC)
	SET(PROTOBUF_INCLUDE_DIR "${PROTOBUF_DIR}/src" "Path to google protocol buffer source")
	SET(PROTOBUF_LIBRARYDIR "${INC_PROTOBUF_LIBRARYDIR}")
	SET(PROTOBUF_LIBRARY_PREFIX "")
	SET(PROTOBUF_LIBRARY_PREFIX_DEBUG "debug-")
	SET(PROTOBUF_BINARYDIR "${PROTOBUF_DIR}/vsprojects/debug")
ELSE(WIN32 AND MSVC)
# TODO
ENDIF(WIN32 AND MSVC)
# ### ### ### ###
# Crypto++
SET(CRYPTOPP_DIR "${INC_CRYPTOPP_DIR}" CACHE PATH "Path to crypto++ root folder")
# ### ### ### ###
# Lua
FIND_PATH(LUA_INCLUE_DIR lua.h
	${INC_LUA_DIR}
	${INC_LUA_DIR}/src
	${LUA_DIR}
	${LUA_DIR}/src
	/usr/include/lua
)
# ### ### ### ###
# Google breakpad
SET(GOOGLE_BREAKPAD_DIR "${INC_GOOGLE_BREAKPAD_DIR}" CACHE PATH "Path to google protocol breakpad root")
IF(WIN32 AND MSVC)
	SET(BREAKPAD_LIBRARY_PREFIX "google-breakpad-")
	SET(BREAKPAD_LIBRARY_PREFIX_DEBUG "debug-google-breakpad-")
	SET(BREAKPAD_INCLUDE_DIR "${GOOGLE_BREAKPAD_DIR}/src")
ELSE(WIN32 AND MSVC)
# TODO
ENDIF(WIN32 AND MSVC)

# ### ### ### ###
# Build related paths
SET(BUILD_PYTHON_FOLDER "${CMAKE_SOURCE_DIR}/build/python")
SET(BUILD_CMAKE_FOLDER "${CMAKE_SOURCE_DIR}/build/cmake")

# ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ###


INCLUDE(${BUILD_CMAKE_FOLDER}/SetMultiThread.cmake)
INCLUDE(${BUILD_CMAKE_FOLDER}/NSCPPBuffer.cmake)

SET(VERSION_TXT ${CMAKE_CURRENT_SOURCE_DIR}/version.txt)

execute_process(
	COMMAND ${PYTHON_EXECUTABLE}
		"${BUILD_PYTHON_FOLDER}/version.py"
		--file ${VERSION_TXT}
		--display
	OUTPUT_VARIABLE TMP_VERSION_OUT
	)
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1" VERSION_SERIES ${TMP_VERSION_OUT})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\2" VERSION_MAJOR ${TMP_VERSION_OUT})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\3" VERSION_MINOR ${TMP_VERSION_OUT})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\4" VERSION_BUILD ${TMP_VERSION_OUT})
MESSAGE(STATUS "Version: ${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}")

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "NSClient++")
SET(CPACK_PACKAGE_VENDOR "nsclient.org")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/docs/readme.txt")
#SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/Copyright.txt")

SET(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_SERIES})
SET(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${VERSION_MINOR})

SET(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")
IF(WIN32 AND NOT UNIX)
	SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
	SET(CPACK_GENERATOR ZIP)
ELSE(WIN32 AND NOT UNIX)
	SET(CPACK_STRIP_FILES "bin/MyExecutable")
	SET(CPACK_SOURCE_STRIP_FILES "")
	SET(CPACK_PACKAGE_EXECUTABLES "nscp" "My Executable")
ENDIF(WIN32 AND NOT UNIX)
INCLUDE(CPack)



INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR})
SET(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH};${OPENSSL_LIBRARY_DIR};${NSCP_LIBRARYDIR};${BOOST_LIBRARYDIR}")

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


IF(WIN32)
  SET(LIB_INSTALL_DIR "lib" CACHE STRING "Default path for libraries within ${CMAKE_INSTALL_PREFIX}")
  SET(RUNDIR "c:/nscp" CACHE PATH "Not really used in WIN32")
ELSE(WIN32)
  SET(USERLIB_ROOT /usr CACHE PATH "Other installation prefix for dependent libraries")
  SET(LIB_INSTALL_DIR "lib" CACHE STRING "Default path for libraries within ${CMAKE_INSTALL_PREFIX}")
  SET(RUNDIR "/usr/wt/run" CACHE PATH "Default path for wt session management (only used by FCGI connector; not relative to CMAKE_INSTALL_PREFIX)")
  SET(CONFIGURATION /etc/nscp/boot.ini CACHE PATH "Path to the boot configuration file")
  SET(MAIN_CONFIGURATION /etc/nscp/nscp.xml CACHE PATH "Path for the client configurtion file")
  SET(DEPLOYROOT /usr/bin CACHE PATH "Path to deploy examples into)")
ENDIF(WIN32)

FIND_PACKAGE(GoogleBreakpad COMPONENTS common exception_handler crash_report_sender crash_generation_client)
if(BREAKPAD_FOUND)
	MESSAGE(STATUS "Found google breakpad")
	SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DUSE_BREAK_PAD)
else(BREAKPAD_FOUND)
	MESSAGE(WARNING "Breakpad was not found in: ${BREAKPAD_EXCEPTION_HANDLER_INCLUDE_DIR}/${BREAKPAD_EXCEPTION_HANDLER_LIBRARIES}")
endif(BREAKPAD_FOUND)

FIND_PACKAGE(OpenSSL)
if(OPENSSL_FOUND)
	MESSAGE(STATUS "Found OpenSSL")
	SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DUSE_SSL)
else(OPENSSL_FOUND)
	MESSAGE(WARNING " OpenSSL NOT found (no ssl support, ${OPENSSL_INCLUDE_DIR})")
endif(OPENSSL_FOUND)

FIND_PACKAGE(Boost COMPONENTS system filesystem thread regex date_time program_options REQUIRED)
if(Boost_FOUND)
	MESSAGE(STATUS "Found boost in: ${Boost_INCLUDE_DIRS} / ${Boost_LIBRARY_DIRS}")
	# Boost is used nearly everywhere, so we can put these here
	INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
	#LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
	IF(WIN32)
	  #IF(BOOST_DYNAMIC)
		#ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)
	  #ENDIF(BOOST_DYNAMIC)
	  SET(EXTRA_LIBS ${EXTRA_LIBS} Userenv ole32)
	ENDIF(WIN32)
else(Boost_FOUND)
	MESSAGE(FATAL_ERROR, "boost NOT fround: ${Boost_ERROR_REASON}")
endif(Boost_FOUND)

if(PROTOBUF_FOUND)
	message(STATUS "Found proto in: ${PROTOBUF_LIBRARY} (${PROTOBUF_PROTOC_EXECUTABLE})")
	INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIR})
else(PROTOBUF_FOUND)
	message(FATAL_ERROR, "Protocol buffers NOT found (${PROTOBUF_LIBRARYDIR}, )")
endif(PROTOBUF_FOUND)

IF(WIN32)
	FIND_PATH(PSDK_61_INCLUDE_DIRS Windows.h
		${INC_PSDK_61}
		${INC_PSDK_61}/include
		"C:/Program Files/Microsoft SDKs/Windows/v6.1/include"
	)

	if(CMAKE_CL_64)
		FIND_PATH(PSDK_61_LIBRARY_DIRS WSock32.Lib
			${INC_PSDK_61}/lib/x64
			"C:/Program Files/Microsoft SDKs/Windows/v6.1/lib/x64"
		)
	else(CMAKE_CL_64)
		FIND_PATH(PSDK_61_LIBRARY_DIRS WS2_32.Lib
			${INC_PSDK_61}/lib
			"C:/Program Files/Microsoft SDKs/Windows/v6.1/lib"
		)
	endif(CMAKE_CL_64)
	if(PSDK_61_INCLUDE_DIRS AND PSDK_61_LIBRARY_DIRS)
		SET(PSDK_61_FOUND TRUE)
		message(STATUS "Found PSDK 6.1 (${CMAKE_SIZEOF_VOID_P}): ${PSDK_61_INCLUDE_DIRS} (${PSDK_61_LIBRARY_DIRS})")
	else()
		SET(PSDK_61_FOUND FALSE)
		message(STATUS "PSDK 6.1 NOT FOUND(${CMAKE_SIZEOF_VOID_P}): ${PSDK_61_INCLUDE_DIRS} (${PSDK_61_LIBRARY_DIRS})")
	endif()
endif(WIN32)

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

FIND_PACKAGE(PythonInterp)
IF (NOT PYTHONINTERP_FOUND)
	MESSAGE(FATAL_ERROR "Could not find python which is required for building")
ELSE (NOT PYTHONINTERP_FOUND)
	message(STATUS "Found python in: ${PYTHON_EXECUTABLE}")
ENDIF (NOT PYTHONINTERP_FOUND)


SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0403)
# TODO: Fix this issue! (crypto++ and masm)
IF (WIN32)
	SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DCRYPTOPP_DISABLE_ASM)
ENDIF (WIN32)


IF(CMAKE_HOST_UNIX)
	SET(UNIX_FLAVOUR LINUX CACHE STRING "Choose your unix flavour UNIX, AIX, HPUX" FORCE)
	IF(UNIX_FLAVOUR STREQUAL LINUX)
		SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DLINUX)
	ENDIF(UNIX_FLAVOUR STREQUAL LINUX)
	
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_DL_LIBS})
ENDIF()

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
	${NSCP_INCLUDEDIR}/settings/client/settings_client.cpp
)

SET(NSCP_DEF_PLUGIN_HPP
	${NSCP_INCLUDEDIR}/strEx.h
	${NSCP_INCLUDEDIR}/config.h
	${NSCP_INCLUDEDIR}/utils.h
	${NSCP_INCLUDEDIR}/NSCAPI.h
#	${NSCP_INCLUDEDIR}/nscapi/macros.hpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_wrapper.hpp	
	${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.hpp
	${NSCP_INCLUDEDIR}/settings/client/settings_client.hpp
	${NSCP_INCLUDEDIR}/nscapi/settings_proxy.hpp
	${NSCP_INCLUDEDIR}/nscapi/macros.hpp
)

SET(NSCP_FILTER_CPP
	${NSCP_INCLUDEDIR}/parsers/helpers.cpp
)

SET(NSCP_FILTER_HPP
	${NSCP_INCLUDEDIR}/parsers/ast.hpp
	${NSCP_INCLUDEDIR}/parsers/eval.hpp
	${NSCP_INCLUDEDIR}/parsers/helpers.hpp
	${NSCP_INCLUDEDIR}/parsers/where.hpp
	${NSCP_INCLUDEDIR}/parsers/where_parser.hpp
	
	${NSCP_INCLUDEDIR}/parsers/filter/where_filter.hpp
	${NSCP_INCLUDEDIR}/parsers/filter/where_filter_impl.hpp	
	
	${NSCP_INCLUDEDIR}/parsers/where/ast_bind.hpp
	${NSCP_INCLUDEDIR}/parsers/where/ast_static_eval.hpp
	${NSCP_INCLUDEDIR}/parsers/where/ast_type_inference.hpp
	${NSCP_INCLUDEDIR}/parsers/where/ast_visitors.hpp
	${NSCP_INCLUDEDIR}/parsers/where/expression_ast.hpp
	${NSCP_INCLUDEDIR}/parsers/where/expression_ast_impl.hpp
	${NSCP_INCLUDEDIR}/parsers/where/list_value.hpp
	${NSCP_INCLUDEDIR}/parsers/where/operators.hpp
	${NSCP_INCLUDEDIR}/parsers/where/operators_impl.hpp
	${NSCP_INCLUDEDIR}/parsers/where/unary_fun.hpp
	${NSCP_INCLUDEDIR}/parsers/where/variable.hpp
	${NSCP_INCLUDEDIR}/parsers/where/varible_handler.hpp
	
	${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar.hpp
	${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar_impl.hpp	
)

SET(NSCP_DEF_PLUGIN_LIB
	${Boost_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${EXTRA_LIBS}
	${PROTOBUF_LIBRARY}
	protobuf
)

message(STATUS "NSCP PLUGIN LIB: ${NSCP_DEF_PLUGIN_LIB}")
message(STATUS "NSCP PLUGIN CPP: ${NSCP_DEF_PLUGIN_CPP}")
	
LINK_DIRECTORIES(${CMAKE_LIBRARY_PATH} ${OPENSSL_LIBRARY_DIR} ${NSCP_LIBRARYDIR} ${BOOST_LIBRARYDIR})

SET(ALL_LIB_NAMES)
message(STATUS "Adding libraries")
FILE(GLOB ALL_LIB_PROJECTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "libs/*/CMakeLists.txt")
foreach(CURRENT_LIB ${ALL_LIB_PROJECTS})
	get_filename_component(CURRENT_LIB_PATH ${CURRENT_LIB} PATH)
	get_filename_component(CURRENT_LIB_NAME ${CURRENT_LIB_PATH} NAME)
	message(STATUS " + Library: ${CURRENT_LIB_PATH} (${CURRENT_LIB_NAME})")
	ADD_SUBDIRECTORY("${CURRENT_LIB_PATH}")
	SET(ALL_LIB_NAMES ${ALL_LIB_NAMES} ${CURRENT_LIB_NAME})
endforeach(CURRENT_LIB ${ALL_LIB_PROJECTS})

# Compile time constants & make sure our build finds it
CONFIGURE_FILE(
  ${NSCP_INCLUDEDIR}/config.h.in
  ${NSCP_INCLUDEDIR}/config.h
  @ONLY
)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${NSCP_LOCAL_INCLUDE_PATH})
INSTALL_FILES(/include FILES ${WCONFIG_H_PATH})

add_definitions(-DUNICODE -D_UNICODE)

include_directories("${PROJECT_BINARY_DIR}")

MESSAGE(STATUS "Build path: ${PROJECT_BINARY_DIR}")
ADD_SUBDIRECTORY("service")
SET(MAIN_NSCP_TARGET "nscp")
MESSAGE(STATUS "NSCP Target: ${MAIN_NSCP_TARGET}")

ADD_SUBDIRECTORY("scripts")

SET(ALL_MODULE_NAMES)
SET(BUILD_MODULE 0)
SET(BUILD_MODULE_SKIP_REASON "")
FILE(GLOB ALL_MODULES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "modules/*/module.cmake")
foreach(CURRENT_MODULE ${ALL_MODULES})
	get_filename_component(CURRENT_MODULE_PATH ${CURRENT_MODULE} PATH)
	get_filename_component(CURRENT_MODULE_NAME ${CURRENT_MODULE_PATH} NAME)
	SET (BUILD_MODULE 0)
	include(${CURRENT_MODULE})
	IF(BUILD_MODULE)
		message(STATUS "+ Module ${CURRENT_MODULE_NAME}: Added")
		ADD_SUBDIRECTORY("${CURRENT_MODULE_PATH}")
		SET(ALL_MODULE_NAMES ${ALL_MODULE_NAMES} ${CURRENT_MODULE_NAME})
	ELSE(BUILD_MODULE)
		message(STATUS "- Module ${CURRENT_MODULE_NAME}: Skipped ${BUILD_MODULE_SKIP_REASON}")
	ENDIF(BUILD_MODULE)
endforeach(CURRENT_MODULE ${ALL_MODULES})

SET(ALL_TOOL_NAMES)
FILE(GLOB ALL_TOOLS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "tools/*/module.cmake")
foreach(CURRENT_MODULE ${ALL_TOOLS})
	get_filename_component(CURRENT_MODULE_PATH ${CURRENT_MODULE} PATH)
	get_filename_component(CURRENT_MODULE_NAME ${CURRENT_MODULE_PATH} NAME)
	SET (BUILD_MODULE 0)
	include(${CURRENT_MODULE})
	IF(BUILD_MODULE)
		message(STATUS "+ Tool ${CURRENT_MODULE_NAME}: Added")
		ADD_SUBDIRECTORY("${CURRENT_MODULE_PATH}")
		SET(ALL_TOOL_NAMES ${ALL_TOOL_NAMES} ${CURRENT_MODULE_NAME})
	ELSE(BUILD_MODULE)
		message(STATUS "- Tool ${CURRENT_MODULE_NAME}: Skipped ${BUILD_MODULE_SKIP_REASON}")
	ENDIF(BUILD_MODULE)
endforeach(CURRENT_MODULE ${ALL_TOOLS})


SET_TARGET_PROPERTIES(${MAIN_NSCP_TARGET} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES} PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/modules"
        MODULE_OUTPUT_DIRECTORY  "${PROJECT_BINARY_DIR}/modules"
        COMPILE_DEFINITIONS_DEBUG _DEBUG
)

if (MSVC_IDE)
    # hack to get around the "Debug" and "Release" directories cmake tries to add on Windows
    set_target_properties(${MAIN_NSCP_TARGET} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES} PROPERTIES PREFIX "../")
endif()

IF(CMAKE_COMPILER_IS_GNUCXX)
	IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
		SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC"  )
		SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG} -fPIC"  )
	ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)
