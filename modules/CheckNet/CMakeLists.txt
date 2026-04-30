cmake_minimum_required(VERSION 3.10)

set(TARGET CheckNet)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    filter.cpp
    check_tcp.cpp
    check_dns.cpp
    check_http.cpp
    check_connections.cpp
    check_ntp_offset.cpp
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/bytes/base64.c
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        filter.hpp
        check_tcp.h
        check_dns.h
        check_http.h
        check_connections.h
        check_ntp_offset.h
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
)

if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DUSE_SSL)
    target_link_libraries(${TARGET} ${OPENSSL_LIBRARIES})
    if(WIN32)
        target_link_libraries(${TARGET} CRYPT32)
    endif(WIN32)
endif(OPENSSL_FOUND)

if(WIN32)
    target_link_libraries(
        ${TARGET}
        iphlpapi
        ws2_32
    )
endif(WIN32)

include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    check_net_test
    SOURCES
        check_net_test.cpp
        filter.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        ${NSCP_FILTER_LIB}
        expression_parser
        ${PLUGIN_API_TARGET}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_REGEX_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
