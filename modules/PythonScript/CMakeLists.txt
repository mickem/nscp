cmake_minimum_required(VERSION 3.5)

set(TARGET PythonScript)

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
    python_script.cpp
    script_wrapper.cpp
    extscr_cli.cpp
    script_provider.cpp
    ${NSCP_DEF_PLUGIN_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES} -DBOOST_PYTHON_STATIC_LIB)

if(WIN32)
  set(SRCS
      ${SRCS}
      script_wrapper.hpp
      "${TARGET}.h"
      extscr_cli.h
      script_interface.hpp
      python_script.hpp
      script_provider.hpp
      ${NSCP_DEF_PLUGIN_HPP})
endif(WIN32)
include_directories(${Python3_INCLUDE_DIRS})

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(
  ${TARGET}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_PYTHON311_LIBRARY}
  ${NSCP_DEF_PLUGIN_LIB}
  ${Python3_LIBRARIES}
  ${JSON_LIB})

include(${BUILD_CMAKE_FOLDER}/module.cmake)
