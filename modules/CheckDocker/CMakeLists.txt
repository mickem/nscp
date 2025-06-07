cmake_minimum_required(VERSION 3.10)

set(TARGET CheckDocker)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})


set(SRCS ${SRCS} "${TARGET}.cpp" 
    check_docker.cpp 
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
)

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
  set(SRCS ${SRCS} "${TARGET}.h" check_docker.hpp ${NSCP_DEF_PLUGIN_HPP}
           ${NSCP_FILTER_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})
openssl_link_fix(${TARGET})

target_link_libraries(${TARGET} ${NSCP_DEF_PLUGIN_LIB} ${NSCP_FILTER_LIB}
                      expression_parser ${JSON_LIB} ${EXTRA_LIBS})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
