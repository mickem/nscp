cmake_minimum_required(VERSION 3.5)

set(TARGET CollectdClient)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS} "${TARGET}.cpp" ${NSCP_INCLUDEDIR}/collectd/collectd_packet.cpp
    ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
  set(SRCS
      ${SRCS}
      "${TARGET}.h"
      collectd_client.hpp
      collectd_handler.hpp
      ${NSCP_INCLUDEDIR}/collectd/collectd_packet.hpp
      ${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
      ${NSCP_INCLUDEDIR}/socket/client.hpp
      ${NSCP_DEF_PLUGIN_HPP}
      ${NSCP_CLIENT_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(
  ${TARGET}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB}
  ${EXTRA_LIBS}
  expression_parser)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Client" REGULAR_EXPRESSION .*include/collectd/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)
