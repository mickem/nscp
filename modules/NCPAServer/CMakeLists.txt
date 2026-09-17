cmake_minimum_required(VERSION 3.10)

set(TARGET NCPAServer)

project(${TARGET})

# NCPAServer links against nscp_mongoose, which is only built when the selected
# web backend's dependencies are satisfied (see NSCP_MONGOOSE_AVAILABLE in
# build/cmake/dependencies.cmake). Fail with a clear message here instead of a
# cryptic "target nscp_mongoose not found" later. Disable this module with
# -DBUILD_MODULE_NCPAServer=OFF if you cannot provide the backend.
if(NOT TARGET nscp_mongoose)
    message(
        FATAL_ERROR
        "NCPAServer requires the nscp_mongoose library, which was not built "
        "because the NSCP_WEB_BACKEND='${NSCP_WEB_BACKEND}' web backend's "
        "dependencies are not satisfied. Provide the backend dependencies "
        "(mongoose source, or OpenSSL for beast), switch NSCP_WEB_BACKEND, "
        "or disable this module with -DBUILD_MODULE_NCPAServer=OFF."
    )
endif()

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ncpa_check.cpp
    ncpa_tree.cpp
    ncpa_sources.cpp
    ncpa_controller.cpp
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
)

# Only the data acquisition is platform specific: the node tree, the check
# rendering and the HTTP layer are shared.
if(WIN32)
    set(SRCS
        ${SRCS}
        ncpa_sysinfo_win.cpp
    )
else()
    set(SRCS
        ${SRCS}
        ncpa_sysinfo_unix.cpp
    )
endif()

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
        ncpa_check.hpp
        ncpa_tree.hpp
        ncpa_sources.hpp
        ncpa_controller.hpp
        ${NSCP_INCLUDEDIR}/net/http/auth_rate_limiter.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
    # WTSEnumerateSessions / WTSQuerySessionInformation for the `user` node.
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        WtsApi32
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
    ${Boost_THREAD_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
    nscp_mongoose
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The tree and check halves are dispatch-free, so they test without a core.
NSCP_CREATE_TEST(
    ${TARGET}_test
    SOURCES
        ncpa_check.cpp
        ncpa_check_test.cpp
        ncpa_tree.cpp
        ncpa_tree_test.cpp
        ncpa_sources.cpp
        ncpa_sources_test.cpp
        ${NSCP_INCLUDEDIR}/json/use_json.cpp
        # ncpa_sources.cpp reaches the platform shim for the `system` and
        # `user` nodes; the tests assert on the snapshot-driven nodes, but the
        # tree they build contains those two as well.
        $<$<BOOL:${WIN32}>:ncpa_sysinfo_win.cpp>
        $<$<NOT:$<BOOL:${WIN32}>>:ncpa_sysinfo_unix.cpp>
    LIBRARIES
        GTest::gtest
        GTest::gtest_main
        # ncpa_sources.cpp walks PB::Metrics messages, so the test needs the
        # generated protobuf code (and the dependency that puts
        # protobuf/metrics.pb.h in the build tree before we compile).
        nscp_protobuf
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_REGEX_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${EXTRA_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
# The nscapi headers declare their public API with NSCAPI_EXPORT, which expands
# to __declspec(dllimport) for consumers. The test binary compiles the matching
# sources in directly, so plugin_api_NOLIB neuters the macro and both sides
# agree. Guarded because NSCP_CREATE_TEST is a no-op with NSCP_BUILD_TESTS=OFF.
if(NSCP_BUILD_TESTS)
    target_compile_definitions(${TARGET}_test PRIVATE plugin_api_NOLIB)
    if(WIN32)
        target_link_libraries(${TARGET}_test WtsApi32)
    endif()
endif()
