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

# Boost.Thread is not optional here: script_provider.cpp guards its alias and
# command tables with a boost::shared_mutex, which pulls in
# this_thread::interruption_point, disable_interruption and
# get_current_thread_data. script_provider_test below has always listed it -
# the same source needed it as an executable.
#
# The module got away without it because an ELF shared object may leave symbols
# undefined and resolve them from the host process at dlopen time, and nscp has
# Boost.Thread loaded by then. A Mach-O bundle must resolve everything at link
# time, so on macOS the omission is a build failure rather than a latent one:
#
#     Undefined symbols for architecture arm64:
#       "boost::this_thread::interruption_point()", referenced from: ...
#
# Linking it explicitly is what the module always meant. Building the tree with
# -Wl,--no-undefined makes the GNU linker enforce the same rule, and says this
# is the only target in the project that was relying on the difference.
target_link_libraries(
    ${TARGET}
    nscp_json
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The script provider (alias/command bookkeeping and wrapped-command expansion)
# only needs a core wrapper with stubbed settings/registry endpoints, so it is
# unit-testable on every platform.
# Header-only path arithmetic shared by the ext-scr CLI; unit-tested rather
# than covered by tests/ because the decision it makes is Windows-only and a
# Linux integration run cannot observe it.
NSCP_CREATE_TEST(
    script_paths_test
    SOURCES
        script_paths_test.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

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
