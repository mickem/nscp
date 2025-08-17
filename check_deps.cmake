cmake_minimum_required(VERSION 3.10)

if(COMMAND CMAKE_POLICY)
    cmake_policy(SET CMP0011 NEW)
endif(COMMAND CMAKE_POLICY)

set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")
set(CMAKE_FIND_LIBRARY_SUFFIXES .lib)
#############################################################################
#
# Setup cmake enviornment and include custom config overrides
#
#############################################################################
if(EXISTS ${TARGET}/build.cmake)
    message(STATUS "Reading custom variables from: ${TARGET}/build.cmake")
    include(${TARGET}/build.cmake)
endif(EXISTS ${TARGET}/build.cmake)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_MODULE_PATH "${SOURCE}/build/cmake;${CMAKE_MODULE_PATH}")

set(BUILD_PYTHON_FOLDER "${SOURCE}/build/python")
set(BUILD_CMAKE_FOLDER "${SOURCE}/build/cmake")
set(NSCP_PROJECT_BINARY_DIR ${TARGET})

#############################################################################
#
# Find all dependencies and report anything missing.
#
#############################################################################
include(${BUILD_CMAKE_FOLDER}/dependencies.cmake)
