CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(TARGET CheckTaskSched)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	TaskSched.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

SET(SRCS ${SRCS}
	stdafx.h
	"${TARGET}.h"
	"${TARGET}.def"
	TaskSched.h

	${NSCP_DEF_PLUGIN_HPP}
)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	Mstask.lib
)

INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)