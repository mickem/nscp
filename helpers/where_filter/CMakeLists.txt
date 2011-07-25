cmake_minimum_required(VERSION 2.6)

SET(TARGET where_filter)

INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})

SET(service_SRCS
	${NSCP_INCLUDEDIR}/parsers/where.cpp
	${NSCP_INCLUDEDIR}/parsers/operators.cpp
	${NSCP_INCLUDEDIR}/parsers/helpers.cpp

	${NSCP_INCLUDEDIR}/parsers/where/expression_ast.cpp
	${NSCP_INCLUDEDIR}/parsers/where/unary_fun.cpp
	${NSCP_INCLUDEDIR}/parsers/where/variable.cpp
	${NSCP_INCLUDEDIR}/parsers/where/binary_op.cpp
	${NSCP_INCLUDEDIR}/parsers/where/unary_op.cpp
	
	${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar.cpp	
	
)

IF(WIN32)
	SET(service_SRCS ${service_SRCS}
		
		${NSCP_INCLUDEDIR}/parsers/operators.hpp
		${NSCP_INCLUDEDIR}/parsers/helpers.hpp
		${NSCP_INCLUDEDIR}/parsers/where.hpp
		
		${NSCP_INCLUDEDIR}/parsers/where/ast_bind.hpp
		${NSCP_INCLUDEDIR}/parsers/where/ast_static_eval.hpp
		${NSCP_INCLUDEDIR}/parsers/where/ast_type_inference.hpp
		${NSCP_INCLUDEDIR}/parsers/where/ast_visitors.hpp
		${NSCP_INCLUDEDIR}/parsers/where/expression_ast.hpp
		${NSCP_INCLUDEDIR}/parsers/where/list_value.hpp
		${NSCP_INCLUDEDIR}/parsers/where/unary_fun.hpp
		${NSCP_INCLUDEDIR}/parsers/where/variable.hpp
		${NSCP_INCLUDEDIR}/parsers/where/binary_op.hpp
		${NSCP_INCLUDEDIR}/parsers/where/unary_op.hpp
		
		${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar.hpp

	)
ENDIF(WIN32)
add_library (${TARGET} ${service_SRCS})

target_link_libraries(${TARGET} protobuf)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "core")
