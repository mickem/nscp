cmake_minimum_required(VERSION 2.6)

PROJECT(NSCP CXX C)

ENABLE_TESTING()
IF(COMMAND CMAKE_POLICY)
CMAKE_POLICY(SET CMP0011 NEW)
ENDIF(COMMAND CMAKE_POLICY)

#############################################################################
#
# Setup cmake enviornment and include custom config overrides
#
#############################################################################
IF(NSCP_CMAKE_CONFIG)
	INCLUDE(${NSCP_CMAKE_CONFIG})
ELSE(NSCP_CMAKE_CONFIG)
	IF(EXISTS ${CMAKE_SOURCE_DIR}/build.cmake)
		MESSAGE(STATUS "Reading custom variables from: ${CMAKE_SOURCE_DIR}/build.cmake")
		INCLUDE(${CMAKE_SOURCE_DIR}/build.cmake)
	ELSEIF(EXISTS ${PROJECT_BINARY_DIR}/build.cmake)
		MESSAGE(STATUS "Reading custom variables from: ${PROJECT_BINARY_DIR}/build.cmake")
		INCLUDE(${PROJECT_BINARY_DIR}/build.cmake)
	ENDIF(EXISTS ${CMAKE_SOURCE_DIR}/build.cmake)
ENDIF(NSCP_CMAKE_CONFIG)
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build/cmake;${CMAKE_MODULE_PATH}")
SET(MODULE_SUBFOLDER "modules")

SET(BUILD_PYTHON_FOLDER "${CMAKE_SOURCE_DIR}/build/python")
SET(BUILD_CMAKE_FOLDER "${CMAKE_SOURCE_DIR}/build/cmake")
SET(BUILD_TARGET_EXE_PATH "${PROJECT_BINARY_DIR}")
SET(BUILD_TARGET_LIB_PATH "${BUILD_TARGET_EXE_PATH}/${MODULE_SUBFOLDER}")
SET(BUILD_TARGET_INSTLIB_PATH "${BUILD_TARGET_EXE_PATH}/installer-dlls")
SET(NSCP_PROJECT_BINARY_DIR ${PROJECT_BINARY_DIR})

INCLUDE(${BUILD_CMAKE_FOLDER}/SetMultiThread.cmake)
INCLUDE(${BUILD_CMAKE_FOLDER}/GoogleProtoBuf.cmake)
INCLUDE(${BUILD_CMAKE_FOLDER}/functions.cmake)

#############################################################################
#
# Find all dependencies and report anything missing.
#
#############################################################################
INCLUDE(${BUILD_CMAKE_FOLDER}/dependencies.cmake)

#############################################################################
#
# Validate all required dependencies and terminate on failures.
#
#############################################################################
MESSAGE(STATUS "Validating required dependencies:")
SET(HAVE_REQUIRED_DEPENDENCIES TRUE)
IF(NOT PYTHONINTERP_FOUND)
	SET(HAVE_REQUIRED_DEPENDENCIES FALSE)
	MESSAGE(WARNING " - python(exe) not found! (Required by the build process)")
ENDIF(NOT PYTHONINTERP_FOUND)
IF(NOT PROTOBUF_FOUND)
	SET(HAVE_REQUIRED_DEPENDENCIES FALSE)
	MESSAGE(WARNING " - protocol buffers not found! XXX=${PROTOBUF_LIBRARYDIR}! (Required by nsclient++)")
ENDIF(NOT PROTOBUF_FOUND)
IF(NOT Boost_FOUND)
	SET(HAVE_REQUIRED_DEPENDENCIES FALSE)
	MESSAGE(WARNING " - boost libraries was not found: ${Boost_ERROR_REASON} (Required by nsclient++)")
ENDIF(NOT Boost_FOUND)
IF(NOT WIN32)
	IF(NOT CMAKE_USE_PTHREADS_INIT)
		SET(HAVE_REQUIRED_DEPENDENCIES FALSE)
		MESSAGE(WARNING " - Could not find POSIX threads")
	ENDIF(NOT CMAKE_USE_PTHREADS_INIT)
