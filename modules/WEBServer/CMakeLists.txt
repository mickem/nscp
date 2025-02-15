cmake_minimum_required(VERSION 3.5)

set(TARGET WEBServer)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

if(JSON_SPIRIT_FOUND)
  include_directories(${JSON_SPIRIT_INCLUDE_DIR})
  add_definitions(-DHAVE_JSON_SPIRIT)
  add_definitions(-DJSON_SPIRIT_STATIC_LINKED)
  set(JSON_LIB json_spirit)
else(JSON_SPIRIT_FOUND)
  set(JSON_LIB)
endif(JSON_SPIRIT_FOUND)

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    static_controller.cpp
    token_store.cpp
    grant_store.cpp
    session_manager_interface.cpp
    metrics_handler.cpp
    error_handler.cpp
    modules_controller.cpp
    query_controller.cpp
    scripts_controller.cpp
    legacy_command_controller.cpp
    legacy_controller.cpp
    api_controller.cpp
    log_controller.cpp
    info_controller.cpp
    settings_controller.cpp
    login_controller.cpp
    metrics_controller.cpp
    openmetrics_controller.cpp
    ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/client/simple_client.cpp
    ${NSCP_INCLUDEDIR}/metrics/metrics_store_map.cpp
    ${NSCP_DEF_PLUGIN_CPP})

add_definitions(-DMG_ENABLE_HTTP_WEBSOCKET=0)
add_definitions(-DMG_ENABLE_BROADCAST=1)
add_definitions(-DMG_ENABLE_THREADS)
add_definitions(${NSCP_GLOBAL_DEFINES})
if(OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIR})
  add_definitions(-DMG_ENABLE_OPENSSL=1)
  add_definitions(-DMG_DISABLE_PFS=1)
  add_definitions(-DUSE_SSL)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
  if(WIN32)
    set(EXTRA_LIBS ${EXTRA_LIBS} CRYPT32)
  endif(WIN32)
endif(OPENSSL_FOUND)
include_directories(../../libs/mongoose-cpp)

if(WIN32)
  set(SRCS
      ${SRCS}
      "${TARGET}.h"
      ${NSCP_INCLUDEDIR}/client/simple_client.hpp
      ${NSCP_INCLUDEDIR}/metrics/metrics_store_map.hpp
      ${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
      info_controller.hpp
      settings_controller.hpp
      metrics_controller.hpp
      openmetrics_controller.hpp
      ${NSCP_DEF_PLUGIN_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})
openssl_link_fix(${TARGET})

target_link_libraries(
  ${TARGET}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_DATE_TIME_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB}
  ${EXTRA_LIBS}
  ${JSON_LIB}
  nscp_mongoose)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
