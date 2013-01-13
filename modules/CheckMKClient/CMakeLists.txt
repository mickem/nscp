cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckMKClient)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
	${NSCP_INCLUDEDIR}/client/command_line_parser.cpp
	${NSCP_INCLUDEDIR}/check_mk/lua/lua_check_mk.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
INCLUDE_DIRECTORIES(${LUA_INCLUE_DIR})
IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DUSE_SSL)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
ENDIF(OPENSSL_FOUND)

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		${NSCP_INCLUDEDIR}/check_mk/data.hpp
		${NSCP_INCLUDEDIR}/check_mk/parser.hpp
		${NSCP_INCLUDEDIR}/check_mk/client/client_protocol.hpp
		${NSCP_INCLUDEDIR}/check_mk/lua/lua_check_mk.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
		${NSCP_INCLUDEDIR}/socket/client.hpp
		${NSCP_INCLUDEDIR}/client/command_line_parser.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})
OPENSSL_LINK_FIX(${TARGET})

SET(LUA_PB)
IF (PROTOC_GEN_LUA_FOUND)
	SET(LUA_PB lua_pb)
ENDIF()

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
	${LUA_LIB}
	lua_nscp
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Client" REGULAR_EXPRESSION .*include/check_mk/.*)
SOURCE_GROUP("Socket" REGULAR_EXPRESSION .*include/socket/.*)
