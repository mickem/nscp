cmake_minimum_required(VERSION 2.6)

SET(TARGET plugin_api)

INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})
#INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR})

SET(SRCS
	${NSCP_INCLUDEDIR}/arrayBuffer.cpp
	${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
	${NSCP_INCLUDEDIR}/b64/b64.c
	${NSCP_INCLUDEDIR}/utils.cpp
	
	${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_core_helper.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_impl.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_wrapper.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_functions.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_types.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.cpp
	
	${NSCP_INCLUDEDIR}/settings/client/settings_client.cpp
)
	
IF(WIN32)
	SET(SRCS ${SRCS}

		${NSCP_INCLUDEDIR}/strEx.h
		${NSCP_INCLUDEDIR}/config.h
		${NSCP_INCLUDEDIR}/utils.h
		${NSCP_INCLUDEDIR}/NSCAPI.h
		
		${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_core_helper.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_impl.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_wrapper.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_functions.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_types.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.hpp

		${NSCP_INCLUDEDIR}/settings/client/settings_client.hpp
		${NSCP_INCLUDEDIR}/nscapi/settings_proxy.hpp
		${NSCP_INCLUDEDIR}/nscapi/macros.hpp

	)
ENDIF(WIN32)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

add_library(${TARGET} ${SRCS})
target_link_libraries(${TARGET} protobuf)
ADD_DEPENDENCIES(${TARGET} protobuf)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "libraries")
IF(CMAKE_COMPILER_IS_GNUCXX)
	IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
		SET_TARGET_PROPERTIES(${TARGET} PROPERTIES COMPILE_FLAGS -fPIC)
	ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)
