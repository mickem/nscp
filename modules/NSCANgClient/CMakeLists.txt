cmake_minimum_required(VERSION 3.10)

set(TARGET NSCANgClient)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    nsca_ng.cpp
    nsca_ng_client.cpp
    nsca_ng_handler.cpp
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP}
)

# Headers listed unconditionally so IDE projects show them on Linux too.
# (Previously the headers were only listed inside the WIN32 block, which
# broke navigation on non-Windows generators — D2 fix.)
set(SRCS
    ${SRCS}
    "${TARGET}.h"
    nsca_ng.hpp
    nsca_ng_client.hpp
    nsca_ng_handler.hpp
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
    ${NSCP_DEF_PLUGIN_HPP}
    ${NSCP_CLIENT_HPP}
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

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

if(WIN32)
    set(NSCA_NG_TEST_EXTRA_LIBS CRYPT32)
endif()

NSCP_CREATE_TEST(
    nsca_ng_test
    SOURCES
        nsca_ng_test.cpp
        nsca_ng.cpp
        nsca_ng_client.cpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
        # Defines utf8::to_unicode / utf8::wstring_to_string, used transitively
        # via utf8::utf8_from_native by both nsca_ng_client.cpp (native error
        # message conversion) and socket_helpers.cpp (OpenSSL error conversion).
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${OPENSSL_LIBRARIES}
        ${NSCP_DEF_PLUGIN_LIB}
        # OpenSSL's bundled CAPI engine on Windows pulls in Crypt32. The
        # module target picks this up via EXTRA_LIBS; the test target needs
        # it spelled out separately.
        ${NSCA_NG_TEST_EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${OPENSSL_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
