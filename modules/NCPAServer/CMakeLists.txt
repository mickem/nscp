cmake_minimum_required(VERSION 3.10)

set(TARGET NCPAServer)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ncpa_controller.cpp
    ncpa_protocol.cpp
    ncpa_sources.cpp
    # The token is stored hashed (pbkdf2-sha256$...) and verified through the
    # same helper as the WEB and check_nt passwords.
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
include_directories(../../libs/mongoose-cpp)

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        ncpa_controller.hpp
        ncpa_protocol.hpp
        ncpa_sources.hpp
        ${NSCP_INCLUDEDIR}/net/auth_rate_limiter.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_INCLUDEDIR}/nscp/password_hash.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_JSON_LIBRARY}
    nscp_net
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
    nscp_mongoose
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The wire contract (path splitting, argument splitting, the token check, the
# plugin policy and the JSON bodies) is dispatch-free, so it tests without a
# core. It decodes with the HTTP layer's own helpers, hence nscp_mongoose, and
# verifies the token through password_hash, hence OpenSSL (${EXTRA_LIBS}).
NSCP_CREATE_TEST(
    ${TARGET}_test
    SOURCES
        ncpa_protocol_test.cpp
        ncpa_protocol.cpp
        ${NSCP_INCLUDEDIR}/nscp/password_hash.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_JSON_LIBRARY}
        nscp_mongoose
        ${EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/mongoose-cpp
)
