if(WIN32)
    NSCP_CREATE_TEST(
        shellapi_test
        SOURCES
            ${NSCP_INCLUDEDIR}/win/shellapi_test.cpp
            ${NSCP_INCLUDEDIR}/str/utf8.cpp
            ${NSCP_INCLUDEDIR}/win/shellapi.cpp
        LIBRARIES
            GTest::gtest_main
            ${Boost_FILESYSTEM_LIBRARY}
            Userenv
            ole32
            Shell32
        INCLUDES
            ${NSCP_INCLUDEDIR}
    )
    NSCP_CREATE_TEST(
        argv_quote_test
        SOURCES
            ${NSCP_INCLUDEDIR}/process/argv_quote_test.cpp
            ${NSCP_INCLUDEDIR}/process/argv_quote.cpp
            ${NSCP_INCLUDEDIR}/str/utf8.cpp
        LIBRARIES
            GTest::gtest_main
            Shell32
        INCLUDES
            ${NSCP_INCLUDEDIR}
    )
    NSCP_CREATE_TEST(
        wmi_query_test
        SOURCES
            ${NSCP_INCLUDEDIR}/win/wmi/wmi_query_test.cpp
            ${NSCP_INCLUDEDIR}/win/wmi/wmi_query.cpp
            ${NSCP_INCLUDEDIR}/error/error_w32.cpp
            ${NSCP_INCLUDEDIR}/str/utf8.cpp
        LIBRARIES
            GTest::gtest_main
            ${Boost_FILESYSTEM_LIBRARY}
            ole32
            wbemuuid
        INCLUDES
            ${NSCP_INCLUDEDIR}
    )
endif()

add_definitions(
    ${NSCP_GLOBAL_DEFINES}
    -D${PLUGIN_API_TARGET}_NOLIB
)

