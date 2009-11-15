CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
	
PROJECT(CheckHelpers)

SET(CheckHelpers_SRCS
	stdafx.cpp
	CheckHelpers.cpp
	${NSCP_INCLUDE_PATH}/NSCHelper.cpp
	${NSCP_INCLUDE_PATH}/arrayBuffer.cpp
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(CheckHelpers_SRCS ${CheckHelpers_SRCS}
		stdafx.h
		${NSCP_INCLUDE_PATH}/strEx.h
		${NSCP_INCLUDE_PATH}/config.h
		${NSCP_INCLUDE_PATH}/utils.h
		${NSCP_INCLUDE_PATH}/NSCAPI.h
		${NSCP_INCLUDE_PATH}/NSCHelper.h
		${NSCP_INCLUDE_PATH}/nsc_module_wrapper.hpp
		CheckHelpers.def
	)
ENDIF(WIN32)

add_library(CheckHelpers MODULE ${CheckHelpers_SRCS})

target_link_libraries(CheckHelpers
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${EXTRA_LIBS}
)


#INSTALL(FILES ${PROJECT_SOURCE_DIR}/cmake/FindWt.cmake DESTINATION
#    ${CMAKE_INSTALL_PREFIX}/${WT_CMAKE_FINDER_INSTALL_DIR} )

