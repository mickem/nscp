cmake_minimum_required(VERSION 3.10)

set(TARGET IcingaAgent)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# The protocol pieces: no socket, no core, so the unit test links them alone.
set(PROTOCOL_SRCS
    icinga_agent_arguments.cpp
    icinga_agent_execute.cpp
    icinga_agent_fingerprint.cpp
    icinga_agent_import.cpp
    icinga_agent_json.cpp
    icinga_agent_macros.cpp
    icinga_agent_messages.cpp
    icinga_agent_netstring.cpp
    icinga_agent_thresholds.cpp
    icinga_agent_translate.cpp
)

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ${PROTOCOL_SRCS}
    icinga_agent_connection.cpp
    icinga_agent_pki.cpp
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})
# socket_helpers' certificate and peer-identity helpers sit behind USE_SSL.
add_definitions(-DUSE_SSL)
include_directories(${OPENSSL_INCLUDE_DIR})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        icinga_agent_protocol.hpp
        icinga_agent_arguments.hpp
        icinga_agent_execute.hpp
        icinga_agent_fingerprint.hpp
        icinga_agent_import.hpp
        icinga_agent_json.hpp
        icinga_agent_macros.hpp
        icinga_agent_messages.hpp
        icinga_agent_netstring.hpp
        icinga_agent_thresholds.hpp
        icinga_agent_translate.hpp
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
    nscp_net
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${OPENSSL_LIBRARIES}
    ${EXTRA_LIBS}
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    ${TARGET}_test
    SOURCES
        icinga_agent_arguments_test.cpp
        icinga_agent_execute_test.cpp
        icinga_agent_fingerprint_test.cpp
        icinga_agent_import_test.cpp
        icinga_agent_json_test.cpp
        icinga_agent_macros_test.cpp
        icinga_agent_messages_test.cpp
        icinga_agent_netstring_test.cpp
        icinga_agent_thresholds_test.cpp
        icinga_agent_translate_test.cpp
        ${PROTOCOL_SRCS}
    LIBRARIES
        GTest::gtest_main
        ${Boost_JSON_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
