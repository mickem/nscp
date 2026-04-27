cmake_minimum_required(VERSION 3.10)

set(TARGET CheckDisk)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_drive.cpp
    check_disk_io.cpp
    check_disk_health.cpp
    collector_thread.cpp
    file_finder.cpp
    filter.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
    ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.cpp
    ${NSCP_INCLUDEDIR}/compat.cpp
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        check_drive.hpp
        check_disk_io.hpp
        check_disk_health.hpp
        collector_thread.hpp
        file_finder.hpp
        filter.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
        ${NSCP_ERROR_HPP}
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.hpp
        ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.hpp
        ${NSCP_INCLUDEDIR}/compat.hpp
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
    version.lib
    Wbemuuid.lib
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    check_disk_test
    SOURCES
        # Test sources (one per CheckDisk source file with unit-testable logic).
        check_disk_io_test.cpp
        check_disk_health_test.cpp
        check_drive_test.cpp
        collector_thread_test.cpp
        file_finder_filter_test.cpp
        # Module sources under test.
        check_disk_io.cpp
        check_disk_health.cpp
        check_drive.cpp
        collector_thread.cpp
        file_finder.cpp
        filter.cpp
        # NSCP support sources.
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
        ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.cpp
        ${NSCP_INCLUDEDIR}/error/error_w32.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        ${NSCP_FILTER_LIB}
        expression_parser
        plugin_api
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_REGEX_LIBRARY}
        Wbemuuid.lib
        ole32
        version.lib
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

