cmake_minimum_required(VERSION 2.6)

SET(TARGET NSCAClient)
	
PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

SET(SRCS ${SRCS}
	stdafx.cpp
	"${TARGET}.cpp"
	${NSCP_INCLUDEDIR}/nsca/nsca_packet.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
	${NSCP_INCLUDEDIR}/client/command_line_parser.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DUSE_SSL)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
ENDIF(OPENSSL_FOUND)

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		${NSCP_INCLUDEDIR}/nsca/nsca_packet.hpp
		${NSCP_INCLUDEDIR}/nsca/client/nsca_client_protocol.hpp
		${NSCP_INCLUDEDIR}/cryptopp/cryptopp.hpp
		${NSCP_INCLUDEDIR}/swap_bytes.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
		${NSCP_INCLUDEDIR}/socket/client.hpp
		${NSCP_INCLUDEDIR}/client/command_line_parser.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
	cryptopp
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Client" REGULAR_EXPRESSION .*include/nsca/.*)
SOURCE_GROUP("Socket" REGULAR_EXPRESSION .*include/socket/.*)
