cmake_minimum_required(VERSION 2.6)

PROJECT(service)
INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})
#INCLUDE_DIRECTORIES(${NSCP_INCLUDE_PATH})

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

SET(service_SRCS
	StdAfx.cpp
	NSClient++.cpp
	NSCPlugin.cpp
	core_api.cpp
	settings_manager_impl.cpp

	${NSCP_DEF_PLUGIN_CPP}

	${NSCP_INCLUDE_PATH}/simpleini/ConvertUTF.c
	${NSCP_INCLUDE_PATH}/b64/b64.c

)

IF(WIN32)
	SET(service_SRCS ${service_SRCS}
		${NSCP_INCLUDE_PATH}/ServiceCmd.cpp
	)
	SET(service_SRCS ${service_SRCS}
		StdAfx.h
		NSClient++.h
		NSCPlugin.h
		core_api.h
		settings_manager_impl.h
		simple_client.hpp
		settings_client.hpp
		
		commands.hpp
		logger.hpp
		settings_logger_impl.hpp
		service_manager.hpp
		
		${NSCP_INCLUDE_PATH}/settings/Settings.h
		${NSCP_INCLUDE_PATH}/charEx.h
		${NSCP_INCLUDE_PATH}/config.h
		${NSCP_INCLUDE_PATH}/msvc_wrappers.h
		${NSCP_INCLUDE_PATH}/settings/macros.h
		${NSCP_INCLUDE_PATH}/settings/settings_ini.hpp
		${NSCP_INCLUDE_PATH}/settings/settings_old.hpp
		${NSCP_INCLUDE_PATH}/settings/settings_registry.hpp
		${NSCP_INCLUDE_PATH}/settings/Settings.h
		${NSCP_INCLUDE_PATH}/com_helpers.hpp
		${NSCP_INCLUDE_PATH}/dll/dll.hpp
		${NSCP_INCLUDE_PATH}/dll/impl_unix.hpp
		${NSCP_INCLUDE_PATH}/dll/impl_w32.hpp
#		${NSCP_INCLUDE_PATH}/nsclient_session.hpp

		${NSCP_INCLUDE_PATH}/service/system_service.hpp
		${NSCP_INCLUDE_PATH}/service/win32_service.hpp

		${NSCP_DEF_PLUGIN_HPP}

	)
	SET(service_SRCS ${service_SRCS}
		${NSCP_INCLUDE_PATH}/service/unix_service.hpp
	)
	
	ADD_DEFINITIONS(-D_WIN32_DCOM)

ENDIF(WIN32)

add_executable (nsclient++ ${service_SRCS})

target_link_libraries(nsclient++ 
	${Boost_FILESYSTEM_LIBRARY}
	${PROTOBUF_LIBRARY}
)

INSTALL(FILES ${PROJECT_SOURCE_DIR}/cmake/FindWt.cmake DESTINATION
    ${CMAKE_INSTALL_PREFIX}/${WT_CMAKE_FINDER_INSTALL_DIR} )

