cmake_minimum_required(VERSION 3.5)

set(TARGET SimpleFileWriter)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS ${SRCS} "${TARGET}.cpp" ${NSCP_DEF_PLUGIN_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
  set(SRCS ${SRCS} "${TARGET}.h" ${NSCP_DEF_PLUGIN_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(
  ${TARGET} ${Boost_FILESYSTEM_LIBRARY} ${NSCP_DEF_PLUGIN_LIB}
  ${Boost_THREAD_LIBRARY} expression_parser)

include(${BUILD_CMAKE_FOLDER}/module.cmake)
