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

# Syslog over TLS (RFC 5425). Without OpenSSL the module still builds and
# speaks UDP/TCP; a `transport = tls` target then fails the submission with a
# clear error instead of silently sending in the clear.
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DUSE_SSL)
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        ${OPENSSL_LIBRARIES}
    )
    if(WIN32)
        set(EXTRA_LIBS
            ${EXTRA_LIBS}
            CRYPT32
        )
    endif(WIN32)
endif(OPENSSL_FOUND)

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
if(OPENSSL_FOUND)
    OPENSSL_LINK_FIX(${TARGET})
endif(OPENSSL_FOUND)

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The payload construction (connection_data, the RFC 3164 priority tables and
# what submit() puts on the wire) is ordinary C++ with a socket at the end, so
# it is unit-testable on every platform: the UDP datagrams, the RFC 6587/5425
# stream framings and the TLS handshake are all read back from loopback
# sockets.
NSCP_CREATE_TEST(
    syslog_client_test
    SOURCES
        syslog_client_test.cpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        # syslog_handler.hpp defines syslog_target_object::read(), which calls
        # the settings helper. Those symbols are in the plugin-api library but
        # are not exported from it on Windows, so linking the library alone
        # leaves them unresolved there while ELF's default visibility hides
        # the problem on unix. Compile them in, as every other test that
        # touches the settings helper does.
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_CLIENT_CPP}
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
