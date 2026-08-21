cmake_minimum_required(VERSION 3.10)

set(TARGET nscp)

project(service)
include_directories(${BOOST_INCLUDE_DIRS})
# Include path for the zip backend: vendored miniz on Windows, system libzip
# elsewhere. The nscp_miniz target propagates the right path either way, but
# legacy direct includes need this fallback while the source still uses
# unqualified <miniz.h>.
link_directories(${BOOST_LIB_DIRS})

if(MINIZ_FOUND)
    include_directories(${MINIZ_INCLUDE_DIR})
    add_definitions(-DMINIZ)
endif()
set(service_SRCS
    NSClient++.cpp
    core_api.cpp
    path_manager.cpp
    plugins/plugin_manager.cpp
    plugins/master_plugin_list.cpp
    plugins/dll_plugin.cpp
    plugins/plugin_cache.cpp
    plugins/zip_plugin.cpp
    ${NSCP_INCLUDEDIR}/bytes/unzip.cpp
    ${NSCP_INCLUDEDIR}/str/utf8.cpp
    # logger
    logger/nsclient_logger.cpp
    logger/simple_console_logger.cpp
    logger/simple_file_logger.cpp
    logger/threaded_logger.cpp
    settings_query_handler.cpp
    registry_query_handler.cpp
    storage_query_handler.cpp
    storage_manager.cpp
    cli_parser.cpp
    settings_client.cpp
    ${NSCP_INCLUDEDIR}/scheduler/simple_scheduler.cpp
    scheduler_handler.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_convert.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_copy.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_exec.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_perfdata.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_query.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_response.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_status.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_submit.cpp
    ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    # Logger
    ${NSCP_INCLUDEDIR}/nsclient/logger/log_level.cpp
    ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper.cpp
    ${NSCP_INCLUDEDIR}/nsclient/logger/log_message_factory.cpp
    ${NSCP_INCLUDEDIR}/pid_file.cpp
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
    ${NSCP_INCLUDEDIR}/nscp/layout_migration.cpp
    ../resources/nsclient.ico
)

if(WIN32)
    set(service_SRCS
        ${service_SRCS}
        ${NSCP_INCLUDEDIR}/win/service_control.cpp
        ${NSCP_INCLUDEDIR}/bytes/base64.c
        windows_ca_store.cpp
        windows_ca_store.hpp
        ${NSCP_INCLUDEDIR}/win/acl.cpp
        ${NSCP_INCLUDEDIR}/win/acl.hpp
    )
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        CRYPT32
        ADVAPI32
    )
    set(service_SRCS
        ${service_SRCS}
        NSClient++.h
        NSClient++.rc
        ${NSCP_INCLUDEDIR}/str/utf8.hpp
        core_api.h
        settings_client.hpp
        cli_parser.hpp
        ${NSCP_INCLUDEDIR}/scheduler/simple_scheduler.hpp
        scheduler_handler.hpp
        path_manager.hpp
        storage_manager.hpp
        commands.hpp
        channels.hpp
        routers.hpp
        ${NSCP_ERROR_CPP}
        ${NSCP_ERROR_HPP}
        # logger
        logger/nsclient_logger.hpp
        logger/simple_console_logger.hpp
        logger/simple_file_logger.hpp
        logger/threaded_logger.hpp
        settings_query_handler.hpp
        registry_query_handler.hpp
        storage_query_handler.hpp
        plugins/plugin_manager.hpp
        plugins/dll_plugin.h
        plugins/plugin_interface.hpp
        plugins/plugin_list.hpp
        plugins/plugin_cache.hpp
        plugins/zip_plugin.h
        plugins/master_plugin_list.hpp
        service_manager.hpp
        # ${NSCP_INCLUDEDIR}/config.h
        ${NSCP_INCLUDEDIR}/win/com_helpers.hpp
        ${NSCP_INCLUDEDIR}/dll/dll.hpp
        ${NSCP_INCLUDEDIR}/dll/impl_unix.hpp
        ${NSCP_INCLUDEDIR}/dll/impl_w32.hpp
        ${NSCP_INCLUDEDIR}/service/system_service.hpp
        ${NSCP_INCLUDEDIR}/service/win32_service.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
        # Logger
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger_impl.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_driver_interface.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_level.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_message_factory.hpp
        ${NSCP_INCLUDEDIR}/pid_file.hpp
        ${NSCP_INCLUDEDIR}/threads/scoped_thread_group.hpp
        ${NSCP_INCLUDEDIR}/win/shellapi.cpp
        ${NSCP_INCLUDEDIR}/win/shellapi.hpp
        # Protobuf
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_convert.hpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_copy.hpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_exec.hpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_perfdata.hpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_query.hpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_response.hpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_status.hpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_submit.hpp
    )
    # SET(service_SRCS ${service_SRCS}
    # ${CMAKE_CURRENT_SOURCE_DIR}/NSClient++.manifest )

    add_definitions(-D_WIN32_DCOM)
endif(WIN32)

add_definitions(
    ${NSCP_GLOBAL_DEFINES}
    -D${PLUGIN_API_TARGET}_NOLIB
)

# Add python
if(WIN32)
    set(service_SRCS
        ${service_SRCS}
        ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.cpp
        ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.hpp
    )
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        psapi
    )
endif(WIN32)

