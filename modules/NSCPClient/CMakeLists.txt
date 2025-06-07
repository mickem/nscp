cmake_minimum_required(VERSION 3.10)

set(TARGET NSCPClient)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS ${SRCS} "${TARGET}.cpp" ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
         ${NSCP_DEF_PLUGIN_CPP} ${NSCP_CLIENT_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})
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
      nscp_client.hpp
      nscp_handler.hpp
      ${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
      ${NSCP_INCLUDEDIR}/socket/client.hpp
      ${NSCP_DEF_PLUGIN_HPP}
      ${NSCP_CLIENT_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})
openssl_link_fix(${TARGET})

target_link_libraries(
  ${TARGET} ${Boost_FILESYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB} ${EXTRA_LIBS})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)
