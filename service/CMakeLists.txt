cmake_minimum_required(VERSION 2.6)

SET(TARGET nscp)

PROJECT(service)
INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})
#INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR})

SET(VERSION_HPP ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp)

ADD_CUSTOM_TARGET(
	"${TARGET}_VERSION"
	COMMAND ${PYTHON_EXECUTABLE}
		"${BUILD_PYTHON_FOLDER}/version.py"
		--file ${VERSION_TXT}
		--generate-hpp ${VERSION_HPP}
		--update build
	)

SET(service_SRCS
	StdAfx.cpp
	nsclient++.cpp
	NSCPlugin.cpp
	core_api.cpp
	settings_manager_impl.cpp
	settings_logger_impl.cpp

	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
	${NSCP_INCLUDEDIR}/settings/client/settings_client.cpp

	${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
	${NSCP_INCLUDEDIR}/b64/b64.c
	${NSCP_INCLUDEDIR}/arrayBuffer.cpp

)

IF(WIN32)
	SET(service_SRCS ${service_SRCS}
		${NSCP_INCLUDEDIR}/ServiceCmd.cpp
	)
	SET(service_SRCS ${service_SRCS}
		StdAfx.h
		NSClient++.h
		NSCPlugin.h
		core_api.h
		settings_manager_impl.h
		settings_logger_impl.hpp
		simple_client.hpp
		settings_client.hpp
		cli_parser.hpp
		
		commands.hpp
		channels.hpp
		logger.hpp
		settings_logger_impl.hpp
		service_manager.hpp
		
		${NSCP_INCLUDEDIR}/settings/Settings.h
		${NSCP_INCLUDEDIR}/charEx.h
		${NSCP_INCLUDEDIR}/config.h
		${NSCP_INCLUDEDIR}/msvc_wrappers.h
		${NSCP_INCLUDEDIR}/settings/macros.h
		${NSCP_INCLUDEDIR}/settings/settings_ini.hpp
		${NSCP_INCLUDEDIR}/settings/settings_old.hpp
		${NSCP_INCLUDEDIR}/settings/settings_registry.hpp
		${NSCP_INCLUDEDIR}/settings/settings_core.hpp
		${NSCP_INCLUDEDIR}/settings/settings_core_impl.hpp
		${NSCP_INCLUDEDIR}/settings/settings_handler_impl.hpp
		${NSCP_INCLUDEDIR}/settings/settings_interface_impl.hpp
		${NSCP_INCLUDEDIR}/settings/client/settings_client.hpp
		${NSCP_INCLUDEDIR}/com_helpers.hpp
		${NSCP_INCLUDEDIR}/dll/dll.hpp
		${NSCP_INCLUDEDIR}/dll/impl_unix.hpp
		${NSCP_INCLUDEDIR}/dll/impl_w32.hpp
#		${NSCP_INCLUDEDIR}/nsclient_session.hpp

		${NSCP_INCLUDEDIR}/service/system_service.hpp
		${NSCP_INCLUDEDIR}/service/win32_service.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp

	)
	SET(service_SRCS ${service_SRCS}
		${NSCP_INCLUDEDIR}/service/unix_service.hpp
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


add_executable (${TARGET} ${service_SRCS})
add_dependencies(${TARGET} ${TARGET}_VERSION)

MESSAGE(STATUS "Extra: ${EXTRA_LIBS}")

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
)
SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "core")
SET_TARGET_PROPERTIES(${TARGET}_VERSION PROPERTIES FOLDER "core")

SOURCE_GROUP("Common Files" REGULAR_EXPRESSION .*include/.*)
SOURCE_GROUP("Parser" REGULAR_EXPRESSION .*include/parser/.*)
SOURCE_GROUP("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
SOURCE_GROUP("Settings" REGULAR_EXPRESSION .*include/settings/.*)

#SOURCE_GROUP("NSCP API/Sources" FILES ${NSCP_DEF_PLUGIN_CPP})
#SOURCE_GROUP("Settings" REGULAR_EXPRESSION .*settings.*)

INSTALL(TARGETS ${TARGET} RUNTIME DESTINATION .)

