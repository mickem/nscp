cmake_minimum_required(VERSION 2.6)

SET(TARGET check_nrpe)

PROJECT(clients)

SET(SRCS ${SRCS}
	check_nrpe.cpp
	../../modules/NRPEClient/nrpe_client.cpp
	${NSCP_INCLUDEDIR}/nrpe/packet.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
	${NSCP_INCLUDEDIR}/client/command_line_parser.cpp
	${NSCP_INCLUDEDIR}/utils.cpp
	
#	${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_types.cpp
#	${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_functions.cpp
#	${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.cpp
#	${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.cpp
#	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp

	../../resources/nsclient.ico
)

IF(WIN32)
	SET(SRCS ${SRCS}
		check_nrpe.hpp
		../../modules/NRPEClient/nrpe_client.hpp
		${NSCP_INCLUDEDIR}/utils.h
		${NSCP_INCLUDEDIR}/nrpe/packet.hpp
		${NSCP_INCLUDEDIR}/nrpe/client/nrpe_client_protocol.hpp
		${NSCP_INCLUDEDIR}/swap_bytes.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
		${NSCP_INCLUDEDIR}/socket/client.hpp
		${NSCP_INCLUDEDIR}/client/command_line_parser.hpp
	)
ENDIF(WIN32)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
IF(JSON_SPIRIT_FOUND)
	INCLUDE_DIRECTORIES(${JSON_SPIRIT_INCLUDE_DIR})
	ADD_DEFINITIONS(-DHAVE_JSON_SPIRIT)
	SET(JSON_LIB json_spirit)
ELSE(JSON_SPIRIT_FOUND)
	SET(JSON_LIB)
ENDIF(JSON_SPIRIT_FOUND)
LINK_DIRECTORIES(${BOOST_LIB_DIRS})

IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DUSE_SSL)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
ENDIF(OPENSSL_FOUND)

SETUP_BREAKPAD(SRCS)

NSCP_MAKE_EXE_BIN(${TARGET} "${SRCS}")
IF(WIN32 AND NOT CMAKE_CL_64)
	OPENSSL_LINK_FIX(${TARGET})
ENDIF(WIN32 AND NOT CMAKE_CL_64)

TARGET_LINK_LIBRARIES(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
)
SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "clients")

SOURCE_GROUP("Client" REGULAR_EXPRESSION .*include/nrpe/.*)
SOURCE_GROUP("Socket" REGULAR_EXPRESSION .*include/socket/.*)
SOURCE_GROUP("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
SOURCE_GROUP("Settings" REGULAR_EXPRESSION .*include/settings/.*)
SOURCE_GROUP("Common Files" REGULAR_EXPRESSION .*include/.*)

