cmake_minimum_required(VERSION 3.10)

set(TARGET NSCAServer)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ${NSCP_INCLUDEDIR}/net/nsca/nsca_packet.cpp
    ${NSCP_INCLUDEDIR}/bytes/crc32.cpp
    ${NSCP_INCLUDEDIR}/nscp/password_hash.cpp
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})
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
        ${NSCP_INCLUDEDIR}/net/nsca/server/protocol.hpp
        ${NSCP_INCLUDEDIR}/net/nsca/server/handler.hpp
        ${NSCP_INCLUDEDIR}/net/nsca/server/parser.hpp
        ${NSCP_INCLUDEDIR}/net/nsca/nsca_packet.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_INCLUDEDIR}/net/socket/server.hpp
        ${NSCP_INCLUDEDIR}/net/socket/connection.hpp
        ${NSCP_INCLUDEDIR}/nscpcrypt/nscpcrypt.hpp
        ${NSCP_INCLUDEDIR}/bytes/crc32.h
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(
    ${TARGET}
    nscp_net
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${CRYPTOPP_LIBRARIES}
    ${EXTRA_LIBS}
    nscpcrypt
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Server" REGULAR_EXPRESSION .*include/nsca/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)

# Unit tests for the module class itself: the settings it registers/reads and
# the load-mode split - a start refuses a configuration the listener cannot
# safely come up on, dontStart (settings tooling, the documentation extractor)
# loads it anyway so the configuration can be shown and fixed. The stub core
# (include/nscapi/test_helpers.hpp) stands in for the daemon; no test binds the
# port.
NSCP_CREATE_TEST(
    nsca_server_module_test
    SOURCES
        nsca_server_module_test.cpp
        "${TARGET}.cpp"
        ${NSCP_INCLUDEDIR}/net/nsca/nsca_packet.cpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
        ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.cpp
        ${NSCP_INCLUDEDIR}/bytes/crc32.cpp
        ${NSCP_INCLUDEDIR}/nscp/password_hash.cpp
        ${NSCP_DEF_PLUGIN_CPP}
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${CRYPTOPP_LIBRARIES}
        ${EXTRA_LIBS}
        nscpcrypt
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
