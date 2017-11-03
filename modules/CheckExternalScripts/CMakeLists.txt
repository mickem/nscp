cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckExternalScripts)
	
PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

IF(JSON_SPIRIT_FOUND)
	INCLUDE_DIRECTORIES(${JSON_SPIRIT_INCLUDE_DIR})
	ADD_DEFINITIONS(-DHAVE_JSON_SPIRIT)
	ADD_DEFINITIONS(-DJSON_SPIRIT_STATIC_LINKED)
	SET(JSON_LIB json_spirit)
ELSE(JSON_SPIRIT_FOUND)
	SET(JSON_LIB)
ENDIF(JSON_SPIRIT_FOUND)
SET(SRCS ${SRCS}
	"${TARGET}.cpp"
	extscr_cli.cpp
	script_provider.cpp

	${NSCP_DEF_PLUGIN_CPP}
  ${NSCP_ERROR_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		"${TARGET}.h"
		commands.hpp
		alias.hpp
		extscr_cli.h
		script_provider.hpp
		script_interface.hpp

		${NSCP_INCLUDEDIR}/process/execute_process.hpp

		${NSCP_INCLUDEDIR}/win_sysinfo/win_sysinfo.cpp
		${NSCP_INCLUDEDIR}/win_sysinfo/win_sysinfo.hpp

		${NSCP_DEF_PLUGIN_HPP}
		${NSCP_ERROR_HPP}

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
	${JSON_LIB}
	${EXTRA_LIBS}
)

INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)