cmake_minimum_required(VERSION 3.10)

set(TARGET CheckDHCP)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# The module is Windows-only (module.cmake gates it). check_dhcp.cpp and
# dhcp_filter.cpp are platform-neutral and also built into the unit test;
# dhcp_source_win.cpp is the only file that touches dhcpsapi and PDH.
set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_dhcp.cpp
    dhcp_filter.cpp
    dhcp_source_win.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/basic_impl.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_collectors.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_counters.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_enumerations.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_interface.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_object_gather.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_query.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_resolver.cpp
    ${NSCP_INCLUDEDIR}/win/pdh/thread_safe_impl.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
    # Headers (for IDE project generation).
    "${TARGET}.h"
    check_dhcp.hpp
    dhcp_filter.hpp
    dhcp_source.hpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_interface.hpp
    ${NSCP_INCLUDEDIR}/win/pdh/pdh_object_gather.hpp
    ${NSCP_DEF_PLUGIN_HPP}
    ${NSCP_FILTER_HPP}
    ${NSCP_ERROR_HPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

# dhcpsapi: the DHCP Server Management API (DhcpEnumSubnets, DhcpGetSubnetInfo,
# DhcpGetMibInfo). It ships with every Windows edition, the DHCP Server role or
# not; without the role the calls fail and the check reports UNKNOWN.
target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
    dhcpsapi
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)
source_group("pdh" REGULAR_EXPRESSION .*include/win/pdh/.*)

include(${CMAKE_CURRENT_SOURCE_DIR}/unit_tests.cmake)
