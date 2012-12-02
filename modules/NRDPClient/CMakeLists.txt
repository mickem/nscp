cmake_minimum_required(VERSION 2.6)

SET(TARGET NRDPClient)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	nrdp.cpp
	${TINYXML2_INCLUDE_DIR}/tinyxml2.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
	${NSCP_INCLUDEDIR}/client/command_line_parser.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
INCLUDE_DIRECTORIES(${TINYXML2_INCLUDE_DIR})

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		nrdp.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
		${NSCP_INCLUDEDIR}/client/command_line_parser.hpp
		${TINYXML2_INCLUDE_DIR}/tinyxml2.h
		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)


add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Server" REGULAR_EXPRESSION .*include/nrdp/.*)
