cmake_minimum_required(VERSION 2.8.12)

IF(COMMAND CMAKE_POLICY)
CMAKE_POLICY(SET CMP0011 NEW)
ENDIF(COMMAND CMAKE_POLICY)

set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")
set (CMAKE_FIND_LIBRARY_SUFFIXES .lib)
#############################################################################
#
# Setup cmake enviornment and include custom config overrides
#
#############################################################################
IF(EXISTS ${TARGET}/build.cmake)
	MESSAGE(STATUS "Reading custom variables from: ${TARGET}/build.cmake")
	INCLUDE(${TARGET}/build.cmake)
ENDIF(EXISTS ${TARGET}/build.cmake)
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
SET(CMAKE_MODULE_PATH "${SOURCE}/build/cmake;${CMAKE_MODULE_PATH}")

SET(BUILD_PYTHON_FOLDER "${SOURCE}/build/python")
SET(BUILD_CMAKE_FOLDER "${SOURCE}/build/cmake")
SET(NSCP_PROJECT_BINARY_DIR ${TARGET})

#############################################################################
#
# Find all dependencies and report anything missing.
#
#############################################################################
INCLUDE(${BUILD_CMAKE_FOLDER}/dependencies.cmake)
