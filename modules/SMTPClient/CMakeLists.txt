cmake_minimum_required(VERSION 3.10)

set(TARGET SMTPClient)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    smtp.cpp
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/bytes/base64.c
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

# SMTP submission to Gmail / Microsoft 365 / any modern relay requires
# STARTTLS (port 587) or implicit TLS (port 465). OpenSSL is mandatory.
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
else(OPENSSL_FOUND)
    message(FATAL_ERROR "SMTPClient requires OpenSSL for STARTTLS / SMTPS support")
endif(OPENSSL_FOUND)

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        smtp_client.hpp
        smtp_handler.hpp
        smtp.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_CLIENT_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Server" REGULAR_EXPRESSION .*include/smtp/.*)

NSCP_CREATE_TEST(
    ${TARGET}_test
    SOURCES
        smtp_test.cpp
        smtp.cpp
        ${NSCP_INCLUDEDIR}/bytes/base64.c
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest
        GTest::gtest_main
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
