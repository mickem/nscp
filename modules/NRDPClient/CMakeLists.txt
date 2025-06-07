cmake_minimum_required(VERSION 3.10)

set(TARGET NRDPClient)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    nrdp.cpp
    ${TINYXML2_INCLUDE_DIR}/tinyxml2.cpp
    ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})
include_directories(${TINYXML2_INCLUDE_DIR})
if(OPENSSL_FOUND)
  add_definitions(-DUSE_SSL)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
  if(WIN32)
    set(EXTRA_LIBS ${EXTRA_LIBS} CRYPT32)
  endif(WIN32)
  include_directories(${OPENSSL_INCLUDE_DIR})
endif(OPENSSL_FOUND)

if(WIN32)
  set(SRCS
      ${SRCS}
      "${TARGET}.h"
      nrdp.hpp
      nrdp_client.hpp
      nrdp_handler.hpp
      ${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
      ${TINYXML2_INCLUDE_DIR}/tinyxml2.h
      ${NSCP_DEF_PLUGIN_HPP}
      ${NSCP_CLIENT_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(
  ${TARGET} ${Boost_FILESYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB} ${EXTRA_LIBS})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Server" REGULAR_EXPRESSION .*include/nrdp/.*)
