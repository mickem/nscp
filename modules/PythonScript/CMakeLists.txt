cmake_minimum_required(VERSION 3.5)

set(TARGET PythonScript)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    python_script.cpp
    script_wrapper.cpp
    extscr_cli.cpp
    script_provider.cpp
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
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
      ${NSCP_DEF_PLUGIN_HPP}
  )
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
  ${Python3_LIBRARIES})

include(${BUILD_CMAKE_FOLDER}/module.cmake)
