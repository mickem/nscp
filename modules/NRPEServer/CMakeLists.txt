CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(TARGET NRPEServer)
	
PROJECT(${TARGET})

# Workaround for bug 0003353 in cmake (aparently not fixed)
SET_SOURCE_FILES_PROPERTIES(${PB_PLUGIN} PROPERTIES GENERATED TRUE)

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	nrpe_server.cpp
	nrpe_connection.cpp
	nrpe_handler.cpp
	${NSCP_INCLUDE_PATH}/nrpe/nrpepacket.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		stdafx.h
		"${TARGET}.h"
		"${TARGET}.def"
		nrpe_server.hpp
		nrpe_connection.hpp
		nrpe_handler.hpp
		nrpe_parser.hpp
		${NSCP_INCLUDE_PATH}/nrpe/nrpepacket.hpp
		${NSCP_INCLUDE_PATH}/socket_helpers.hpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${OPENSSL_LIBRARIES}
)


#INSTALL(FILES ${PROJECT_SOURCE_DIR}/cmake/FindWt.cmake DESTINATION
#    ${CMAKE_INSTALL_PREFIX}/${WT_CMAKE_FINDER_INSTALL_DIR} )

