cmake_minimum_required(VERSION 3.10)

set(TARGET CheckMySQL)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# Platform-neutral throughout: MariaDB Connector/C is the data source on both
# Windows and Linux, and only mysql_session.cpp touches it.
set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_mysql.cpp
    check_mysql_query.cpp
    mysql_client.cpp
    mysql_session.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
)

if(WIN32)
    set(SRCS
        ${SRCS}
        # Headers (for IDE project generation).
        "${TARGET}.h"
        check_mysql.hpp
        check_mysql_query.hpp
        mysql_client.hpp
        mysql_session.hpp
        mysql_options.hpp
        mysql_filter_helpers.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
        ${NSCP_ERROR_HPP}
    )
endif(WIN32)

add_definitions(${NSCP_GLOBAL_DEFINES})

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_include_directories(${TARGET} PRIVATE ${MARIADB_INCLUDE_DIR})

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
    ${MARIADB_LIBRARY}
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)

# Stage libmariadb.dll next to nscp.exe, NOT next to this module.
#
# The plugin loader calls plain LoadLibrary() with a full path
# (include/dll/impl_w32.hpp), which does not add the loaded DLL's own directory
# to the dependency search path - so a copy in modules/ is never found and the
# module fails to load with "the specified module could not be found". The
# application directory is searched, which is where nscp.exe lives; lua.dll and
# the Python runtime DLL are shipped the same way (see Product.wxs).
if(WIN32 AND MARIADB_DLL)
    add_custom_command(
        TARGET ${TARGET}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "${MARIADB_DLL}"
            "${NSCP_PROJECT_BINARY_DIR}"
    )
elseif(WIN32)
    message(
        WARNING
        "libmariadb.dll not found next to ${MARIADB_LIBRARY}: CheckMySQL will build but not load. Set MARIADB_ROOT_DIR at a Connector/C install tree."
    )
endif()

# The unit tests exercise the checks through an injected session factory, so
# they compile without mysql_session.cpp and never link the connector.
NSCP_CREATE_TEST(
    check_mysql_test
    SOURCES
        check_mysql_test.cpp
        # Module sources under test.
        check_mysql.cpp
        check_mysql_query.cpp
        mysql_client.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
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
