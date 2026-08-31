cmake_minimum_required(VERSION 3.10)

set(TARGET CheckExternalScripts)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    extscr_cli.cpp
    script_provider.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_ERROR_CPP}
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        commands.hpp
        ${NSCP_INCLUDEDIR}/nscapi/command_alias.hpp
        extscr_cli.h
        script_provider.hpp
        script_interface.hpp
        ${NSCP_INCLUDEDIR}/process/execute_process.hpp
        ${NSCP_INCLUDEDIR}/win/sysinfo/win_sysinfo.cpp
        ${NSCP_INCLUDEDIR}/win/sysinfo/win_sysinfo.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_ERROR_HPP}
    )
    set(SRCS
        ${SRCS}
        ${NSCP_INCLUDEDIR}/process/execute_process_w32.cpp
        ${NSCP_INCLUDEDIR}/process/argv_quote.cpp
    )
else(WIN32)
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        pthread
    )
    set(SRCS
        ${SRCS}
        ${NSCP_INCLUDEDIR}/process/execute_process_unix.cpp
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The script provider (alias/command bookkeeping and wrapped-command expansion)
# only needs a core wrapper with stubbed settings/registry endpoints, so it is
# unit-testable on every platform.
NSCP_CREATE_TEST(
    script_provider_test
    SOURCES
        script_provider_test.cpp
        script_provider.cpp
        # The Windows plugin_api DLL does not export the settings_helper
        # symbols commands::command_object::read() needs, so compile them in
        # along with the utf8 conversions they use
        # (see client_command_line_parser_test).
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_REGEX_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