if(OPENSSL_FOUND)
    # Fleet onboarding (`nscp enroll`) and the fleet configuration sync loop
    # (fleet_sync, started at boot when the enrollment manifest exists) - see
    # libs/onboarding. Only available when OpenSSL is present.
    add_definitions(-DHAVE_ONBOARDING)
    include_directories(${OPENSSL_INCLUDE_DIR})
    set(service_SRCS
        ${service_SRCS}
        fleet_sync.cpp
        fleet_sync.hpp
    )
    # fleet_sync talks HTTPS/mTLS through net/http/client.hpp, which only
    # compiles its TLS transport under USE_SSL. Scoped to this file to keep
    # the rest of the service target unchanged.
    set_source_files_properties(
        fleet_sync.cpp
        PROPERTIES
            COMPILE_DEFINITIONS
                USE_SSL
    )
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        onboarding
    )
endif(OPENSSL_FOUND)

NSCP_MAKE_EXE_SBIN(${TARGET} "${service_SRCS}")
set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "core"
)

# if(WIN32) SET_TARGET_PROPERTIES(${TARGET} PROPERTIES LINK_FLAGS
# "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"
# /SUBSYSTEM:CONSOLE") endif()
target_link_libraries(
    ${TARGET}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    # boost::this_thread::sleep_for (fleet_sync's interruptible poll wait) pulls
    # in boost::chrono::steady_clock, which is not header-only.
    ${Boost_CHRONO_LIBRARY}
    ${PROTOBUF_LIBRARY}
    ${ICONV_LIBRARIES}
    ${EXTRA_LIBS}
    nscp_protobuf
    settings_manager
    expression_parser
    nscp_miniz
)

source_group("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
source_group("Settings" REGULAR_EXPRESSION .*include/settings/.*)
# SOURCE_GROUP("Common Files" REGULAR_EXPRESSION .*include/.*)
source_group("Log" REGULAR_EXPRESSION .*/logger/.*)

NSCP_CREATE_TEST(
    service_test
    SOURCES
        cron_test.cpp
        path_manager.cpp
        path_manager_test.cpp
        acl_test.cpp
        ${NSCP_INCLUDEDIR}/win/acl.cpp
        ${NSCP_INCLUDEDIR}/nscp/layout_migration.cpp
        permissions_test.cpp
        permissions.hpp
        tag_repository_test.cpp
        tag_repository.hpp
        ${NSCP_INCLUDEDIR}/json/use_json.cpp
        ../include/parsers/cron/cron_parser.hpp
        ${NSCP_INCLUDEDIR}/win/shellapi.cpp
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${EXTRA_LIBS}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        settings_manager
        nscp_miniz
        expression_parser
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
if(WIN32)
    set(service_test_SRCS ${NSCP_INCLUDEDIR}/error/error_w32.cpp)
else()
    set(service_test_SRCS)
endif()

if(WIN32)
    NSCP_CREATE_TEST(
        error_w32_test
        SOURCES
            ${NSCP_INCLUDEDIR}/error/error_w32.cpp
            ${NSCP_INCLUDEDIR}/error/error_w32_test.cpp
            ${NSCP_INCLUDEDIR}/str/utf8.cpp
        LIBRARIES
            GTest::gtest_main
        INCLUDES
            ${NSCP_INCLUDEDIR}
    )

    NSCP_CREATE_TEST(
        windows_ca_store_test
        SOURCES
            windows_ca_store.cpp
            windows_ca_store_test.cpp
            ${NSCP_INCLUDEDIR}/bytes/base64.c
        LIBRARIES
            GTest::gtest_main
            ${Boost_FILESYSTEM_LIBRARY}
            CRYPT32
        INCLUDES
            ${NSCP_INCLUDEDIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# settings_query_handler drives the process-wide settings manager, so the test
# needs the same pieces the service links for it: the settings manager itself,
# the plugin cache (plugin id -> name), boost::json for template registration
# and the protobuf helpers.
NSCP_CREATE_TEST(
    settings_query_handler_test
    SOURCES
        settings_query_handler.cpp
        settings_query_handler_test.cpp
        plugins/plugin_cache.cpp
        ${NSCP_INCLUDEDIR}/json/use_json.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_query.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_response.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_level.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_message_factory.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${EXTRA_LIBS}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        settings_manager
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

NSCP_CREATE_TEST(
    plugins_test
    SOURCES
        plugins/master_plugin_list.cpp
        plugins/master_plugin_list_test.cpp
        plugins/plugin_cache.cpp
        plugins/plugin_cache_test.cpp
        plugins/plugin_manager.cpp
        plugins/plugin_manager_test.cpp
        plugins/plugin_interface_test.cpp
        plugins/dll_plugin.cpp
        plugins/dll_plugin_test.cpp
        plugins/plugin_list_test.cpp
        plugins/zip_plugin.cpp
        plugins/zip_plugin_test.cpp
        commands_test.cpp
        commands.hpp
        ${NSCP_INCLUDEDIR}/bytes/unzip.cpp
        path_manager.cpp
        ${service_test_SRCS}
        ${NSCP_INCLUDEDIR}/json/use_json.cpp
        ${NSCP_INCLUDEDIR}/win/shellapi.cpp
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${EXTRA_LIBS}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        settings_manager
        nscp_miniz
        expression_parser
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    service_logger_test
    SOURCES
        logger/threaded_logger.cpp
        logger/threaded_logger_test.cpp
        logger/simple_console_logger.cpp
        logger/simple_console_logger_test.cpp
        logger/simple_file_logger.cpp
        logger/simple_file_logger_test.cpp
        logger/nsclient_logger.cpp
        logger/nsclient_logger_test.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_level.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_message_factory.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
        ${service_test_SRCS}
        ${NSCP_INCLUDEDIR}/win/shellapi.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        settings_manager
        ${EXTRA_LIBS}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_REGEX_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
