cmake_minimum_required(VERSION 3.10)

set(TARGET IcingaAgent)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    icinga_agent_protocol.cpp
    icinga_agent_connection.cpp
    icinga_agent_pki.cpp
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})
include_directories(${OPENSSL_INCLUDE_DIR})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        icinga_agent_protocol.hpp
        icinga_agent_connection.hpp
        icinga_agent_pki.hpp
        icinga_agent_openssl.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
    set(EXTRA_LIBS CRYPT32)
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_JSON_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${OPENSSL_LIBRARIES}
    ${EXTRA_LIBS}
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    ${TARGET}_test
    SOURCES
        icinga_agent_protocol_test.cpp
        icinga_agent_protocol.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_JSON_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
