cmake_minimum_required(VERSION 3.5)

set(TARGET CheckDocker)

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

set(SRCS ${SRCS} "${TARGET}.cpp" check_docker.cpp ${NSCP_DEF_PLUGIN_CPP}
         ${NSCP_FILTER_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
  set(SRCS ${SRCS} "${TARGET}.h" check_docker.hpp ${NSCP_DEF_PLUGIN_HPP}
           ${NSCP_FILTER_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(${TARGET} ${NSCP_DEF_PLUGIN_LIB} ${NSCP_FILTER_LIB}
                      expression_parser ${JSON_LIB})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
