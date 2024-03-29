cmake_minimum_required(VERSION 3.5)

SET(TARGET NRDPClient)
	
PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

SET(SRCS ${SRCS}
	"${TARGET}.cpp"
	nrdp.cpp
	${TINYXML2_INCLUDE_DIR}/tinyxml2.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp

	${NSCP_DEF_PLUGIN_CPP}
	${NSCP_CLIENT_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
INCLUDE_DIRECTORIES(${TINYXML2_INCLUDE_DIR})
IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DUSE_SSL)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
	IF(WIN32)
		SET(EXTRA_LIBS ${EXTRA_LIBS} CRYPT32)
	ENDIF(WIN32)
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
ENDIF(OPENSSL_FOUND)


IF(WIN32)
	SET(SRCS ${SRCS}
		"${TARGET}.h"
		nrdp.hpp
		nrdp_client.hpp
		nrdp_handler.hpp

		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
		${TINYXML2_INCLUDE_DIR}/tinyxml2.h
		${NSCP_DEF_PLUGIN_HPP}
		${NSCP_CLIENT_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Server" REGULAR_EXPRESSION .*include/nrdp/.*)
