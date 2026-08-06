cmake_minimum_required(VERSION 3.10)

set(TARGET onboarding)

# Fleet onboarding (enrollment) shared between the installer and the core
# client: Ed25519 identity generation, /enroll/v1 over HTTPS and durable
# state persistence. Requires OpenSSL for key/CSR generation and TLS.
if(NOT OPENSSL_FOUND)
    message(STATUS "   - OpenSSL not found (onboarding library disabled)")
    return()
endif()

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

add_definitions(${NSCP_GLOBAL_DEFINES})
add_definitions(-DUSE_SSL)

set(EXTRA_LIBS ${OPENSSL_LIBRARIES})
if(WIN32)
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        CRYPT32
        Ws2_32.lib
    )
endif(WIN32)

set(onboarding_SRCS
    identity.cpp
    enroll.cpp
    state.cpp
    sync.cpp
    verify.cpp
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/bytes/base64.c
    ${NSCP_INCLUDEDIR}/str/utf8.cpp
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
)
if(WIN32)
    set(onboarding_SRCS
        ${onboarding_SRCS}
        json_util.hpp
        ${NSCP_INCLUDEDIR}/onboarding/onboarding.hpp
        ${NSCP_INCLUDEDIR}/onboarding/sync.hpp
        ${NSCP_INCLUDEDIR}/net/http/client.hpp
    )
endif(WIN32)

add_library(${TARGET} STATIC ${onboarding_SRCS})

target_link_libraries(
    ${TARGET}
    ${EXTRA_LIBS}
)

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "libraries"
)

if(CMAKE_COMPILER_IS_GNUCXX)
    nscp_apply_pic(${TARGET})
endif(CMAKE_COMPILER_IS_GNUCXX)

NSCP_CREATE_TEST(
    onboarding_test
    SOURCES
        onboarding_test.cpp
        sync_test.cpp
    LIBRARIES
        GTest::gtest_main
        onboarding
        ${EXTRA_LIBS}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
