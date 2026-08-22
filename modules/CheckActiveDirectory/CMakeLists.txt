cmake_minimum_required(VERSION 3.10)

set(TARGET CheckActiveDirectory)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# The module is Windows-only (module.cmake gates it), so all sources are
# unconditional. kdc_probe.cpp and ad_replication_filter.cpp are pure
# (no win32 API) so the unit test below can link them without ntdsapi.
set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_ad_replication.cpp
    ad_replication_filter.cpp
    ad_replication_source_win.cpp
    check_secure_channel.cpp
    check_kdc.cpp
    kdc_probe.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
    # Headers (for IDE project generation).
    "${TARGET}.h"
    check_ad_replication.hpp
    ad_replication_filter.hpp
    ad_replication_source.hpp
    check_secure_channel.hpp
    check_kdc.hpp
    kdc_probe.hpp
    netapi_buffer.hpp
    win32_error.hpp
    ${NSCP_DEF_PLUGIN_HPP}
    ${NSCP_FILTER_HPP}
    ${NSCP_ERROR_HPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

# Ntdsapi (DsBind/DsReplicaGetInfo), Netapi32 (I_NetLogonControl2,
# NetGetJoinInformation, DsGetDcName, DsRoleGetPrimaryDomainInformation),
# ws2_32/mswsock (asio KDC probe).
set(EXTRA_LIBS Ntdsapi Netapi32 ws2_32 mswsock)

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
    ${EXTRA_LIBS}
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    check_activedirectory_test
    SOURCES
        check_activedirectory_test.cpp
        kdc_probe.cpp
        ad_replication_filter.cpp
        ${NSCP_FILTER_CPP}
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
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
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_REGEX_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
