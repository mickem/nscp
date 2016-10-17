cmake_minimum_required(VERSION 2.6)

SET(TARGET CommandClient)
	
PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

SET(SRCS ${SRCS}
	"${TARGET}.cpp"
	${NSCP_INCLUDEDIR}/client/simple_client.cpp

	${NSCP_INCLUDEDIR}/metrics/metrics_store_map.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		"${TARGET}.h"
		${NSCP_INCLUDEDIR}/client/simple_client.hpp

		${NSCP_INCLUDEDIR}/metrics/metrics_store_map.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${Boost_DATE_TIME_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
