cmake_minimum_required(VERSION 2.6)

SET(TARGET CollectdClient)
	
PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

SET(SRCS ${SRCS}
	"${TARGET}.cpp"
	${NSCP_INCLUDEDIR}/collectd/collectd_packet.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp

	${NSCP_DEF_PLUGIN_CPP}
	${NSCP_CLIENT_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		"${TARGET}.h"
		collectd_client.hpp
		collectd_handler.hpp

		${NSCP_INCLUDEDIR}/collectd/collectd_packet.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
		${NSCP_INCLUDEDIR}/socket/client.hpp

		${NSCP_DEF_PLUGIN_HPP}
		${NSCP_CLIENT_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
	expression_parser
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("Client" REGULAR_EXPRESSION .*include/collectd/.*)
SOURCE_GROUP("Socket" REGULAR_EXPRESSION .*include/socket/.*)