ENDIF(NOT WIN32)

IF(NOT HAVE_REQUIRED_DEPENDENCIES)
	MESSAGE(FATAL_ERROR " - missing required dependencies found!")
ELSE(NOT HAVE_REQUIRED_DEPENDENCIES)
	MESSAGE(STATUS " - All required dependecies found! (some modules and features migh stil be missing though)")
ENDIF(NOT HAVE_REQUIRED_DEPENDENCIES)


#############################################################################
#
# Bump version number (if requested) and set up variables with current version
#
#############################################################################
SET(VERSION_TXT ${CMAKE_CURRENT_SOURCE_DIR}/version.txt)
IF("${INCREASE_BUILD}" STREQUAL "1")
EXECUTE_PROCESS(
	COMMAND ${PYTHON_EXECUTABLE}
		"${BUILD_PYTHON_FOLDER}/version.py" 
		--file ${VERSION_TXT} 
		--display 
		--update build
		--generate-hpp ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp
	OUTPUT_VARIABLE TMP_VERSION_OUT
	)
	MESSAGE(STATUS "===================== Bumping version")
ENDIF("${INCREASE_BUILD}" STREQUAL "1")

EXECUTE_PROCESS(
	COMMAND ${PYTHON_EXECUTABLE}
		"${BUILD_PYTHON_FOLDER}/version.py" 
		--file ${VERSION_TXT} 
		--display 
		--generate-hpp ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp
	OUTPUT_VARIABLE TMP_VERSION_OUT
	)
	
IF(TMP_VERSION_OUT STREQUAL "")
	MESSAGE(FATA_ERROR, "Failed to generate version from: ${PYTHON_EXECUTABLE} ${BUILD_PYTHON_FOLDER}/version.py --file ${VERSION_TXT} --display ")
ENDIF(TMP_VERSION_OUT STREQUAL "")
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1" VERSION_SERIES ${TMP_VERSION_OUT})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\2" VERSION_MAJOR ${TMP_VERSION_OUT})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\3" VERSION_MINOR ${TMP_VERSION_OUT})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\4" VERSION_BUILD ${TMP_VERSION_OUT})
MESSAGE(STATUS "Version: ${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}")
IF(WIN32)
	SET(VERSION_ARCH Win32)
	IF(CMAKE_CL_64)
		SET(VERSION_ARCH x64)
	ENDIF(CMAKE_CL_64)
ELSE(WIN32)
	SET(VERSION_ARCH Unknown)
ENDIF(WIN32)

#############################################################################
#
# Setup c/C++ build enviornment and flags
#
#############################################################################
SET(NSCP_INCLUDEDIR ${CMAKE_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR} ${CMAKE_CURRENT_BINARY_DIR}/libs)
SET(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH};${OPENSSL_LIBRARY_DIR};${NSCP_LIBRARYDIR};${BOOST_LIBRARYDIR}")

# This is used to disable "automatic linking on windows which seems to break since I dont know how to set link dir
SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DBOOST_ALL_NO_LIB)

SET_MULTITHREAD()

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-long-long -pedantic -Wno-unused-parameter")
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  message(STATUS "Added parallel build arguments to CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
endif()

IF(NOT SHARED_LIBS)
  IF(WIN32)
    OPTION(SHARED_LIBS "Compile shared libraries" OFF)
  ELSE(WIN32)
    OPTION(SHARED_LIBS "Compile shared libraries" ON)
  ENDIF(WIN32)
ENDIF(NOT SHARED_LIBS)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
INCLUDE_DIRECTORIES("${PROTOBUF_INCLUDE_DIR}")
INCLUDE_DIRECTORIES("${PROJECT_BINARY_DIR}")
add_definitions(-DUNICODE -D_UNICODE)

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

IF(CMAKE_COMPILER_IS_GNUCXX)
	IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
		SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC"  )
		SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC"  )
	ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

#SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DBOOST_FILESYSTEM_VERSION=2)
SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DUNICODE -D_UNICODE)
IF (WIN32)
	SET(NSCP_GLOBAL_DEFINES "${NSCP_GLOBAL_DEFINES} -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501 -DWINVER=0x0501")
