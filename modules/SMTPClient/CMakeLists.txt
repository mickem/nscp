cmake_minimum_required(VERSION 3.5)

set(TARGET SMTPClient)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS} "${TARGET}.cpp" smtp.cpp
    ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
  set(SRCS
      ${SRCS}
      "${TARGET}.h"
      smtp_client.hpp
      smtp_handler.hpp
      smtp.hpp
      ${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
      ${NSCP_DEF_PLUGIN_HPP}
      ${NSCP_CLIENT_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(${TARGET} ${Boost_FILESYSTEM_LIBRARY}
                      ${Boost_PROGRAM_OPTIONS_LIBRARY} ${NSCP_DEF_PLUGIN_LIB})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Server" REGULAR_EXPRESSION .*include/smtp/.*)
