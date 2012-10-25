cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckLogFile)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	filter.cpp
	${NSCP_DEF_PLUGIN_CPP}
	${NSCP_FILTER_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"

		filter.hpp
		filters.hpp

		${NSCP_DEF_PLUGIN_HPP}
		${NSCP_FILTER_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${NSCP_DEF_PLUGIN_LIB}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${NSCP_FILTER_LIB}
	expression_parser
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)