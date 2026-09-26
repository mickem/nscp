cmake_minimum_required(VERSION 3.10)

set(TARGET CheckKubernetes)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_cluster.cpp
    check_pods.cpp
    check_nodes.cpp
    check_workloads.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_INCLUDEDIR}/bytes/base64.c
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

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        check_cluster.hpp
        check_pods.hpp
        check_nodes.hpp
        check_workloads.hpp
        kube_client.hpp
        kube_json.hpp
        kube_settings.hpp
        kube_quantity.hpp
        kube_pod_status.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(
    ${TARGET}
    nscp_json
    nscp_net
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
    ${EXTRA_LIBS}
)

# The two header-only helpers need nothing beyond boost::json.
NSCP_CREATE_TEST(
    kube_quantity_test
    SOURCES
        kube_quantity_test.cpp
    LIBRARIES
        GTest::gtest_main
)
NSCP_CREATE_TEST(
    kube_pod_status_test
    SOURCES
        kube_pod_status_test.cpp
    LIBRARIES
        nscp_json
        GTest::gtest_main
)

# The check logic is tested through an injected fetcher serving canned API
# server payloads, so this target needs no HTTP client. nscp_net is linked
# for the verify-mode helper the settings resolver shares with the client,
# and OpenSSL (EXTRA_LIBS) for the inline helpers in its header.
NSCP_CREATE_TEST(
    check_kubernetes_test
    SOURCES
        check_kubernetes_test.cpp
        check_cluster.cpp
        check_pods.cpp
        check_nodes.cpp
        check_workloads.cpp
        ${NSCP_INCLUDEDIR}/bytes/base64.c
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        nscp_json
        nscp_net
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
        ${EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)
