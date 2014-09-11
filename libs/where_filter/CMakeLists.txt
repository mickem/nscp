cmake_minimum_required(VERSION 2.6)

SET(TARGET where_filter)

INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})
ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

SET(SRCS
	${NSCP_INCLUDEDIR}/parsers/where.cpp
	${NSCP_INCLUDEDIR}/parsers/operators.cpp
	${NSCP_INCLUDEDIR}/parsers/helpers.cpp

	${NSCP_INCLUDEDIR}/parsers/where/binary_op.cpp
	${NSCP_INCLUDEDIR}/parsers/where/helpers.cpp
	${NSCP_INCLUDEDIR}/parsers/where/list_node.cpp
	${NSCP_INCLUDEDIR}/parsers/where/node.cpp
	${NSCP_INCLUDEDIR}/parsers/where/unary_fun.cpp
	${NSCP_INCLUDEDIR}/parsers/where/unary_op.cpp
	${NSCP_INCLUDEDIR}/parsers/where/value_node.cpp
	${NSCP_INCLUDEDIR}/parsers/where/variable.cpp
	${NSCP_INCLUDEDIR}/parsers/where/engine.cpp

	${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar.cpp
)

IF(WIN32)
	SET(SRCS ${SRCS}
		${NSCP_INCLUDEDIR}/parsers/operators.hpp
		${NSCP_INCLUDEDIR}/parsers/helpers.hpp
		${NSCP_INCLUDEDIR}/parsers/where.hpp
		
		${NSCP_INCLUDEDIR}/parsers/where/binary_op.hpp
		${NSCP_INCLUDEDIR}/parsers/where/helpers.hpp
		${NSCP_INCLUDEDIR}/parsers/where/list_node.hpp
		${NSCP_INCLUDEDIR}/parsers/where/node.hpp
		${NSCP_INCLUDEDIR}/parsers/where/unary_fun.hpp
		${NSCP_INCLUDEDIR}/parsers/where/unary_op.hpp
		${NSCP_INCLUDEDIR}/parsers/where/value_node.hpp
		${NSCP_INCLUDEDIR}/parsers/where/variable.hpp

		${NSCP_INCLUDEDIR}/parsers/where/filter_handler_impl.hpp
		
		${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar.hpp

		${NSCP_INCLUDEDIR}/parsers/where/engine.hpp
		${NSCP_INCLUDEDIR}/parsers/where/engine_impl.hpp
	)
ENDIF(WIN32)

IF(USE_STATIC_RUNTIME)
	ADD_LIBRARY(${TARGET} STATIC ${SRCS})
	IF(CMAKE_COMPILER_IS_GNUCXX AND "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
		SET_TARGET_PROPERTIES(${TARGET} PROPERTIES COMPILE_FLAGS -fPIC)
	ENDIF(CMAKE_COMPILER_IS_GNUCXX AND "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
ELSE(USE_STATIC_RUNTIME)
	ADD_LIBRARY(${TARGET} SHARED ${SRCS})
	SET_LIBRARY_OUT_FOLDER(${TARGET})
ENDIF(USE_STATIC_RUNTIME)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "libraries")
TARGET_LINK_LIBRARIES(${TARGET}
	${Boost_SYSTEM_LIBRARY}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${Boost_REGEX_LIBRARY}
)
IF(NOT USE_STATIC_RUNTIME)
	INSTALL(TARGETS ${TARGET} DESTINATION .)
ENDIF(NOT USE_STATIC_RUNTIME)
