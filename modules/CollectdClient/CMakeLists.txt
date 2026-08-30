cmake_minimum_required(VERSION 3.10)

set(TARGET CollectdClient)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ${NSCP_INCLUDEDIR}/net/collectd/collectd_packet.cpp
    ${NSCP_INCLUDEDIR}/net/collectd/collectd_crypto.cpp
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})
if(OPENSSL_FOUND)
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
    include_directories(${OPENSSL_INCLUDE_DIR})
endif(OPENSSL_FOUND)

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        collectd_client.hpp
        collectd_handler.hpp
        ${NSCP_INCLUDEDIR}/net/collectd/collectd_packet.hpp
        ${NSCP_INCLUDEDIR}/net/collectd/collectd_crypto.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
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
    ${Boost_REGEX_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
    expression_parser
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Client" REGULAR_EXPRESSION .*include/collectd/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)

NSCP_CREATE_TEST(
    collectd_packet_test
    SOURCES
        collectd_packet_test.cpp
        ${NSCP_INCLUDEDIR}/net/collectd/collectd_packet.cpp
        ${NSCP_INCLUDEDIR}/net/collectd/collectd_crypto.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        expression_parser
        ${Boost_REGEX_LIBRARY}
        ${Boost_FILESYSTEM_LIBRARY}
        ${OPENSSL_LIBRARIES}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OPENSSL_INCLUDE_DIR}
)
# The crypto tests verify real signatures/ciphertext, so the test binary needs
# the same USE_SSL the module gets (harmless no-op when OpenSSL is absent —
# NSCP_BUILD_TESTS builds are OpenSSL builds in practice).
if(OPENSSL_FOUND AND TARGET collectd_packet_test)
    target_compile_definitions(collectd_packet_test PRIVATE USE_SSL)
endif()
