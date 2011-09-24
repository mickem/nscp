cmake_minimum_required(VERSION 2.6)

SET(TARGET NSCAServer)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	"handler_impl.cpp"
	${NSCP_INCLUDEDIR}/nsca/server/server.cpp
	${NSCP_INCLUDEDIR}/nsca/server/connection.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
	${NSCP_INCLUDEDIR}/nsca/nsca_packet.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

IF(HAVE_CRYPTOPP)
	SET(CRYPTOPP_LIB cryptopp_static)
	ADD_DEFINITIONS(-DHAVE_LIBCRYPTOPP)
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
ELSE(HAVE_CRYPTOPP)
	message(STATUS "WARNING: No libCrypto++ in NSCA Module")
ENDIF(HAVE_CRYPTOPP)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		"handler_impl.hpp"
		${NSCP_INCLUDEDIR}/nsca/server/server.hpp
		${NSCP_INCLUDEDIR}/nsca/server/connection.hpp
		${NSCP_INCLUDEDIR}/nsca/server/handler.hpp
		${NSCP_INCLUDEDIR}/nsca/server/parser.hpp
		${NSCP_INCLUDEDIR}/nsca/nsca_packet.hpp
		${NSCP_INCLUDEDIR}/swap_bytes.hpp
		${NSCP_INCLUDEDIR}/socket/server.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${CRYPTOPP_LIB}
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Server" REGULAR_EXPRESSION .*include/nsca/.*)
