cmake_minimum_required(VERSION 3.10)

set(TARGET NSClientServer)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_nt_commands.cpp
    ${NSCP_INCLUDEDIR}/net/check_nt/packet.cpp
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
        # libcrypto's BIO layer calls into winsock (BIO_sock_init, BIO_lookup
        # and the socket BIOs), so anything linking the static libcrypto needs
        # ws2_32 as well. The module pulls it in through nscp_net; the password
        # memo test links libcrypto for PBKDF2 and never touches a socket, and
        # would otherwise fail with 30 unresolved externals out of libcrypto
        # on the static build (same trap as GearmanClient's crypt tests).
        set(EXTRA_LIBS
            ${EXTRA_LIBS}
            CRYPT32
            ws2_32
        )
    endif(WIN32)
endif(OPENSSL_FOUND)

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        check_nt_commands.hpp
        ${NSCP_INCLUDEDIR}/net/check_nt/server/protocol.hpp
        ${NSCP_INCLUDEDIR}/net/check_nt/server/parser.hpp
        ${NSCP_INCLUDEDIR}/net/check_nt/packet.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_INCLUDEDIR}/net/socket/connection.hpp
        ${NSCP_INCLUDEDIR}/net/socket/server.hpp
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
    ${EXTRA_LIBS}
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Server" REGULAR_EXPRESSION .*include/check_nt/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)

# The dispatch-free protocol logic (allow parsing, request mapping, response
# formatting) is platform-neutral, so the test runs everywhere the module
# builds.
NSCP_CREATE_TEST(
    check_nt_commands_test
    SOURCES
        check_nt_commands_test.cpp
        check_nt_commands.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        ${PLUGIN_API_TARGET}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

# The password check is a class of its own precisely so the KDF-per-request
# question can be asserted on without standing up the server: the memo exposes
# how many derivations it has run.
NSCP_CREATE_TEST(
    check_nt_password_memo_test
    SOURCES
        password_memo_test.cpp
        ${NSCP_INCLUDEDIR}/nscp/password_hash.cpp
    LIBRARIES
        GTest::gtest_main
        ${EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
