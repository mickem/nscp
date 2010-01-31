CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
	
PROJECT(Scheduler)

SET(Scheduler_SRCS
	stdafx.cpp
	Scheduler.cpp
	simple_scheduler.cpp
	${NSCP_DEF_PLUGIN_CPP}
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(Scheduler_SRCS ${Scheduler_SRCS}
		stdafx.h
		simple_scheduler.hpp
		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(Scheduler MODULE ${Scheduler_SRCS})

target_link_libraries(Scheduler
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
)
