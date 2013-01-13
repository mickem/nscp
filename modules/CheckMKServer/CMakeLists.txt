cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckMKServer)
	
PROJECT(${TARGET})

# Workaround for bug 0003353 in cmake (aparently not fixed)
SET_SOURCE_FILES_PROPERTIES(${PB_PLUGIN} PROPERTIES GENERATED TRUE)

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	"handler_impl.cpp"
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
	${NSCP_INCLUDEDIR}/check_mk/lua/lua_check_mk.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
INCLUDE_DIRECTORIES(${LUA_INCLUE_DIR})
IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DUSE_SSL)
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
ENDIF(OPENSSL_FOUND)

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		"handler_impl.hpp"
		${NSCP_INCLUDEDIR}/check_mk/server/server_protocol.hpp
		${NSCP_INCLUDEDIR}/check_mk/server/server_handler.hpp
		${NSCP_INCLUDEDIR}/check_mk/parser.hpp
		${NSCP_INCLUDEDIR}/check_mk/data.hpp
		${NSCP_INCLUDEDIR}/check_mk/lua/lua_check_mk.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
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
	${Boost_THREAD_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${OPENSSL_LIBRARIES}
	${LUA_LIB}
	lua_nscp
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Server" REGULAR_EXPRESSION .*include/check_mk/.*)
SOURCE_GROUP("Socket" REGULAR_EXPRESSION .*include/socket/.*)
