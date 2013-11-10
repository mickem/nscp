cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckExternalScripts)
	
PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

SET(SRCS ${SRCS}
	stdafx.cpp
	"${TARGET}.cpp"
	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		commands.hpp
		alias.hpp
		${NSCP_INCLUDEDIR}/process/execute_process.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
	SET(SRCS ${SRCS}
		${NSCP_INCLUDEDIR}/process/execute_process_w32.cpp
	)
ELSE(WIN32)
	SET(EXTRA_LIBS ${EXTRA_LIBS} pthread)
	SET(SRCS ${SRCS}
		${NSCP_INCLUDEDIR}/process/execute_process_unix.cpp
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
)

INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)