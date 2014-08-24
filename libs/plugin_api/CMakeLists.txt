cmake_minimum_required(VERSION 2.6)

SET(TARGET plugin_api)

INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})

SET(SRCS
	${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_core_helper.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_functions.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_types.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_filter.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_proxy.cpp
	${NSCP_INCLUDEDIR}/nscapi/command_client.cpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.cpp

	${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.cpp
)
	
IF(WIN32)
	SET(SRCS ${SRCS}
		${NSCP_INCLUDEDIR}/config.h
		${NSCP_INCLUDEDIR}/NSCAPI.h
		
		${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_core_helper.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_functions.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_types.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_filter.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_proxy.hpp
		${NSCP_INCLUDEDIR}/nscapi/macros.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.hpp

		${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.hpp
	)
ENDIF(WIN32)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(USE_STATIC_RUNTIME)
	ADD_LIBRARY(${TARGET} STATIC ${SRCS})
	IF(CMAKE_COMPILER_IS_GNUCXX AND "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
		SET_TARGET_PROPERTIES(${TARGET} PROPERTIES COMPILE_FLAGS -fPIC)
	ENDIF(CMAKE_COMPILER_IS_GNUCXX AND "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
ELSE(USE_STATIC_RUNTIME)
	ADD_LIBRARY(${TARGET} SHARED ${SRCS})
	SET_LIBRARY_OUT_FOLDER(${TARGET})
ENDIF(USE_STATIC_RUNTIME)
IF(WIN32)
	SET_TARGET_PROPERTIES(${TARGET} PROPERTIES COMPILE_FLAGS "/FI\"${BUILD_ROOT_FOLDER}\\include\\nscapi\\dll_defines_protobuf.hpp\"")
ENDIF(WIN32) 

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "libraries")
TARGET_LINK_LIBRARIES(${TARGET}
	${Boost_SYSTEM_LIBRARY}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	protobuf)
ADD_DEPENDENCIES(${TARGET} protobuf)
