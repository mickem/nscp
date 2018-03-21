cmake_minimum_required(VERSION 2.6)

SET(TARGET nscp)

PROJECT(service)
INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})

SET(service_SRCS
	NSClient++.cpp
	core_api.cpp
	plugin_manager.cpp
	master_plugin_list.cpp
	path_manager.cpp
	dll_plugin.cpp


	#logger
	logger/nsclient_logger.cpp
	logger/simple_console_logger.cpp
	logger/simple_file_logger.cpp
	logger/threaded_logger.cpp
  
	settings_query_handler.cpp
	registry_query_handler.cpp
	storage_query_handler.cpp
	plugin_cache.cpp
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

	../resources/nsclient.ico
)

IF(WIN32)
	SET(service_SRCS ${service_SRCS}
		${NSCP_INCLUDEDIR}/ServiceCmd.cpp
	)
	SET(service_SRCS ${service_SRCS}
		NSClient++.h
		NSClient++.rc
		core_api.h
		settings_client.hpp
		cli_parser.hpp
		${NSCP_INCLUDEDIR}/scheduler/simple_scheduler.hpp
		scheduler_handler.hpp
		plugin_manager.hpp
		master_plugin_list.hpp
		path_manager.hpp
		dll_plugin.h
		plugin_interface.hpp
		storage_manager.hpp

		
		plugin_list.hpp
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
		plugin_cache.hpp

		service_manager.hpp

		${NSCP_INCLUDEDIR}/config.h
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

	)
	SET(service_SRCS ${service_SRCS}
		${CMAKE_CURRENT_SOURCE_DIR}/NSClient++.manifest
	)
	
	ADD_DEFINITIONS(-D_WIN32_DCOM)
ENDIF(WIN32)

IF(JSON_SPIRIT_FOUND)
	INCLUDE_DIRECTORIES(${JSON_SPIRIT_INCLUDE_DIR})
	ADD_DEFINITIONS(-DHAVE_JSON_SPIRIT)
	ADD_DEFINITIONS(-DJSON_SPIRIT_STATIC_LINKED)
	SET(JSON_LIB json_spirit nscp_json_pb)
  
	SET(service_SRCS ${service_SRCS}
    zip_plugin.cpp
    zip_plugin.h
  )
  
ELSE(JSON_SPIRIT_FOUND)
	SET(JSON_LIB)
ENDIF(JSON_SPIRIT_FOUND)

IF (PROTOC_GEN_JSON_FOUND)
	ADD_DEFINITIONS(-DHAVE_JSON_PB)
	INCLUDE_DIRECTORIES(${NSCP_PROJECT_BINARY_DIR}/libs/json_pb)
	INCLUDE_DIRECTORIES(${NSCP_PROJECT_BINARY_DIR}/libs/protobuf)
ENDIF()
ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES} -Dplugin_api_NOLIB)

# Add python
IF(PYTHONLIBS_FOUND)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${PYTHON_LIBRARIES})
ENDIF(PYTHONLIBS_FOUND)

SETUP_BREAKPAD(service_SRCS)

NSCP_MAKE_EXE_SBIN(${TARGET} "${service_SRCS}")
SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "core")

TARGET_LINK_LIBRARIES(${TARGET}
	${CMAKE_THREAD_LIBS_INIT}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${Boost_SYSTEM_LIBRARY}
	${PROTOBUF_LIBRARY}
	${ICONV_LIBRARIES}
	${EXTRA_LIBS}
	${JSON_LIB}
	nscp_protobuf
	settings_manager
	expression_parser
  nscp_miniz
)

SOURCE_GROUP("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
SOURCE_GROUP("Settings" REGULAR_EXPRESSION .*include/settings/.*)
#SOURCE_GROUP("Common Files" REGULAR_EXPRESSION .*include/.*)
SOURCE_GROUP("Log" REGULAR_EXPRESSION .*/logger/.*)

IF(GTEST_FOUND)
	INCLUDE_DIRECTORIES(${GTEST_INCLUDE_DIR})
	SET(TEST_SRCS
		various_test.cpp
		performance_data_test.cpp
		cron_test.cpp
		../include/parsers/cron/cron_parser.hpp
		
		../include/nscapi/nscapi_protobuf_functions.cpp
		../include/nscapi/nscapi_protobuf_functions.hpp
	)
	NSCP_MAKE_EXE_TEST(${TARGET}_test "${TEST_SRCS}")
	NSCP_ADD_TEST(${TARGET}_test ${TARGET}_test)
	TARGET_LINK_LIBRARIES(${TARGET}_test
		${GTEST_GTEST_LIBRARY}
		${GTEST_GTEST_MAIN_LIBRARY}
		${NSCP_DEF_PLUGIN_LIB}
		${EXTRA_LIBS}
		${Boost_DATE_TIME_LIBRARY}
		settings_manager
	)
ENDIF(GTEST_FOUND)
