cmake_minimum_required(VERSION 3.10)

set(TARGET CheckSystem)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    CheckSystem.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    realtime_thread.cpp
    collector_calc.cpp
    realtime_data.cpp
    filter_config_object.cpp
    check_uptime.cpp
    check_os_version.cpp
    check_os_updates.cpp
    check_installed_software.cpp
    check_hostname.cpp
    check_kernel_memory.cpp
    check_memory.cpp
    check_cpu.cpp
    check_load.cpp
    check_cpu_utilization.cpp
    check_kernel_stats.cpp
    check_swap_io.cpp
    check_service.cpp
    check_service_launchd.cpp
    exec_command.cpp
    check_process.cpp
    check_pagefile.cpp
    check_cpu_frequency.cpp
    check_temperature.cpp
    check_battery.cpp
    check_network.cpp
    check_process_history.cpp
    system_facts.cpp
    software_facts_unix.cpp
    ${NSCP_INCLUDEDIR}/facts/host_facts.cpp
    ${NSCP_INCLUDEDIR}/facts/network_facts.cpp
    ${NSCP_INCLUDEDIR}/facts/software_facts.cpp
)

# The platform data sources. The files above are platform-neutral: the checks,
# their keyword registries and output builders, and the pure parse/compute
# halves the unit tests drive. Each file below reads one platform's kernel -
# `_linux` procfs and sysfs, `_darwin` sysctl, Mach, libproc and IOKit - behind
# a shared header, and the two lists define the same functions.
if(APPLE)
    set(PLATFORM_SRCS
        collector_source_darwin.cpp
        live_source_darwin.cpp
        process_source_darwin.cpp
        service_source_darwin.cpp
        software_source_darwin.cpp
        plist_darwin.cpp
        hardware_source_darwin.cpp
        network_facts_darwin.cpp
        mach_stats_darwin.cpp
        interfaces_darwin.cpp
    )
    set(PLATFORM_TEST_SRCS
        darwin_sources_test.cpp
    )
    # Property lists (receipts, bundles, the update cache) and IOKit power
    # sources. libproc and the Mach calls are in libSystem.
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation)
    find_library(IOKIT_FRAMEWORK IOKit)
    set(PLATFORM_LIBS ${CORE_FOUNDATION_FRAMEWORK} ${IOKIT_FRAMEWORK})
else()
    set(PLATFORM_SRCS
        collector_source_linux.cpp
        live_source_linux.cpp
        process_source_linux.cpp
        service_source_linux.cpp
        software_source_linux.cpp
        hardware_source_linux.cpp
        network_facts_linux.cpp
    )
    set(PLATFORM_TEST_SRCS)
    set(PLATFORM_LIBS)
endif()
set(SRCS ${SRCS} ${PLATFORM_SRCS})

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        CheckSystem.h
        realtime_thread.hpp
        realtime_data.hpp
        filter_config_object.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
        check_uptime.h
        check_os_version.h
        check_os_updates.h
        check_installed_software.h
        check_hostname.h
        check_kernel_memory.h
        check_memory.h
        check_cpu.h
        check_load.h
        check_cpu_utilization.h
        check_kernel_stats.h
        check_swap_io.h
        check_service.h
        check_process.h
        check_pagefile.h
        check_cpu_frequency.h
        check_temperature.h
        check_battery.h
        check_network.h
        check_process_history.h
        collector_source.h
        system_facts.h
        ${NSCP_INCLUDEDIR}/facts/host_facts.hpp
        ${NSCP_INCLUDEDIR}/facts/network_facts.hpp
        ${NSCP_INCLUDEDIR}/facts/software_facts.hpp
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
    ${PLATFORM_LIBS}
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    check_system_unix_test
    SOURCES
        check_system_unix_test.cpp
        check_battery_test.cpp
        check_battery.cpp
        check_os_updates_test.cpp
        check_os_updates.cpp
        check_installed_software_test.cpp
        check_installed_software.cpp
        check_hostname_test.cpp
        check_hostname.cpp
        check_kernel_memory_test.cpp
        check_kernel_memory.cpp
        check_os_version_test.cpp
        check_os_version.cpp
        check_process_test.cpp
        check_process.cpp
        check_service_test.cpp
        check_service.cpp
        check_service_launchd.cpp
        exec_command.cpp
        check_cpu_frequency_test.cpp
        check_cpu_frequency.cpp
        check_network_test.cpp
        check_temperature_test.cpp
        check_temperature.cpp
        check_load_test.cpp
        check_load.cpp
        check_cpu_utilization_test.cpp
        check_cpu_utilization.cpp
        check_kernel_stats_test.cpp
        check_kernel_stats.cpp
        check_swap_io_test.cpp
        check_swap_io.cpp
        check_uptime_test.cpp
        check_uptime.cpp
        system_facts_test.cpp
        system_facts.cpp
        software_facts_unix.cpp
        software_facts_unix_test.cpp
        network_facts_unix_test.cpp
        collector_calc.cpp
        collector_calc_test.cpp
        ${NSCP_INCLUDEDIR}/facts/host_facts.cpp
        ${NSCP_INCLUDEDIR}/facts/network_facts.cpp
        ${NSCP_INCLUDEDIR}/facts/software_facts.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
        ${PLATFORM_SRCS}
        ${PLATFORM_TEST_SRCS}
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
        ${PLATFORM_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
