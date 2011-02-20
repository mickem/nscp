CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(TARGET CheckDisk)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	file_finder.cpp
	file_info.cpp
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
		file_finder.hpp
		file_info.hpp
		filter.hpp	

		${NSCP_DEF_PLUGIN_HPP}
		${NSCP_FILTER_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	version.lib
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
