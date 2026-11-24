cmake_minimum_required(VERSION 3.10)

set(TARGET CheckLogFile)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    ${TARGET}.cpp
    realtime_data.cpp
    realtime_thread.cpp
    filter_config_object.cpp
    filter.cpp
    bookmarks.cpp
    check_csv.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
)

# The file reader: on Windows CreateFileW with impersonation refused and only
# disk files accepted (log_file.hpp says why), elsewhere a std::filebuf.
if(WIN32)
    set(SRCS ${SRCS} log_file_win.cpp)
else()
    set(SRCS ${SRCS} log_file_unix.cpp)
endif()

add_definitions(${NSCP_GLOBAL_DEFINES})

# check_csv's file reader is the one per-OS piece: CreateFileW on Windows
# (UNC paths, every share flag), an ifstream elsewhere.
if(WIN32)
    set(SRCS ${SRCS} csv_source_win.cpp)
else()
    set(SRCS ${SRCS} csv_source_unix.cpp)
endif()

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        check_csv.hpp
        log_file.hpp
        device_streambuf.hpp
        realtime_thread.hpp
        realtime_data.hpp
        filter.hpp
        filter_config_object.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

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

# check_csv: the column parsing, decoding, splitting and the filter wiring,
# driven through check_with() with an injected reader. The file_reader and
# bookmark tests are header-only and live in tests/CMakeLists.txt.
if(WIN32)
    set(CHECK_CSV_TEST_SOURCE csv_source_win.cpp)
else()
    set(CHECK_CSV_TEST_SOURCE csv_source_unix.cpp)
endif()
NSCP_CREATE_TEST(
    check_csv_test
    SOURCES
        check_csv_test.cpp
        check_csv.cpp
        ${CHECK_CSV_TEST_SOURCE}
        ${NSCP_ERROR_CPP}
        ${NSCP_FILTER_CPP}
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        ${NSCP_FILTER_LIB}
        expression_parser
        ${PLUGIN_API_TARGET}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_REGEX_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Unit tests for the module class: the settings it registers, the file access
# policy, and check_logfile over files the test writes to a temporary
# directory. The stub core (include/nscapi/test_helpers.hpp) stands in for the
# daemon; dontStart keeps the real-time monitor thread from starting.
NSCP_CREATE_TEST(
    check_logfile_module_test
    SOURCES
        check_logfile_module_test.cpp
        ${TARGET}.cpp
        realtime_data.cpp
        realtime_thread.cpp
        filter_config_object.cpp
        filter.cpp
        bookmarks.cpp
        check_csv.cpp
        ${CHECK_CSV_TEST_SOURCE}
        ${NSCP_DEF_PLUGIN_CPP}
        ${NSCP_FILTER_CPP}
        ${NSCP_ERROR_CPP}
    LIBRARIES
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_REGEX_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${NSCP_DEF_PLUGIN_LIB}
        ${NSCP_FILTER_LIB}
        expression_parser
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
