cmake_minimum_required(VERSION 3.10)

set(TARGET Op5Client)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS ${SRCS}
        "${TARGET}.cpp"
        op5_client.cpp
        ${NSCP_DEF_PLUGIN_CPP}
        ${NSCP_CLIENT_CPP}
        ${NSCP_INCLUDEDIR}/json/use_json.cpp)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
  set(SRCS ${SRCS} "${TARGET}.h" op5_client.hpp ${NSCP_DEF_PLUGIN_HPP}
           ${NSCP_CLIENT_HPP})
endif(WIN32)

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

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(
  ${TARGET}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB}
  nscp_mongoose)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
