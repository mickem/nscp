cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckDisk)

PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

SET(SRCS ${SRCS}
	"${TARGET}.cpp"

	check_drive.cpp

	file_finder.cpp
	filter.cpp
	${NSCP_DEF_PLUGIN_CPP}
	${NSCP_FILTER_CPP}
  ${NSCP_ERROR_CPP}

	${NSCP_INCLUDEDIR}/compat.cpp
)


ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		"${TARGET}.h"

		check_drive.hpp

		file_finder.hpp
		filter.hpp	

		${NSCP_DEF_PLUGIN_HPP}
		${NSCP_FILTER_HPP}
    ${NSCP_ERROR_HPP}

		${NSCP_INCLUDEDIR}/compat.hpp

	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${Boost_DATE_TIME_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${NSCP_FILTER_LIB}
	expression_parser
	version.lib
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
