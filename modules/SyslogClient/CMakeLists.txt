cmake_minimum_required(VERSION 3.10)

set(TARGET SyslogClient)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        syslog_client.hpp
        syslog_handler.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_CLIENT_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The payload construction (connection_data, the RFC 3164 priority tables and
# what submit() puts on the wire) is ordinary C++ with a UDP socket at the end,
# so it is unit-testable on every platform.
NSCP_CREATE_TEST(
    syslog_client_test
    SOURCES
        syslog_client_test.cpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        # syslog_target_object's vtable pulls in its read(), which calls
        # settings_keys_easy_init::add_string / string_fun_key. Those live in
        # nscp_plugin_api but carry no NSCAPI_EXPORT, so they are visible when
        # linking against the shared object on unix and absent from the import
        # library on Windows. Compile them into the test rather than widening
        # the exported plugin API surface.
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_CLIENT_CPP}
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
