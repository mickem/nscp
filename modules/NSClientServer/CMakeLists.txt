cmake_minimum_required(VERSION 2.6)

SET(TARGET NSClientServer)
	
PROJECT(${TARGET})

# Workaround for bug 0003353 in cmake (aparently not fixed)
SET_SOURCE_FILES_PROPERTIES(${PB_PLUGIN} PROPERTIES GENERATED TRUE)

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	"handler_impl.cpp"
	${NSCP_INCLUDEDIR}/check_nt/server/server.cpp
	${NSCP_INCLUDEDIR}/check_nt/server/connection.cpp
	${NSCP_INCLUDEDIR}/check_nt/server/tcp_connection.cpp
	${NSCP_INCLUDEDIR}/check_nt/server/ssl_connection.cpp
	${NSCP_INCLUDEDIR}/check_nt/packet.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DUSE_SSL)
ENDIF(OPENSSL_FOUND)

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		"handler_impl.hpp"
		${NSCP_INCLUDEDIR}/check_nt/server/server.hpp
		${NSCP_INCLUDEDIR}/check_nt/server/connection.hpp
		${NSCP_INCLUDEDIR}/check_nt/server/tcp_connection.hpp
		${NSCP_INCLUDEDIR}/check_nt/server/ssl_connection.hpp
		${NSCP_INCLUDEDIR}/check_nt/server/handler.hpp
		${NSCP_INCLUDEDIR}/check_nt/server/parser.hpp
		${NSCP_INCLUDEDIR}/check_nt/packet.hpp
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
	${OPENSSL_LIBRARIES}
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
