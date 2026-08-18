cmake_minimum_required(VERSION 3.10)

set(TARGET CheckWindowsApps)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_iis_checks.cpp
    check_rds_licenses.cpp
    check_rds_sessions.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/basic_impl.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_collectors.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_counters.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_enumerations.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_interface.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_object_gather.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_query.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_resolver.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/thread_safe_impl.cpp
    ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        check_iis_checks.hpp
        check_iis_internal.hpp
        check_rds_licenses.hpp
        check_rds_sessions.hpp
        check_rds_internal.hpp
        ${NSCP_INCLUDEDIR}/win/pdh/basic_impl.hpp
        ${NSCP_INCLUDEDIR}/win/pdh/pdh_collectors.hpp
        ${NSCP_INCLUDEDIR}/win/pdh/pdh_counters.hpp
        ${NSCP_INCLUDEDIR}/win/pdh/pdh_enumerations.hpp
        ${NSCP_INCLUDEDIR}/win/pdh/pdh_interface.hpp
        ${NSCP_INCLUDEDIR}/win/pdh/pdh_object_gather.hpp
        ${NSCP_INCLUDEDIR}/win/pdh/pdh_query.hpp
        ${NSCP_INCLUDEDIR}/win/pdh/pdh_resolver.hpp
        ${NSCP_INCLUDEDIR}/win/pdh/thread_safe_impl.hpp
        ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.hpp
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
    ${Boost_REGEX_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
    winsys_info
    Wbemuuid.lib
    OleAut32
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("pdh" REGULAR_EXPRESSION .*include/win/pdh/.*)

NSCP_CREATE_TEST(
    check_iis_test
    SOURCES
        check_iis_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
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

NSCP_CREATE_TEST(
    check_rds_test
    SOURCES
        check_rds_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
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
