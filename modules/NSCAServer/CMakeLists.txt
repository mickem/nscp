cmake_minimum_required(VERSION 2.6)

SET(TARGET NSCAServer)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	"handler_impl.cpp"
	${NSCP_INCLUDEDIR}/nsca/nsca_packet.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
IF(OPENSSL_FOUND)
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
	ADD_DEFINITIONS(-DUSE_SSL)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
ENDIF(OPENSSL_FOUND)

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		"handler_impl.hpp"
		${NSCP_INCLUDEDIR}/nsca/server/protocol.hpp
		${NSCP_INCLUDEDIR}/nsca/server/handler.hpp
		${NSCP_INCLUDEDIR}/nsca/server/parser.hpp
		${NSCP_INCLUDEDIR}/nsca/nsca_packet.hpp
		${NSCP_INCLUDEDIR}/swap_bytes.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
		${NSCP_INCLUDEDIR}/socket/server.hpp
		${NSCP_INCLUDEDIR}/socket/connection.hpp
		${NSCP_INCLUDEDIR}/cryptopp/cryptopp.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
	cryptopp
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Server" REGULAR_EXPRESSION .*include/nsca/.*)
SOURCE_GROUP("Socket" REGULAR_EXPRESSION .*include/socket/.*)