ENDIF (WIN32)

IF(WIN32)
  SET(EXTRA_LIBS ${EXTRA_LIBS} Userenv ole32)
ENDIF(WIN32)

IF(CMAKE_HOST_UNIX)
	SET(UNIX_FLAVOUR LINUX CACHE STRING "Choose your unix flavour UNIX, AIX, HPUX" FORCE)
	IF(UNIX_FLAVOUR STREQUAL LINUX)
		SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DLINUX)
	ENDIF(UNIX_FLAVOUR STREQUAL LINUX)
	
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_DL_LIBS})
ENDIF()

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

LINK_DIRECTORIES(${CMAKE_LIBRARY_PATH} ${OPENSSL_LIBRARY_DIR} ${NSCP_LIBRARYDIR} ${BOOST_LIBRARYDIR})

# Compile time constants & make sure our build finds it
CONFIGURE_FILE(
  ${NSCP_INCLUDEDIR}/config.h.in
  ${NSCP_INCLUDEDIR}/config.h
  @ONLY
)
CONFIGURE_FILE(
  ${BUILD_PYTHON_FOLDER}/postbuild.py
  ${BUILD_TARGET_EXE_PATH}/postbuild.py
)
CONFIGURE_FILE(
  ${BUILD_PYTHON_FOLDER}/config.py
  ${BUILD_TARGET_EXE_PATH}/config.py
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${NSCP_LOCAL_INCLUDE_PATH})
INSTALL_FILES(/include FILES ${WCONFIG_H_PATH})

#############################################################################
#
# Build all included and required libraries
#
#############################################################################

SET(ALL_LIB_NAMES)
MESSAGE(STATUS "Adding libraries")
FILE(GLOB ALL_LIB_PROJECTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "libs/*/CMakeLists.txt")
FOREACH(CURRENT_LIB ${ALL_LIB_PROJECTS})
	GET_FILENAME_COMPONENT(CURRENT_LIB_PATH ${CURRENT_LIB} PATH)
	GET_FILENAME_COMPONENT(CURRENT_LIB_NAME ${CURRENT_LIB_PATH} NAME)
	MESSAGE(STATUS " + Library: ${CURRENT_LIB_PATH} (${CURRENT_LIB_NAME})")
	ADD_SUBDIRECTORY("${CURRENT_LIB_PATH}")
	SET(ALL_LIB_NAMES ${ALL_LIB_NAMES} ${CURRENT_LIB_NAME})
ENDFOREACH(CURRENT_LIB ${ALL_LIB_PROJECTS})
ADD_SUBDIRECTORY("helpers/settings_manager")
ADD_SUBDIRECTORY("helpers/where_filter")

#############################################################################
#
# Construct the bases for plugins
#
#############################################################################

SET(NSCP_DEF_PLUGIN_CPP)
SET(NSCP_DEF_PLUGIN_HPP
	${NSCP_INCLUDEDIR}/utf8.hpp
	${NSCP_INCLUDEDIR}/format.hpp
	${NSCP_INCLUDEDIR}/strEx.h
	${NSCP_INCLUDEDIR}/config.h
	${NSCP_INCLUDEDIR}/utils.h
	${NSCP_INCLUDEDIR}/NSCAPI.h
	${NSCP_INCLUDEDIR}/settings/client/settings_client.hpp
)

