cmake_minimum_required(VERSION 2.6)

SET(TARGET settings_manager)

INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})

SET(EXTRA_LIBS)

SET(service_SRCS
	settings_manager_impl.cpp
	settings_handler_impl.cpp

	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
#	${NSCP_INCLUDEDIR}/settings/client/settings_client.cpp
	${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
)

IF(WIN32)
	SET(service_SRCS ${service_SRCS}
		settings_manager_impl.h
		settings_handler_impl.hpp
		
		${NSCP_INCLUDEDIR}/config.h

		${NSCP_INCLUDEDIR}/settings/settings_core.hpp
		${NSCP_INCLUDEDIR}/settings/settings_interface_impl.hpp


		${NSCP_INCLUDEDIR}/settings/impl/settings_ini.hpp
		${NSCP_INCLUDEDIR}/settings/impl/settings_dummy.hpp
		${NSCP_INCLUDEDIR}/settings/impl/settings_old.hpp
		${NSCP_INCLUDEDIR}/settings/impl/settings_registry.hpp
		${NSCP_INCLUDEDIR}/settings/impl/settings_http.hpp

		${NSCP_INCLUDEDIR}/http/client.hpp

	)
ENDIF(WIN32)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DUSE_SSL)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
ENDIF(OPENSSL_FOUND)

IF(CRYPTOPP_FOUND)
	ADD_DEFINITIONS(-DHAVE_LIBCRYPTOPP)
	INCLUDE_DIRECTORIES(${CRYPTOPP_INCLUDE_DIR})
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${CRYPTOPP_LIBRARIES})
ENDIF(CRYPTOPP_FOUND)



add_library (${TARGET} STATIC ${service_SRCS})

target_link_libraries(${TARGET} ${EXTRA_LIBS} nscp_protobuf)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "core")

SOURCE_GROUP("Common Files" REGULAR_EXPRESSION .*include/.*)
SOURCE_GROUP("Settings" REGULAR_EXPRESSION .*include/settings/.*)
