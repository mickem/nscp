cmake_minimum_required(VERSION 3.10)

set(TARGET DotnetPlugins)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    dotnet_host.cpp
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        dotnet_bridge.hpp
        dotnet_host.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${CMAKE_DL_LIBS}
)
if(WIN32)
    # RegOpenKeyExW & co. for locating the .NET install through the registry.
    target_link_libraries(${TARGET} advapi32)
endif()

include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    dotnet_plugins_test
    SOURCES
        dotnet_host_test.cpp
        dotnet_host.cpp
        # The module class: its value resolution is plain C++ and the log
        # macros only need the plugin singleton the test defines itself.
        "${TARGET}.cpp"
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_impl.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        ${PLUGIN_API_TARGET}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${CMAKE_DL_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
