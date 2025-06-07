cmake_minimum_required(VERSION 3.10)

set(TARGET CheckMKServer)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS} "${TARGET}.cpp" "handler_impl.cpp"
    ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/check_mk/lua/lua_check_mk.cpp ${NSCP_DEF_PLUGIN_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})
include_directories(${LUA_INCLUDE_DIR})
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
      ${NSCP_INCLUDEDIR}/check_mk/server/server_protocol.hpp
      ${NSCP_INCLUDEDIR}/check_mk/server/server_handler.hpp
      ${NSCP_INCLUDEDIR}/check_mk/parser.hpp
      ${NSCP_INCLUDEDIR}/check_mk/data.hpp
      ${NSCP_INCLUDEDIR}/check_mk/lua/lua_check_mk.hpp
      ${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
      ${NSCP_DEF_PLUGIN_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})
openssl_link_fix(${TARGET})

set(LUA_PB)
if(PROTOC_GEN_LUA_FOUND)
  set(LUA_PB nscp_lua_pb)
endif()

target_link_libraries(
  ${TARGET}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB}
  ${EXTRA_LIBS}
  ${LUA_LIB}
  lua_nscp)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Server" REGULAR_EXPRESSION .*include/check_mk/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)
