cmake_minimum_required(VERSION 3.10)

set(TARGET NRDPClient)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# When tinyxml2 came from the system package (libtinyxml2-dev / tinyxml2-devel)
# we link against the imported target. Otherwise FindTinyXML2 located a source
# checkout (the Windows / vendored path) and we compile tinyxml2.cpp directly.
if(TARGET tinyxml2::tinyxml2)
    set(TINYXML2_SRCS "")
    set(TINYXML2_EXTRA_LIBS tinyxml2::tinyxml2)
else()
    set(TINYXML2_SRCS ${TINYXML2_INCLUDE_DIR}/tinyxml2.cpp)
    set(TINYXML2_EXTRA_LIBS "")
endif()

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    nrdp.cpp
    ${TINYXML2_SRCS}
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/bytes/base64.c
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})
include_directories(${TINYXML2_INCLUDE_DIR})
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
        nrdp.hpp
        nrdp_client.hpp
        nrdp_handler.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${TINYXML2_INCLUDE_DIR}/tinyxml2.h
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
    ${EXTRA_LIBS}
    ${TINYXML2_EXTRA_LIBS}
)
OPENSSL_LINK_FIX(${TARGET})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("Server" REGULAR_EXPRESSION .*include/nrdp/.*)

NSCP_CREATE_TEST(
    nrdp_test
    SOURCES
        nrdp_test.cpp
        nrdp.cpp
        ${TINYXML2_SRCS}
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
        ${TINYXML2_EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${TINYXML2_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
