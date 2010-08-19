CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(TARGET CheckEventLog)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	${NSCP_DEF_PLUGIN_CPP}
	
	${NSCP_INCLUDEDIR}/parsers/ast.cpp
	${NSCP_INCLUDEDIR}/parsers/grammar.cpp
	${NSCP_INCLUDEDIR}/parsers/helpers.cpp
	${NSCP_INCLUDEDIR}/parsers/where.cpp
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"

		eventlog_filter.hpp
		eventlog_record.hpp
		simple_registry.hpp

		${NSCP_INCLUDEDIR}/parsers/ast.hpp
		${NSCP_INCLUDEDIR}/parsers/eval.hpp
		${NSCP_INCLUDEDIR}/parsers/grammar.hpp
		${NSCP_INCLUDEDIR}/parsers/helpers.hpp
		${NSCP_INCLUDEDIR}/parsers/where.hpp	

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	version.lib
)
