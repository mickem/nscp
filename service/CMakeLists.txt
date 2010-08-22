cmake_minimum_required(VERSION 2.6)

PROJECT(service)
INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})
#INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR})

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

# Workaround for bug 0003353 in cmake (aparently not fixed)
#WRAP_PROTO(PB_PLUGIN ${PB_PATH}/plugin.pb)
#SET_SOURCE_FILES_PROPERTIES(${PB_PLUGIN} PROPERTIES GENERATED TRUE)

SET(service_SRCS
	StdAfx.cpp
	NSClient++.cpp
	NSCPlugin.cpp
	core_api.cpp
	settings_manager_impl.cpp

	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp

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

add_executable (nsclient++ ${service_SRCS})

target_link_libraries(nsclient++ 
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
)

INSTALL(FILES ${PROJECT_SOURCE_DIR}/cmake/FindWt.cmake DESTINATION
    ${CMAKE_INSTALL_PREFIX}/${WT_CMAKE_FINDER_INSTALL_DIR} )

