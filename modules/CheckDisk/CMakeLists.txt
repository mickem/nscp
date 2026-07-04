cmake_minimum_required(VERSION 3.10)

set(TARGET CheckDisk)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# Platform-neutral sources (the plugin, the filters/checks, the metric builders
# and the collector). Per-OS data acquisition is added below.
set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    check_disk_io.cpp
    check_disk_health.cpp
    check_files.cpp
    check_mount.cpp
    check_single_file.cpp
    collector_thread.cpp
    filter.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
    ${NSCP_INCLUDEDIR}/compat.cpp
)

if(WIN32)
    set(SRCS
        ${SRCS}
        check_drive_win.cpp
        check_disk_io_win.cpp
        file_finder_win.cpp
        filter_win.cpp
        ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.cpp
        # Headers (for IDE project generation).
        "${TARGET}.h"
        check_drive.hpp
        check_disk_io.hpp
        check_disk_health.hpp
        check_files.hpp
        check_mount.hpp
        check_single_file.hpp
        collector_thread.hpp
        file_finder.hpp
        filter.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
        ${NSCP_ERROR_HPP}
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.hpp
        ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.hpp
        ${NSCP_INCLUDEDIR}/compat.hpp
    )
else()
    set(SRCS
        ${SRCS}
        check_drive_unix.cpp
        check_disk_io_unix.cpp
        file_finder_unix.cpp
        filter_unix.cpp
    )
endif()

add_definitions(${NSCP_GLOBAL_DEFINES})

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

set(EXTRA_LIBS)
if(WIN32)
    set(EXTRA_LIBS version.lib Wbemuuid.lib)
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

# OpenSSL powers the check_files content checksums (md5/sha*). Optional: without
# it the checksum keywords compile but return empty strings.
if(OPENSSL_FOUND)
    target_include_directories(${TARGET} PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(${TARGET} ${OPENSSL_LIBRARIES})
    target_compile_definitions(${TARGET} PRIVATE CHECKDISK_HAVE_OPENSSL)
    if(WIN32)
        # Static libcrypto pulls in the CAPI engine / cert store and BIO sockets.
        target_link_libraries(${TARGET} CRYPT32 ws2_32)
    endif()
endif()

include(${BUILD_CMAKE_FOLDER}/module.cmake)

if(WIN32)
    NSCP_CREATE_TEST(
        check_disk_test
        SOURCES
            check_disk_io_test.cpp
            check_disk_health_test.cpp
            check_drive_test.cpp
            check_files_test.cpp
            check_single_file_test.cpp
            collector_thread_test.cpp
            file_finder_filter_test.cpp
            # Module sources under test.
            check_disk_io.cpp
            check_disk_io_win.cpp
            check_disk_health.cpp
            check_drive_win.cpp
            check_files.cpp
            check_single_file.cpp
            collector_thread.cpp
            file_finder_win.cpp
            filter.cpp
            filter_win.cpp
            ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
            ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
            ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
            ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
            ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
            ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.cpp
            ${NSCP_INCLUDEDIR}/error/error_w32.cpp
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
            Wbemuuid.lib
            ole32
            version.lib
        INCLUDES
            ${NSCP_INCLUDEDIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(OPENSSL_FOUND AND TARGET check_disk_test)
        target_include_directories(check_disk_test PRIVATE ${OPENSSL_INCLUDE_DIR})
        target_link_libraries(check_disk_test ${OPENSSL_LIBRARIES} CRYPT32 ws2_32)
        target_compile_definitions(check_disk_test PRIVATE CHECKDISK_HAVE_OPENSSL)
    endif()
else()
    NSCP_CREATE_TEST(
        check_disk_unix_test
        SOURCES
            check_disk_unix_test.cpp
            check_disk_health_test.cpp
            check_disk_health.cpp
            check_disk_io.cpp
            check_disk_io_unix.cpp
            check_drive_unix.cpp
            check_files.cpp
            check_files_checksum_test.cpp
            check_mount.cpp
            check_mount_test.cpp
            check_single_file.cpp
            file_finder_unix.cpp
            filter.cpp
            filter_unix.cpp
            ${NSCP_FILTER_CPP}
            ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
            ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
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
    if(OPENSSL_FOUND AND TARGET check_disk_unix_test)
        target_include_directories(check_disk_unix_test PRIVATE ${OPENSSL_INCLUDE_DIR})
        target_link_libraries(check_disk_unix_test ${OPENSSL_LIBRARIES})
        target_compile_definitions(check_disk_unix_test PRIVATE CHECKDISK_HAVE_OPENSSL)
    endif()
endif()
