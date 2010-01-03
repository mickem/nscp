CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
	
PROJECT(Scheduler)

SET(Scheduler_SRCS
	stdafx.cpp
	Scheduler.cpp
	simple_scheduler.cpp
	${NSCP_INCLUDE_PATH}/NSCHelper.cpp
	${NSCP_INCLUDE_PATH}/arrayBuffer.cpp
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(Scheduler_SRCS ${Scheduler_SRCS}
		stdafx.h
		simple_scheduler.hpp
		${NSCP_INCLUDE_PATH}/strEx.h
		${NSCP_INCLUDE_PATH}/config.h
		${NSCP_INCLUDE_PATH}/utils.h
		${NSCP_INCLUDE_PATH}/NSCAPI.h
		${NSCP_INCLUDE_PATH}/NSCHelper.h
		${NSCP_INCLUDE_PATH}/nsc_module_wrapper.hpp
		Scheduler.def
	)
ENDIF(WIN32)

add_library(Scheduler MODULE ${Scheduler_SRCS})

target_link_libraries(Scheduler
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${EXTRA_LIBS}
)
