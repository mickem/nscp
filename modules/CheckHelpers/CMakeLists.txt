cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckHelpers)

PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)


ADD_LIBRARY(${TARGET} MODULE ${SRCS})


target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
