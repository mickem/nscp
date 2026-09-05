cmake_minimum_required(VERSION 3.10)

set(TARGET CheckSecurity)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# Platform-neutral sources. Certificate parsing is OpenSSL-based and shared;
# only the certificate-store and firewall data acquisition is per-OS.
set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_activation.cpp
    check_certificate.cpp
    check_file_security.cpp
    check_firewall_rules.cpp
    check_firewall.cpp
    check_users.cpp
    check_nla.cpp
    check_antivirus.cpp
    check_bitlocker.cpp
    check_secureboot.cpp
    check_defender.cpp
    check_local_accounts.cpp
    check_group_members.cpp
    cert_filter.cpp
    cert_source_openssl.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
)

if(WIN32)
    set(SRCS
        ${SRCS}
        cert_source_win.cpp
        check_activation_win.cpp
        check_file_security_win.cpp
        check_firewall_rules_win.cpp
        check_firewall_win.cpp
        check_users_win.cpp
        posture_win.cpp
        ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.cpp
        # Headers (for IDE project generation).
        "${TARGET}.h"
        cert_filter.hpp
        cert_source.hpp
        check_activation.hpp
        check_certificate.hpp
        check_file_security.hpp
        check_firewall_rules.hpp
        check_firewall.hpp
        check_users.hpp
        check_nla.hpp
        check_antivirus.hpp
        check_bitlocker.hpp
        check_secureboot.hpp
        check_defender.hpp
        check_local_accounts.hpp
        check_group_members.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
        ${NSCP_ERROR_HPP}
    )
else()
    set(SRCS
        ${SRCS}
        check_firewall_unix.cpp
        check_users_unix.cpp
        posture_unix.cpp
    )
endif()

add_definitions(${NSCP_GLOBAL_DEFINES})

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

set(EXTRA_LIBS)
if(WIN32)
    # CRYPT32 (cert store), Wtsapi32 (sessions), ole32/oleaut32/Wbemuuid (COM/WMI
    # for NLA + Security Center + BitLocker + Software Licensing), Advapi32
    # (registry, ConvertSidToStringSid, security descriptors and the service
    # control manager for check_file_security), Netapi32 (NetLocalGroupGetMembers
    # for check_group_members).
    set(EXTRA_LIBS CRYPT32 ws2_32 ole32 oleaut32 Wbemuuid Wtsapi32 Advapi32 Netapi32)
endif()

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${NSCP_FILTER_LIB}
    expression_parser
    ${EXTRA_LIBS}
)

# OpenSSL powers the cross-platform certificate parsing (check_certificate).
if(OPENSSL_FOUND)
    target_include_directories(${TARGET} PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(${TARGET} ${OPENSSL_LIBRARIES})
else()
    message(WARNING "CheckSecurity: OpenSSL not found; check_certificate will not function.")
endif()

include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The firewall group-policy overlay is platform-neutral (header-inline), so
# unlike check_security_test (OpenSSL fixtures, Linux-only) this builds on
# both platforms.
NSCP_CREATE_TEST(
    check_firewall_test
    SOURCES
        check_firewall_test.cpp
        ${NSCP_FILTER_CPP}
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
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
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# The licensing decode, the owner/DACL classification and the command bodies of
# the Windows-only posture checks are platform neutral (only the data
# acquisition is Windows), so these unit tests build everywhere. The command
# tests stand in for the platform sources, which is what lets the *matched* half
# of these checks - thresholds, rendering, perf data - run off Windows at all.
NSCP_CREATE_TEST(
    check_posture_test
    SOURCES
        check_activation_test.cpp
        check_file_security_test.cpp
        check_firewall_rules_test.cpp
        check_posture_commands_test.cpp
        check_activation.cpp
        check_file_security.cpp
        check_firewall_rules.cpp
        check_antivirus.cpp
        check_bitlocker.cpp
        check_defender.cpp
        check_group_members.cpp
        check_local_accounts.cpp
        check_nla.cpp
        check_secureboot.cpp
        check_users.cpp
        ${NSCP_FILTER_CPP}
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
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
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(NOT WIN32)
    NSCP_CREATE_TEST(
        check_security_test
        SOURCES
            check_certificate_test.cpp
            cert_filter.cpp
            cert_source_openssl.cpp
            ${NSCP_FILTER_CPP}
            ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
            ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
            ${NSCP_INCLUDEDIR}/str/utf8.cpp
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
        INCLUDES
            ${NSCP_INCLUDEDIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(OPENSSL_FOUND AND TARGET check_security_test)
        target_include_directories(check_security_test PRIVATE ${OPENSSL_INCLUDE_DIR})
        target_link_libraries(check_security_test ${OPENSSL_LIBRARIES})
    endif()
endif()
