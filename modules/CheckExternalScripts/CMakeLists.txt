cmake_minimum_required(VERSION 3.5)

set(TARGET CheckExternalScripts)

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
set(SRCS ${SRCS} "${TARGET}.cpp" extscr_cli.cpp script_provider.cpp
         ${NSCP_DEF_PLUGIN_CPP} ${NSCP_ERROR_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
  set(SRCS
      ${SRCS}
      "${TARGET}.h"
      commands.hpp
      alias.hpp
      extscr_cli.h
      script_provider.hpp
      script_interface.hpp
      ${NSCP_INCLUDEDIR}/process/execute_process.hpp
      ${NSCP_INCLUDEDIR}/win_sysinfo/win_sysinfo.cpp
      ${NSCP_INCLUDEDIR}/win_sysinfo/win_sysinfo.hpp
      ${NSCP_DEF_PLUGIN_HPP}
      ${NSCP_ERROR_HPP})
  set(SRCS ${SRCS} ${NSCP_INCLUDEDIR}/process/execute_process_w32.cpp)
else(WIN32)
  set(EXTRA_LIBS ${EXTRA_LIBS} pthread)
  set(SRCS ${SRCS} ${NSCP_INCLUDEDIR}/process/execute_process_unix.cpp)
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(
  ${TARGET} ${Boost_FILESYSTEM_LIBRARY} ${Boost_REGEX_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB} ${JSON_LIB} ${EXTRA_LIBS})

include(${BUILD_CMAKE_FOLDER}/module.cmake)
