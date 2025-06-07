cmake_minimum_required(VERSION 3.10)

set(TARGET CheckDisk)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_drive.cpp
    file_finder.cpp
    filter.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
    ${NSCP_INCLUDEDIR}/compat.cpp)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
  set(SRCS
      ${SRCS}
      "${TARGET}.h"
      check_drive.hpp
      file_finder.hpp
      filter.hpp
      ${NSCP_DEF_PLUGIN_HPP}
      ${NSCP_FILTER_HPP}
      ${NSCP_ERROR_HPP}
      ${NSCP_INCLUDEDIR}/compat.hpp)
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(
  ${TARGET}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  ${Boost_DATE_TIME_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB}
  ${NSCP_FILTER_LIB}
  expression_parser
  version.lib)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
