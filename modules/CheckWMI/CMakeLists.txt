cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckWMI)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	
	${NSCP_DEF_PLUGIN_CPP}
	WMIQuery.cpp
)


ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

SET(SRCS ${SRCS}
	stdafx.h
	"${TARGET}.h"
	"${TARGET}.def"
	WMIQuery.h

	${NSCP_DEF_PLUGIN_HPP}
)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${NSCP_DEF_PLUGIN_LIB}
	Wbemuuid.lib

)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)