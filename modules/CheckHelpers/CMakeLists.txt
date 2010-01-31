CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
	
PROJECT(CheckHelpers)

SET(CheckHelpers_SRCS
	stdafx.cpp
	CheckHelpers.cpp
	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(CheckHelpers_SRCS ${CheckHelpers_SRCS}
		stdafx.h
		CheckHelpers.def
		${NSCP_DEF_PLUGIN_HPP}

	)
ENDIF(WIN32)

add_library(CheckHelpers MODULE ${CheckHelpers_SRCS})

target_link_libraries(CheckHelpers
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
)


#INSTALL(FILES ${PROJECT_SOURCE_DIR}/cmake/FindWt.cmake DESTINATION
#    ${CMAKE_INSTALL_PREFIX}/${WT_CMAKE_FINDER_INSTALL_DIR} )

