cmake_minimum_required(VERSION 3.10)

set(TARGET NSClientServer)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS ${SRCS} "${TARGET}.cpp" ${NSCP_INCLUDEDIR}/check_nt/packet.cpp
         ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp ${NSCP_DEF_PLUGIN_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})
if(OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIR})
  add_definitions(-DUSE_SSL)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
  if(WIN32)
    set(EXTRA_LIBS ${EXTRA_LIBS} CRYPT32)
  endif(WIN32)
endif(OPENSSL_FOUND)

if(WIN32)
  set(SRCS
      ${SRCS}
      "${TARGET}.h"
      "handler_impl.hpp"
      ${NSCP_INCLUDEDIR}/check_nt/server/protocol.hpp
      ${NSCP_INCLUDEDIR}/check_nt/server/parser.hpp
      ${NSCP_INCLUDEDIR}/check_nt/packet.hpp
      ${NSCP_INCLUDEDIR}/swap_bytes.hpp
      ${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
      ${NSCP_INCLUDEDIR}/socket/connection.hpp
      ${NSCP_INCLUDEDIR}/socket/server.hpp
      ${NSCP_DEF_PLUGIN_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})
openssl_link_fix(${TARGET})

target_link_libraries(
  ${TARGET} ${Boost_FILESYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB} ${EXTRA_LIBS})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Server" REGULAR_EXPRESSION .*include/check_nt/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)
