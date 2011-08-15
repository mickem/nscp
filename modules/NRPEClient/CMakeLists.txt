cmake_minimum_required(VERSION 2.6)

SET(TARGET NRPEClient)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	${NSCP_INCLUDEDIR}/nrpe/packet.cpp
	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		${NSCP_INCLUDEDIR}/nrpe/packet.hpp
		${NSCP_INCLUDEDIR}/nrpe/client/socket.hpp
		${NSCP_INCLUDEDIR}/swap_bytes.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${OPENSSL_LIBRARIES}
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
