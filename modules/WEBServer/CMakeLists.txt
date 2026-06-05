cmake_minimum_required(VERSION 3.10)

set(TARGET WEBServer)

project(${TARGET})

# WEBServer links against nscp_mongoose, which is only built when the selected
# web backend's dependencies are satisfied (see NSCP_MONGOOSE_AVAILABLE in
# build/cmake/dependencies.cmake). Fail with a clear message here instead of a
# cryptic "target nscp_mongoose not found" later. Disable this module with
# -DBUILD_MODULE_WEBServer=OFF if you cannot provide the backend.
if(NOT TARGET nscp_mongoose)
    message(
        FATAL_ERROR
        "WEBServer requires the nscp_mongoose library, which was not built "
        "because the NSCP_WEB_BACKEND='${NSCP_WEB_BACKEND}' web backend's "
        "dependencies are not satisfied. Provide the backend dependencies "
        "(mongoose source, or OpenSSL for beast), switch NSCP_WEB_BACKEND, "
        "or disable this module with -DBUILD_MODULE_WEBServer=OFF."
    )
endif()

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    static_controller.cpp
    token_store.cpp
    user_manager.cpp
    password_hash.cpp
    name_safety.cpp
    grant_store.cpp
    session_manager_interface.cpp
    metrics_handler.cpp
    error_handler.cpp
    event_store.cpp
    modules_controller.cpp
    query_controller.cpp
    alias_controller.cpp
    scripts_controller.cpp
    legacy_controller.cpp
    legacy_command_controller.cpp
    api_controller.cpp
    log_controller.cpp
    info_controller.cpp
    settings_controller.cpp
    login_controller.cpp
    metrics_controller.cpp
    openmetrics_controller.cpp
    metadata_controller.cpp
    events_controller.cpp
    web_installer.cpp
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.cpp
    ${NSCP_INCLUDEDIR}/client/simple_client.cpp
    ${NSCP_INCLUDEDIR}/metrics/metrics_store_map.cpp
    ${NSCP_INCLUDEDIR}/bytes/base64.c
    ${NSCP_INCLUDEDIR}/bytes/unzip.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
)

add_definitions(-DMG_ENABLE_HTTP_WEBSOCKET=0)
add_definitions(-DMG_ENABLE_BROADCAST=1)
add_definitions(-DMG_ENABLE_THREADS)
add_definitions(${NSCP_GLOBAL_DEFINES})
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DMG_ENABLE_OPENSSL=1)
    add_definitions(-DMG_DISABLE_PFS=1)
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
        ${NSCP_INCLUDEDIR}/client/simple_client.hpp
        ${NSCP_INCLUDEDIR}/metrics/metrics_store_map.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.cpp
        info_controller.hpp
        settings_controller.hpp
        metrics_controller.hpp
        openmetrics_controller.hpp
        metadata_controller.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
    nscp_mongoose
    nscp_miniz
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    ${TARGET}_test
    SOURCES
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
        token_store_test.cpp
        token_store.cpp
        grant_store_test.cpp
        grant_store.cpp
        session_manager_interface_test.cpp
        session_manager_interface.cpp
        user_manager.cpp
        user_manager_test.cpp
        password_hash.cpp
        password_hash_test.cpp
        name_safety.cpp
        name_safety_test.cpp
        auth_rate_limiter_test.cpp
        error_handler.cpp
        metrics_handler.cpp
        web_installer.cpp
        web_installer_test.cpp
        static_controller.cpp
        static_controller_test.cpp
        ${NSCP_INCLUDEDIR}/bytes/base64.c
        ${NSCP_INCLUDEDIR}/bytes/unzip.cpp
        ${NSCP_INCLUDEDIR}/json/use_json.cpp
        ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.cpp
        # web_installer.cpp pulls in net/http/client.hpp which references
        # socket_helpers::tls_method_parser / verify_mode_parser at link
        # time, even though our tests never go through the HTTP path
        # (from_path code path skips it).
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
        # NSC_LOG_ERROR / NSC_DEBUG_MSG in session_manager_interface.cpp
        # resolve through nscapi::plugin_singleton -> core_wrapper::log /
        # should_log. The wrapper itself is defined in nscapi_core_wrapper.cpp;
        # nscapi_helper.cpp provides logging::matches() that should_log()
        # delegates to; the helper_singleton instance is declared by the
        # test entry point.
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
    LIBRARIES
        GTest::gtest
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_REGEX_LIBRARY}
        ${EXTRA_LIBS}
        nscp_mongoose
        nscp_miniz
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/mongoose-cpp
)
# The nscapi headers above declare their public API with NSCAPI_EXPORT, which
# expands to __declspec(dllimport) when consumers include them. We compile
# the matching cpp files directly into this test binary (so the symbols are
# locally defined, not imported), which produces C4273 / LNK4217 dll-linkage
# mismatch noise on MSVC. plugin_api_NOLIB neuters the macro to "" so both
# sides agree: no dllexport, no dllimport, just a plain symbol. See
# include/nscapi/dll_defines.hpp for the macro definition.
#
# Guarded because NSCP_CREATE_TEST is a no-op when NSCP_BUILD_TESTS=OFF, so the
# ${TARGET}_test target does not exist in that configuration.
if(NSCP_BUILD_TESTS)
    target_compile_definitions(${TARGET}_test PRIVATE plugin_api_NOLIB)
endif()
