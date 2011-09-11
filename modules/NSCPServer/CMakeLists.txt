cmake_minimum_required(VERSION 2.6)

SET(TARGET NSCPServer)
	
PROJECT(${TARGET})

# Workaround for bug 0003353 in cmake (aparently not fixed)
SET_SOURCE_FILES_PROPERTIES(${PB_PLUGIN} PROPERTIES GENERATED TRUE)

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	"handler_impl.cpp"
	${NSCP_INCLUDEDIR}/nscp/server/server.cpp
	${NSCP_INCLUDEDIR}/nscp/server/connection.cpp
	${NSCP_INCLUDEDIR}/nscp/server/tcp_connection.cpp
	${NSCP_INCLUDEDIR}/nscp/server/ssl_connection.cpp
	${NSCP_INCLUDEDIR}/nscp/packet.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
	${NSCP_INCLUDEDIR}/nscp/handler.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		"handler_impl.hpp"
		${NSCP_INCLUDEDIR}/nscp/server/server.hpp
		${NSCP_INCLUDEDIR}/nscp/server/connection.hpp
		${NSCP_INCLUDEDIR}/nscp/server/tcp_connection.hpp
		${NSCP_INCLUDEDIR}/nscp/server/ssl_connection.hpp
		${NSCP_INCLUDEDIR}/nscp/server/handler.hpp
		${NSCP_INCLUDEDIR}/nscp/server/parser.hpp
		${NSCP_INCLUDEDIR}/nscp/packet.hpp
		${NSCP_INCLUDEDIR}/nscp/handler.hpp
		${NSCP_INCLUDEDIR}/swap_bytes.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${OPENSSL_LIBRARIES}
)

INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Server" REGULAR_EXPRESSION .*include/nscp/.*)
