cmake_minimum_required(VERSION 3.10)

set(TARGET CheckSystem)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    CheckSystem.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    realtime_thread.cpp
    realtime_data.cpp
    filter_config_object.cpp
    check_uptime.cpp
    check_os_version.cpp
    check_memory.cpp
    check_cpu.cpp
    check_service.cpp
    check_process.cpp
    check_pagefile.cpp
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        CheckSystem.h
        realtime_thread.hpp
        realtime_data.hpp
        filter_config_object.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
        check_uptime.h
        check_os_version.h
        check_memory.h
        check_cpu.h
        check_service.h
        check_process.h
        check_pagefile.h
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
