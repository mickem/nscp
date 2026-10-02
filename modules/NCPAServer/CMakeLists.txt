cmake_minimum_required(VERSION 3.10)

set(TARGET NCPAServer)

project(${TARGET})

# NCPAServer serves HTTPS through nscp_mongoose, the library the WEB server
# uses, which is only built when the selected web backend's dependencies are
# satisfied (see NSCP_MONGOOSE_AVAILABLE in build/cmake/dependencies.cmake).
# Fail with a clear message here instead of a cryptic "target nscp_mongoose not
# found" later. Disable this module with -DBUILD_MODULE_NCPAServer=OFF if you
# cannot provide the backend.
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
    ncpa_controller.cpp
    ncpa_protocol.cpp
    ncpa_sources.cpp
    ${NSCP_DEF_PLUGIN_CPP}
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
include_directories(../../libs/mongoose-cpp)

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        ncpa_controller.hpp
        ncpa_protocol.hpp
        ncpa_sources.hpp
        ${NSCP_INCLUDEDIR}/net/auth_rate_limiter.hpp
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(
    ${TARGET}
    nscp_json
    nscp_net
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
    nscp_mongoose
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The wire contract (path splitting, form parsing, the token check, the plugin
# policy and the JSON bodies) is dispatch-free, so it tests without a core.
NSCP_CREATE_TEST(
    ${TARGET}_test
    SOURCES
        ncpa_protocol_test.cpp
        ncpa_protocol.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_json
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
