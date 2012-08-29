cmake_minimum_required(VERSION 2.6)

SET(TARGET NRPEServer)
	
PROJECT(${TARGET})

# Workaround for bug 0003353 in cmake (aparently not fixed)
SET_SOURCE_FILES_PROPERTIES(${PB_PLUGIN} PROPERTIES GENERATED TRUE)

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	"handler_impl.cpp"
	${NSCP_INCLUDEDIR}/nrpe/server/server.cpp
	${NSCP_INCLUDEDIR}/nrpe/server/connection.cpp
	${NSCP_INCLUDEDIR}/nrpe/server/tcp_connection.cpp
	${NSCP_INCLUDEDIR}/nrpe/server/ssl_connection.cpp
	${NSCP_INCLUDEDIR}/nrpe/packet.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DUSE_SSL)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
ENDIF(OPENSSL_FOUND)

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		"handler_impl.hpp"
		${NSCP_INCLUDEDIR}/nrpe/server/server.hpp
		${NSCP_INCLUDEDIR}/nrpe/server/connection.hpp
		${NSCP_INCLUDEDIR}/nrpe/server/tcp_connection.hpp
		${NSCP_INCLUDEDIR}/nrpe/server/ssl_connection.hpp
		${NSCP_INCLUDEDIR}/nrpe/server/handler.hpp
		${NSCP_INCLUDEDIR}/nrpe/server/parser.hpp
		${NSCP_INCLUDEDIR}/nrpe/packet.hpp
		${NSCP_INCLUDEDIR}/swap_bytes.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Server" REGULAR_EXPRESSION .*include/nrpe/.*)
SOURCE_GROUP("Socket" REGULAR_EXPRESSION .*include/socket/.*)
