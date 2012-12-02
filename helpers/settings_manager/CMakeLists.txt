cmake_minimum_required(VERSION 2.6)

SET(TARGET settings_manager)

INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})

SET(service_SRCS
	settings_manager_impl.cpp
	settings_handler_impl.cpp
	StdAfx.cpp

	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
	${NSCP_INCLUDEDIR}/settings/client/settings_client.cpp
	${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
)

IF(WIN32)
	SET(service_SRCS ${service_SRCS}
		StdAfx.h
		settings_manager_impl.h
		settings_handler_impl.hpp
		
		${NSCP_INCLUDEDIR}/charEx.h
		${NSCP_INCLUDEDIR}/config.h
		${NSCP_INCLUDEDIR}/settings/macros.h

		${NSCP_INCLUDEDIR}/settings/settings_core.hpp
		${NSCP_INCLUDEDIR}/settings/settings_interface_impl.hpp
		${NSCP_INCLUDEDIR}/settings/client/settings_client.hpp


		${NSCP_INCLUDEDIR}/settings/impl/settings_ini.hpp
		${NSCP_INCLUDEDIR}/settings/impl/settings_dummy.hpp
		${NSCP_INCLUDEDIR}/settings/impl/settings_old.hpp
		${NSCP_INCLUDEDIR}/settings/impl/settings_registry.hpp
		${NSCP_INCLUDEDIR}/settings/impl/settings_http.hpp

		${NSCP_INCLUDEDIR}/http/client.hpp

	)
ENDIF(WIN32)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

add_library (${TARGET} ${service_SRCS})

target_link_libraries(${TARGET} protobuf)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "core")

SOURCE_GROUP("Common Files" REGULAR_EXPRESSION .*include/.*)
SOURCE_GROUP("Settings" REGULAR_EXPRESSION .*include/settings/.*)
