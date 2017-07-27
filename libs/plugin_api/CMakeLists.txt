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
		${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_filter.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_proxy.hpp
		${NSCP_INCLUDEDIR}/nscapi/macros.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.hpp
	)
ENDIF(WIN32)

SET(EXTRA_LIBS)
IF(APPLE)
	SET(EXTRA_LIBS ${ICONV_LIBRARIES})
ENDIF()

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
NSCP_MAKE_LIBRARY(${TARGET} "${SRCS}")
NSCP_FORCE_INCLUDE(${TARGET} "${BUILD_ROOT_FOLDER}/include/nscapi/dll_defines_protobuf.hpp")

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "libraries")
TARGET_LINK_LIBRARIES(${TARGET}
	${Boost_SYSTEM_LIBRARY}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	nscp_protobuf
	${EXTRA_LIBS})
ADD_DEPENDENCIES(${TARGET} nscp_protobuf)


IF(GTEST_FOUND)
	INCLUDE_DIRECTORIES(${GTEST_INCLUDE_DIR})
	SET(TEST_SRCS
		nscapi_helper_test.cpp
		
		${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
	)
	NSCP_MAKE_EXE_TEST(${TARGET}_test "${TEST_SRCS}")
	NSCP_ADD_TEST(${TARGET}_test ${TARGET}_test)
	TARGET_LINK_LIBRARIES(${TARGET}_test
		${GTEST_GTEST_LIBRARY}
		${GTEST_GTEST_MAIN_LIBRARY}
		${NSCP_DEF_PLUGIN_LIB}
		${EXTRA_LIBS}
		${Boost_DATE_TIME_LIBRARY}
	)
ENDIF(GTEST_FOUND)
