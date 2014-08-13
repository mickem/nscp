cmake_minimum_required(VERSION 2.6)

SET(TARGET where_filter)

INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})

SET(service_SRCS
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
	
	${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar.cpp

	${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
)

IF(WIN32)
	SET(service_SRCS ${service_SRCS}
		
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

		${NSCP_INCLUDEDIR}/parsers/filter/cli_helper.hpp
		${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.hpp
		
	)
ENDIF(WIN32)
add_library (${TARGET} ${service_SRCS})

target_link_libraries(${TARGET} protobuf)
IF(CMAKE_COMPILER_IS_GNUCXX)
	IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
		SET_TARGET_PROPERTIES(${TARGET} PROPERTIES COMPILE_FLAGS -fPIC)
	ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "core")
