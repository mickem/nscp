cmake_minimum_required(VERSION 3.10)

set(TARGET nscp)

project(service)
include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})

set(service_SRCS
    NSClient++.cpp
    core_api.cpp
    path_manager.cpp
    plugins/plugin_manager.cpp
    plugins/master_plugin_list.cpp
    plugins/dll_plugin.cpp
    plugins/plugin_cache.cpp
    plugins/zip_plugin.cpp
    ${NSCP_INCLUDEDIR}/utf8.cpp
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
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_functions.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    # Logger
    ${NSCP_INCLUDEDIR}/nsclient/logger/log_level.cpp
    ${NSCP_INCLUDEDIR}/nsclient/logger/base_logger_impl.cpp
    ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper.cpp
    ${NSCP_INCLUDEDIR}/nsclient/logger/log_message_factory.cpp
    ${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
    ${NSCP_INCLUDEDIR}/pid_file.cpp
    ${NSCP_INCLUDEDIR}/has-threads.cpp
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
    ../resources/nsclient.ico
)

if(WIN32)
    set(service_SRCS ${service_SRCS} ${NSCP_INCLUDEDIR}/win/service_control.cpp)
    set(service_SRCS
        ${service_SRCS}
        NSClient++.h
        NSClient++.rc
        ${NSCP_INCLUDEDIR}/utf8.hpp
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
        ${NSCP_INCLUDEDIR}/com_helpers.hpp
        ${NSCP_INCLUDEDIR}/dll/dll.hpp
        ${NSCP_INCLUDEDIR}/dll/impl_unix.hpp
        ${NSCP_INCLUDEDIR}/dll/impl_w32.hpp
        ${NSCP_INCLUDEDIR}/service/system_service.hpp
        ${NSCP_INCLUDEDIR}/service/win32_service.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
        # Logger
        ${NSCP_INCLUDEDIR}/nsclient/logger/base_logger_impl.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_driver_interface.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_level.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper.hpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_message_factory.hpp
        ${NSCP_INCLUDEDIR}/pid_file.hpp
        ${NSCP_INCLUDEDIR}/has-threads.hpp
        ${NSCP_INCLUDEDIR}/win/shellapi.cpp
        ${NSCP_INCLUDEDIR}/win/shellapi.hpp
    )
    # SET(service_SRCS ${service_SRCS}
    # ${CMAKE_CURRENT_SOURCE_DIR}/NSClient++.manifest )

    add_definitions(-D_WIN32_DCOM)
endif(WIN32)

add_definitions(${NSCP_GLOBAL_DEFINES} -Dnscp_plugin_api_NOLIB)

# Add python
if(WIN32)
    set(service_SRCS
        ${service_SRCS}
        ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.cpp
        ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.hpp
    )
    set(EXTRA_LIBS ${EXTRA_LIBS} psapi)
endif(WIN32)

nscp_make_exe_sbin(${TARGET} "${service_SRCS}")
set_target_properties(${TARGET} PROPERTIES FOLDER "core")

# if(WIN32) SET_TARGET_PROPERTIES(${TARGET} PROPERTIES LINK_FLAGS
# "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"
# /SUBSYSTEM:CONSOLE") endif()
target_link_libraries(
    ${TARGET}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
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

nscp_create_test(service_test
    SOURCES
        performance_data_test.cpp
        cron_test.cpp
        path_manager.cpp
        path_manager_test.cpp
        ${NSCP_INCLUDEDIR}/json/use_json.cpp
        ../include/parsers/cron/cron_parser.hpp
        ../include/nscapi/nscapi_protobuf_functions.cpp
        ../include/nscapi/nscapi_protobuf_functions.hpp
        ${NSCP_INCLUDEDIR}/win/shellapi.cpp
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${EXTRA_LIBS}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        settings_manager
        nscp_miniz
        expression_parser
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

nscp_create_test(plugins_test
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
        path_manager.cpp
        ${NSCP_INCLUDEDIR}/error/error_w32.cpp
        ${NSCP_INCLUDEDIR}/json/use_json.cpp
        ${NSCP_INCLUDEDIR}/win/shellapi.cpp
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${EXTRA_LIBS}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        settings_manager
        nscp_miniz
        expression_parser
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