SET(NSCP_FILTER_CPP
	${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
)
SET(NSCP_FILTER_HPP
	${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.hpp
	${NSCP_INCLUDEDIR}/parsers/filter/realtime_helper.hpp
	${NSCP_INCLUDEDIR}/parsers/filter/cli_helper.hpp
	${NSCP_INCLUDEDIR}/parsers/where.hpp
)
SET(NSCP_FILTER_LIB
	where_filter
)

SET(NSCP_DEF_PLUGIN_LIB
	${CMAKE_THREAD_LIBS_INIT}
	${Boost_SYSTEM_LIBRARY}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${EXTRA_LIBS}
	${PROTOBUF_LIBRARY}
	protobuf
	plugin_api
)

#############################################################################
#
# Build everything (not already built lik libraries)!
#
#############################################################################

SET(MAIN_NSCP_TARGET "nscp" "nscp_full_config")
IF(BREAKPAD_FOUND)
	SET(MAIN_NSCP_TARGET ${MAIN_NSCP_TARGET})
ENDIF(BREAKPAD_FOUND)



ADD_SUBDIRECTORY("service")
ADD_SUBDIRECTORY("scripts")
ADD_SUBDIRECTORY("files")

LOAD_SECTIONS(ALL_MODULE_NAMES "modules/*/module.cmake" "Modules")
LOAD_SECTIONS(ALL_TOOL_NAMES "tools/*/module.cmake" "Tools")
LOAD_SECTIONS(ALL_INSTALLERS_DLLS_NAMES "helpers/installer-dlls/*/module.cmake" "Installer DLLs")
IF(WIN32)
	LOAD_SECTIONS(ALL_INSTALLERS_NAMES "helpers/installers/*/module.cmake" "Installers")
ENDIF(WIN32)
SET_TARGET_PROPERTIES(${MAIN_NSCP_TARGET} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES} PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${BUILD_TARGET_EXE_PATH}"
		LIBRARY_OUTPUT_DIRECTORY "${BUILD_TARGET_LIB_PATH}"
        MODULE_OUTPUT_DIRECTORY  "${BUILD_TARGET_LIB_PATH}"
        COMPILE_DEFINITIONS_DEBUG _DEBUG
)

SET_TARGET_PROPERTIES(${ALL_INSTALLERS_DLLS_NAMES} PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${BUILD_TARGET_EXE_PATH}"
		LIBRARY_OUTPUT_DIRECTORY "${BUILD_TARGET_INSTLIB_PATH}"
        MODULE_OUTPUT_DIRECTORY  "${BUILD_TARGET_INSTLIB_PATH}"
        COMPILE_DEFINITIONS_DEBUG _DEBUG
)

IF(MSVC_IDE)
    # hack to get around the "Debug" and "Release" directories cmake tries to add on Windows
#    SET_TARGET_PROPERTIES(${MAIN_NSCP_TARGET} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES} ${ALL_INSTALLERS_DLLS_NAMES} PROPERTIES PREFIX "../")
ENDIF(MSVC_IDE)

ADD_SUBDIRECTORY("docs")

#############################################################################
#
# CTest setup and add testcases
#
#############################################################################
if(WIN32)
	IF(CMAKE_CL_64)
		# Disabled for w32 due to linkage error with openssl from both python and nscp on x64 machines running w32
		add_nscp_py_test(nsca test_nsca)
		add_nscp_py_test(nrpe test_nrpe)
	ENDIF(CMAKE_CL_64)
ELSE(WIN32)
	add_nscp_py_test(nsca test_nsca)
	add_nscp_py_test(nrpe test_nrpe)
ENDIF(WIN32)
add_nscp_lua_test(lua_nrpe test_nrpe)
add_nscp_py_test(python test_python)
IF(WIN32)
# Disabled as is is not a unit test per see, since it requires extenal non mocked stimula
#	add_nscp_py_test(eventlog test_eventlog)
	add_nscp_py_test(w32_system test_w32_system)
ENDIF(WIN32)

#############################################################################
#
# CPacke setup (currently not used)
#
#############################################################################
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "NSClient++")
SET(CPACK_PACKAGE_VENDOR "nsclient.org")
#SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/docs/readme.txt")
#SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/Copyright.txt")

SET(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_SERIES}.${VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${VERSION_BUILD})

SET(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")
IF(WIN32 AND NOT UNIX)
	SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
	SET(CPACK_GENERATOR ZIP)
ELSE(WIN32 AND NOT UNIX)
	SET(CPACK_STRIP_FILES "bin/MyExecutable")
	SET(CPACK_SOURCE_STRIP_FILES "")
	SET(CPACK_PACKAGE_EXECUTABLES "nscp")
ENDIF(WIN32 AND NOT UNIX)
INCLUDE(CPack)