NSCP_CREATE_TEST(
    nscapi_protobuf_test
    SOURCES
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_convert_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_copy_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_exec_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_perfdata_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_query_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_response_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_status_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_submit_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/settings_functions_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/settings_functions.hpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_convert.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_copy.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_exec.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_perfdata.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_query.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_response.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_status.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_submit.cpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/settings_functions.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
NSCP_CREATE_TEST(
    str_test
    SOURCES
        ${NSCP_INCLUDEDIR}/str/format_test.cpp
        ${NSCP_INCLUDEDIR}/str/xtos_test.cpp
        ${NSCP_INCLUDEDIR}/str/nscp_string_test.cpp
        ${NSCP_INCLUDEDIR}/str/utils_test.cpp
        ${NSCP_INCLUDEDIR}/str/wstring_test.cpp
        ${NSCP_INCLUDEDIR}/str/utf8_test.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
        ${NSCP_INCLUDEDIR}/str/constant_time_test.cpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
NSCP_CREATE_TEST(
    nscp_time_test
    SOURCES
        ${NSCP_INCLUDEDIR}/nscp_time_test.cpp
    LIBRARIES
        GTest::gtest
        GTest::gtest_main
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
NSCP_CREATE_TEST(
    logger_test
    SOURCES
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_level_test.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_level.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_message_factory_test.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_message_factory.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper_test.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/log_driver_interface_impl_test.cpp
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger_impl_test.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest
        GTest::gtest_main
        nscp_protobuf
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    settings_helper_test
    SOURCES
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.hpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    settings_object_test
    SOURCES
        ${NSCP_INCLUDEDIR}/nscapi/settings/object_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.hpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    settings_filter_test
    SOURCES
        ${NSCP_INCLUDEDIR}/nscapi/settings/filter_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/filter.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/filter.hpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    settings_proxy_test
    SOURCES
        ${NSCP_INCLUDEDIR}/nscapi/settings/proxy_test.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        ${Boost_FILESYSTEM_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

if(OPENSSL_FOUND)
    NSCP_CREATE_TEST(
        net_test
        SOURCES
            ${NSCP_INCLUDEDIR}/net/pinger_test.cpp
            ${NSCP_INCLUDEDIR}/net/http/client_test.cpp
            ${NSCP_INCLUDEDIR}/net/http/proxy_config_test.cpp
            ${NSCP_INCLUDEDIR}/net/http/http_client_protocol_test.cpp
            ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts_test.cpp
            ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.cpp
            ${NSCP_INCLUDEDIR}/net/socket/socket_helpers_test.cpp
            ${NSCP_INCLUDEDIR}/net/socket/connection_test.cpp
            ${NSCP_INCLUDEDIR}/net/nrpe/packet_test.cpp
            ${NSCP_INCLUDEDIR}/net/nrpe/server/parser_test.cpp
            ${NSCP_INCLUDEDIR}/net/nrpe/server/protocol_test.cpp
            ${NSCP_INCLUDEDIR}/net/nrpe/client/nrpe_client_protocol_test.cpp
            ${NSCP_INCLUDEDIR}/net/nsca/nsca_packet_test.cpp
            ${NSCP_INCLUDEDIR}/net/nsca/server/parser_test.cpp
            ${NSCP_INCLUDEDIR}/net/nsca/server/protocol_test.cpp
            ${NSCP_INCLUDEDIR}/net/nsca/client/nsca_client_protocol_test.cpp
            ${NSCP_INCLUDEDIR}/nscpcrypt/nscpcrypt.hpp
            ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
            ${NSCP_INCLUDEDIR}/net/nrpe/packet.cpp
            ${NSCP_INCLUDEDIR}/net/nsca/nsca_packet.cpp
            ${NSCP_INCLUDEDIR}/bytes/base64.c
            ${NSCP_INCLUDEDIR}/bytes/crc32.cpp
            ${NSCP_INCLUDEDIR}/str/utf8.cpp
        LIBRARIES
            GTest::gtest_main
            nscpcrypt
            ${Boost_FILESYSTEM_LIBRARY}
            ${Boost_DATE_TIME_LIBRARY}
            ${OPENSSL_LIBRARIES}
            $<$<PLATFORM_ID:Windows>:CRYPT32>
        INCLUDES
            ${NSCP_INCLUDEDIR}
            ${OPENSSL_INCLUDE_DIR}
    )
    target_compile_definitions(net_test PRIVATE USE_SSL)
endif()

NSCP_CREATE_TEST(
    metrics_store_map_test
    SOURCES
        ${NSCP_INCLUDEDIR}/metrics/metrics_store_map_test.cpp
        ${NSCP_INCLUDEDIR}/metrics/metrics_store_map.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_THREAD_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    cron_parser_test
    SOURCES
        ${NSCP_INCLUDEDIR}/parsers/cron/cron_parser_test.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    simple_scheduler_test
    SOURCES
        ${NSCP_INCLUDEDIR}/scheduler/simple_scheduler_test.cpp
        ${NSCP_INCLUDEDIR}/scheduler/simple_scheduler.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_CHRONO_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    simple_expression_test
    SOURCES
        ${NSCP_INCLUDEDIR}/parsers/expression/expression_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/expression/expression.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_THREAD_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    perf_config_parser_test
    SOURCES
        ${NSCP_INCLUDEDIR}/parsers/perfconfig/perfconfig_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/perfdata_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/perfconfig/perfconfig.cpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    realtime_helper_test
    SOURCES
        ${NSCP_INCLUDEDIR}/parsers/filter/realtime_helper_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        ${PLUGIN_API_TARGET}
        nscp_protobuf
        nscp_where_filter
        perfconfig_parser
        expression_parser
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_REGEX_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    modern_filter_test
    SOURCES
        ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_where_filter
        perfconfig_parser
        expression_parser
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_REGEX_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    cli_helper_test
    SOURCES
        ${NSCP_INCLUDEDIR}/parsers/filter/cli_helper_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        nscp_where_filter
        perfconfig_parser
        expression_parser
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_REGEX_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    parsers_where_test
    SOURCES
        ${NSCP_INCLUDEDIR}/parsers/where/binary_op_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/helpers_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/list_node_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/node_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/unary_fun_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/unary_op_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/value_node_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/variable_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/perf_generator_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/engine_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/operators_test.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/list_node.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/unary_fun.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/value_node.cpp
        ${NSCP_INCLUDEDIR}/parsers/operators.cpp
        ${NSCP_INCLUDEDIR}/parsers/where.cpp
        ${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_where_filter
        ${Boost_REGEX_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    bytes_test
    SOURCES
        ${NSCP_INCLUDEDIR}/bytes/base64_test.cpp
        ${NSCP_INCLUDEDIR}/bytes/base64.c
        ${NSCP_INCLUDEDIR}/bytes/crc32_test.cpp
        ${NSCP_INCLUDEDIR}/bytes/crc32.cpp
        ${NSCP_INCLUDEDIR}/bytes/buffer_test.cpp
        ${NSCP_INCLUDEDIR}/bytes/char_buffer_test.cpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}/bytes
        ${NSCP_INCLUDEDIR}
)

# unzip_test exercises real archive extraction, so it only makes sense when an
# actual ZIP backend (miniz or libzip) is compiled in. With NSCP_ZIP_BACKEND
# "none" the stub reports every archive as unreadable, so the test is skipped.
if(NOT NSCP_ZIP_BACKEND STREQUAL "none")
    NSCP_CREATE_TEST(
        unzip_test
        SOURCES
            ${NSCP_INCLUDEDIR}/bytes/unzip_test.cpp
            ${NSCP_INCLUDEDIR}/bytes/unzip.cpp
        LIBRARIES
            GTest::gtest_main
            nscp_miniz
            ${Boost_FILESYSTEM_LIBRARY}
        INCLUDES
            ${NSCP_INCLUDEDIR}
    )
endif()

NSCP_CREATE_TEST(
    threads_test
    SOURCES
        ${NSCP_INCLUDEDIR}/threads/queue_test.cpp
        ${NSCP_INCLUDEDIR}/threads/concurrent_queue_test.cpp
        ${NSCP_INCLUDEDIR}/threads/scoped_thread_group_test.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_THREAD_LIBRARY}
        ${Boost_CHRONO_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    check_logfile_test
    SOURCES
        ${CMAKE_SOURCE_DIR}/modules/CheckLogFile/file_reader_test.cpp
        ${CMAKE_SOURCE_DIR}/modules/CheckLogFile/file_reader.hpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_SOURCE_DIR}/modules/CheckLogFile
)

NSCP_CREATE_TEST(
    icinga_client_test
    SOURCES
        ${CMAKE_SOURCE_DIR}/modules/IcingaClient/icinga_test.cpp
        ${CMAKE_SOURCE_DIR}/modules/IcingaClient/icinga.cpp
        ${NSCP_INCLUDEDIR}/json/use_json.cpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_SOURCE_DIR}/modules/IcingaClient
)


set(SETTINGS_INI_TEST_SOURCES
    ${NSCP_INCLUDEDIR}/settings/impl/settings_ini_test.cpp
    ${NSCP_INCLUDEDIR}/str/utf8.cpp
)
if(WIN32)
    list(
        APPEND SETTINGS_INI_TEST_SOURCES
        ${NSCP_INCLUDEDIR}/win/credentials.cpp
    )
    list(
        APPEND SETTINGS_INI_TEST_SOURCES
        ${NSCP_INCLUDEDIR}/error/error_w32.cpp
    )
endif()

NSCP_CREATE_TEST(
    test_settings
    SOURCES
        ${NSCP_INCLUDEDIR}/settings/settings_value_test.cpp
        ${NSCP_INCLUDEDIR}/settings/settings_interface_impl_test.cpp
        ${NSCP_INCLUDEDIR}/settings/impl/settings_dummy_test.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
        ${SETTINGS_INI_TEST_SOURCES}
    LIBRARIES
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

if(OPENSSL_FOUND)
    NSCP_CREATE_TEST(
        settings_http_test
        SOURCES
            ${NSCP_INCLUDEDIR}/settings/impl/settings_http_test.cpp
            ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
            ${NSCP_INCLUDEDIR}/bytes/base64.c
            ${NSCP_INCLUDEDIR}/str/utf8.cpp
        LIBRARIES
            GTest::gtest_main
            ${Boost_FILESYSTEM_LIBRARY}
            ${Boost_THREAD_LIBRARY}
            ${Boost_DATE_TIME_LIBRARY}
            ${OPENSSL_LIBRARIES}
            $<$<PLATFORM_ID:Windows>:CRYPT32>
        INCLUDES
            ${NSCP_INCLUDEDIR}
            ${OPENSSL_INCLUDE_DIR}
    )
    target_compile_definitions(settings_http_test PRIVATE USE_SSL)
endif()

NSCP_CREATE_TEST(
    file_helpers_test
    SOURCES
        ${NSCP_INCLUDEDIR}/file_helpers_test.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    pair_hash_test
    SOURCES
        ${NSCP_INCLUDEDIR}/algorithms/pair_hash_test.cpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    handle_test
    SOURCES
        ${NSCP_INCLUDEDIR}/handle_test.cpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

NSCP_CREATE_TEST(
    rrd_buffer_test
    SOURCES
        ${NSCP_INCLUDEDIR}/rrd_buffer_test.cpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}
)

# Lua acceptance tests (protobuf-free) - drive the built `nscp` over the Lua
# scripting API. Registered as ctest `*_test` targets so they run under the
# sanitizer environment via tools/sanitizers/run.sh (ctest -R '_test$').
# Each is guarded by the modules it exercises.
if(BUILD_MODULE_LUAScript)
    NSCP_ADD_LUA_TEST(lua_selfcheck_test test_selfcheck)
    if(BUILD_MODULE_CheckHelpers)
        NSCP_ADD_LUA_TEST(lua_checks_test test_checks)
        NSCP_ADD_LUA_TEST(lua_check_helpers_test test_check_helpers)
    endif()
    # check_uptime lives in CheckSystem(Unix), which builds under BUILD_MODULE_CheckSystem.
    if(BUILD_MODULE_CheckSystem)
        NSCP_ADD_LUA_TEST(lua_uptime_test test_uptime)
    endif()
    if(BUILD_MODULE_NSCAServer AND BUILD_MODULE_NSCAClient)
        NSCP_ADD_LUA_TEST(lua_nsca_test test_nsca)
    endif()
    if(BUILD_MODULE_CheckHelpers AND BUILD_MODULE_NRPEServer AND BUILD_MODULE_NRPEClient)
        NSCP_ADD_LUA_TEST(lua_nrpe_relay_test test_nrpe_relay)
    endif()
    # test_nrpe registers its own check_echo handler, so it only needs the NRPE
    # transport modules (no CheckHelpers).
    if(BUILD_MODULE_NRPEServer AND BUILD_MODULE_NRPEClient)
        NSCP_ADD_LUA_TEST(lua_nrpe_test test_nrpe)
    endif()
    if(BUILD_MODULE_CheckExternalScripts)
        NSCP_ADD_LUA_TEST(lua_ext_script_test test_ext_script)
    endif()
    if(BUILD_MODULE_CheckLogFile)
        NSCP_ADD_LUA_TEST(lua_log_file_test test_log_file)
    endif()
    if(BUILD_MODULE_Scheduler)
        NSCP_ADD_LUA_TEST(lua_scheduler_test test_scheduler)
    endif()
endif()
