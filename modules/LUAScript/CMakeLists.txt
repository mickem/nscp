cmake_minimum_required(VERSION 3.10)

set(TARGET LUAScript)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    extscr_cli.cpp
    script_provider.cpp
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        extscr_cli.h
        script_provider.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)
include_directories(${LUA_INCLUDE_DIR})
add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_JSON_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${LUA_LIBRARIES}
    lua_nscp
)

# The provider's script lookup needs only a core wrapper with no endpoints, so
# which folders it searches is unit-testable without the Lua runtime.
NSCP_CREATE_TEST(
    LUAScript_provider_test
    SOURCES
        script_provider_test.cpp
        script_provider.cpp
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

# Unit tests for the module class: the settings it registers, loading the
# configured scripts, and dispatch into what they register, with small scripts
# the test writes to a temporary script root. The stub core
# (include/nscapi/test_helpers.hpp) stands in for the daemon.
NSCP_CREATE_TEST(
    lua_script_module_test
    SOURCES
        lua_script_module_test.cpp
        "${TARGET}.cpp"
        extscr_cli.cpp
        script_provider.cpp
        ${NSCP_DEF_PLUGIN_CPP}
    LIBRARIES
        GTest::gtest_main
        ${Boost_JSON_LIBRARY}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${NSCP_DEF_PLUGIN_LIB}
        ${LUA_LIBRARIES}
        lua_nscp
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LUA_INCLUDE_DIR}
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)
