cmake_minimum_required(VERSION 3.10)

set(TARGET GearmanClient)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    gearman_protocol.cpp
    gearman_crypt.cpp
    gearman_job.cpp
    ${NSCP_INCLUDEDIR}/bytes/base64.c
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

# module.cmake skips the whole module without OpenSSL: the payload envelope is
# AES from the EVP API, so there is no unencrypted mode worth building.
add_definitions(-DUSE_SSL)
set(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
if(WIN32)
    set(EXTRA_LIBS ${EXTRA_LIBS} CRYPT32)
endif(WIN32)
include_directories(${OPENSSL_INCLUDE_DIR})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        gearman_protocol.hpp
        gearman_crypt.hpp
        gearman_job.hpp
        ${NSCP_INCLUDEDIR}/bytes/base64.h
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
)
OPENSSL_LINK_FIX(${TARGET})
include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The payloads captured from the real cores. The crypto and text-format tests
# assert against them byte for byte, so they need the directory at build time.
set(GEARMAN_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# The gearman binary protocol on its own: encoding, decoding, and the bounds
# that keep a stream that is not gearmand from being followed anywhere.
NSCP_CREATE_TEST(
    gearman_protocol_test
    SOURCES
        gearman_protocol_test.cpp
        gearman_protocol.cpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# The AES envelope, against payloads produced by ConSol's mod_gearman and by
# the nagios-mod-gearman fork.
NSCP_CREATE_TEST(
    gearman_crypt_test
    SOURCES
        gearman_crypt_test.cpp
        gearman_crypt.cpp
        ${NSCP_INCLUDEDIR}/bytes/base64.c
    LIBRARIES
        GTest::gtest_main
        ${EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${OPENSSL_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# The job and result text formats, round-tripped through the same fixtures.
NSCP_CREATE_TEST(
    gearman_job_test
    SOURCES
        gearman_job_test.cpp
        gearman_job.cpp
        gearman_crypt.cpp
        ${NSCP_INCLUDEDIR}/bytes/base64.c
    LIBRARIES
        GTest::gtest_main
        ${EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${OPENSSL_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(NSCP_BUILD_TESTS)
    target_compile_definitions(gearman_crypt_test PRIVATE GEARMAN_FIXTURE_DIR="${GEARMAN_FIXTURES}")
    target_compile_definitions(gearman_job_test PRIVATE GEARMAN_FIXTURE_DIR="${GEARMAN_FIXTURES}")
endif()
