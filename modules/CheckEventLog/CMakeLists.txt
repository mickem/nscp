cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckEventLog)
	
PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

SET(SRCS ${SRCS}
	${TARGET}.cpp
	realtime_data.cpp
	realtime_thread.cpp
	filter_config_object.cpp
	filter.cpp
	${NSCP_DEF_PLUGIN_CPP}
	${NSCP_FILTER_CPP}
	eventlog_wrapper.cpp
	modern_eventlog.cpp
	bookmarks.cpp

  ${NSCP_ERROR_CPP}

	${NSCP_INCLUDEDIR}/compat.cpp

)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		"${TARGET}.h"
		realtime_thread.hpp
		realtime_data.hpp

		eventlog_record.hpp
		eventlog_wrapper.hpp
		simple_registry.hpp
		filter.hpp
		filter_config_object.hpp
		modern_eventlog.hpp
		bookmarks.hpp

		${NSCP_INCLUDEDIR}/compat.hpp

    ${NSCP_ERROR_HPP}

		${NSCP_DEF_PLUGIN_HPP}
		${NSCP_FILTER_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${Boost_DATE_TIME_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${NSCP_FILTER_LIB}
	expression_parser
	version.lib
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
