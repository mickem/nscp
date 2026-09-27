cmake_minimum_required(VERSION 3.10)

set(TARGET CheckTaskSched)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    TaskSched.cpp
    task_facts.cpp
    filter.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        TaskSched.h
        filter.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
        ${NSCP_ERROR_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
    Mstask.lib
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    check_tasksched_test
    SOURCES
        task_facts_win_test.cpp
        TaskSched.cpp
        filter.cpp
        task_facts.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
        ${NSCP_ERROR_CPP}
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        ${PLUGIN_API_TARGET}
        ${NSCP_FILTER_LIB}
        expression_parser
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_REGEX_LIBRARY}
        Mstask.lib
        ole32
        OleAut32
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
