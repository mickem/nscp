cmake_minimum_required(VERSION 3.10)

set(TARGET CheckMSSQL)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    odbc_query.cpp
    check_mssql.cpp
    check_mssql_query.cpp
    check_mssql_databases.cpp
    check_mssql_backup.cpp
    check_mssql_jobs.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
)

if(WIN32)
    set(SRCS
        ${SRCS}
        # Headers (for IDE project generation).
        "${TARGET}.h"
        odbc_query.hpp
        mssql_options.hpp
        mssql_filter_helpers.hpp
        check_mssql.hpp
        check_mssql_query.hpp
        check_mssql_databases.hpp
        check_mssql_backup.hpp
        check_mssql_jobs.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
        ${NSCP_ERROR_HPP}
    )
endif(WIN32)

add_definitions(${NSCP_GLOBAL_DEFINES})

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
    odbc32
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)

if(WIN32)
    NSCP_CREATE_TEST(
        check_mssql_test
        SOURCES
            check_mssql_test.cpp
            # Module sources under test.
            odbc_query.cpp
            check_mssql.cpp
            check_mssql_query.cpp
            check_mssql_databases.cpp
            check_mssql_backup.cpp
            check_mssql_jobs.cpp
            ${NSCP_FILTER_CPP}
            ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
            ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
            ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
            ${NSCP_INCLUDEDIR}/error/error_w32.cpp
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
            odbc32
        INCLUDES
            ${NSCP_INCLUDEDIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
