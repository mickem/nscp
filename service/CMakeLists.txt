cmake_minimum_required(VERSION 2.6)

SET(TARGET nscp)

PROJECT(service)
INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
IF(JSON_SPIRIT_FOUND)
	INCLUDE_DIRECTORIES(${JSON_SPIRIT_INCLUDE_DIR})
	ADD_DEFINITIONS(-DJSON_SPIRIT)
	SET(JSON_LIB json_spirit_static)
ELSE(JSON_SPIRIT_FOUND)
	SET(JSON_LIB)
ENDIF(JSON_SPIRIT_FOUND)


LINK_DIRECTORIES(${BOOST_LIB_DIRS})

SET(service_SRCS
	StdAfx.cpp
	NSClient++.cpp
	NSCPlugin.cpp
	core_api.cpp
	logger_impl.cpp

	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
	${NSCP_INCLUDEDIR}/settings/client/settings_client.cpp
	${NSCP_INCLUDEDIR}/nsclient/base_logger_impl.cpp

	${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
	${NSCP_INCLUDEDIR}/b64/b64.c
	${NSCP_INCLUDEDIR}/arrayBuffer.cpp

	../resources/nsclient.ico
)

IF(WIN32)
	SET(service_SRCS ${service_SRCS}
		${NSCP_INCLUDEDIR}/ServiceCmd.cpp
	)
	SET(service_SRCS ${service_SRCS}
		StdAfx.h
		NSClient++.h
		NSClient++.rc
		NSCPlugin.h
		core_api.h
		simple_client.hpp
		settings_client.hpp
		cli_parser.hpp
		
		plugin_list.hpp
		commands.hpp
		channels.hpp
		routers.hpp
		logger_impl.hpp
		service_manager.hpp

		${NSCP_INCLUDEDIR}/charEx.h
		${NSCP_INCLUDEDIR}/config.h
		${NSCP_INCLUDEDIR}/com_helpers.hpp
		${NSCP_INCLUDEDIR}/dll/dll.hpp
		${NSCP_INCLUDEDIR}/dll/impl_unix.hpp
		${NSCP_INCLUDEDIR}/dll/impl_w32.hpp
#		${NSCP_INCLUDEDIR}/nsclient_session.hpp

		${NSCP_INCLUDEDIR}/service/system_service.hpp
		${NSCP_INCLUDEDIR}/service/win32_service.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
		${NSCP_INCLUDEDIR}/nsclient/base_logger_impl.hpp
		${NSCP_INCLUDEDIR}/nsclient/logger.hpp

	)
	SET(service_SRCS ${service_SRCS}
		${CMAKE_CURRENT_SOURCE_DIR}/NSClient++.manifest
	)
	
	ADD_DEFINITIONS(-D_WIN32_DCOM)

	
ENDIF(WIN32)
IF(BREAKPAD_FOUND)
	SET(service_SRCS ${service_SRCS} ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.cpp)
	IF(WIN32)
		SET(service_SRCS ${service_SRCS} ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.hpp)
	ENDIF(WIN32)
	SET(EXTRA_LIBS ${BREAKPAD_COMMON_LIBRARY} ${BREAKPAD_EXCEPTION_HANDLER_LIBRARY} ${BREAKPAD_CRASH_GENERATION_CLIENT_LIBRARY})
	ADD_DEFINITIONS(-DUSE_BREAKPAD)
ENDIF(BREAKPAD_FOUND)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

add_executable (${TARGET} ${service_SRCS})

MESSAGE(STATUS "All libraries: ${Boost_FILESYSTEM_LIBRARY}, ${NSCP_DEF_PLUGIN_LIB}, ${EXTRA_LIBS}")

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
	${JSON_LIB}
	settings_manager
)
#IF(WIN32)
#	SET_TARGET_PROPERTIES(${TARGET} PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS")
#ENDIF(WIN32)
INSTALL(TARGETS ${TARGET} RUNTIME DESTINATION .)
IF(WIN32)
	INSTALL(CODE "SET(PDB_FULL_PATH ${BUILD_TARGET_EXE_PATH}/${TARGET}.pdb)")
	INSTALL(CODE "FILE(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX} TYPE EXECUTABLE FILES \${PDB_FULL_PATH})")
ENDIF(WIN32)

SOURCE_GROUP("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
SOURCE_GROUP("Settings" REGULAR_EXPRESSION .*include/settings/.*)
SOURCE_GROUP("Common Files" REGULAR_EXPRESSION .*include/.*)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "core")
