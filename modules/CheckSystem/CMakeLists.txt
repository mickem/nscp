cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckSystem)

PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

IF(JSON_SPIRIT_FOUND)
	INCLUDE_DIRECTORIES(${JSON_SPIRIT_INCLUDE_DIR})
	ADD_DEFINITIONS(-DHAVE_JSON_SPIRIT)
	ADD_DEFINITIONS(-DJSON_SPIRIT_STATIC_LINKED)
	SET(JSON_LIB json_spirit)
ELSE(JSON_SPIRIT_FOUND)
	SET(JSON_LIB)
ENDIF(JSON_SPIRIT_FOUND)
SET(SRCS ${SRCS}
	"${TARGET}.cpp"
	filter.cpp
	counter_filter.cpp
	${NSCP_DEF_PLUGIN_CPP}
	${NSCP_FILTER_CPP}

	pdh_thread.cpp
	check_network.cpp
	check_process.cpp
	check_memory.cpp
	check_pdh.cpp
	filter_config_object.cpp
	realtime_data.cpp

	${NSCP_INCLUDEDIR}/pdh/basic_impl.cpp
	${NSCP_INCLUDEDIR}/pdh/pdh_collectors.cpp
	${NSCP_INCLUDEDIR}/pdh/pdh_counters.cpp
	${NSCP_INCLUDEDIR}/pdh/pdh_enumerations.cpp
	${NSCP_INCLUDEDIR}/pdh/pdh_interface.cpp
	${NSCP_INCLUDEDIR}/pdh/pdh_query.cpp
	${NSCP_INCLUDEDIR}/pdh/pdh_resolver.cpp
	${NSCP_INCLUDEDIR}/pdh/thread_safe_impl.cpp

  ${NSCP_ERROR_CPP}
  
	${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp

	${NSCP_INCLUDEDIR}/wmi/wmi_query.cpp

	${NSCP_INCLUDEDIR}/EnumProcess.cpp
	${NSCP_INCLUDEDIR}/EnumNtSrv.cpp
	${NSCP_INCLUDEDIR}/sysinfo.cpp

	${NSCP_INCLUDEDIR}/compat.cpp

)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

IF(WIN32)
	SET(SRCS ${SRCS}
		"${TARGET}.h"
		filter.hpp
		counter_filter.hpp

		${NSCP_INCLUDEDIR}/EnumProcess.h
		${NSCP_INCLUDEDIR}/EnumNtSrv.h
		${NSCP_INCLUDEDIR}/sysinfo.h

		pdh_thread.hpp
		check_pdh.hpp
		check_memory.hpp
		filter_config_object.hpp
		realtime_data.hpp
		check_network.hpp
		check_process.hpp

		${NSCP_INCLUDEDIR}/pdh/basic_impl.hpp
		${NSCP_INCLUDEDIR}/pdh/pdh_collectors.hpp
		${NSCP_INCLUDEDIR}/pdh/pdh_counters.hpp
		${NSCP_INCLUDEDIR}/pdh/pdh_enumerations.hpp
		${NSCP_INCLUDEDIR}/pdh/pdh_interface.hpp
		${NSCP_INCLUDEDIR}/pdh/pdh_query.hpp
		${NSCP_INCLUDEDIR}/pdh/pdh_resolver.hpp
		${NSCP_INCLUDEDIR}/pdh/thread_safe_impl.hpp

		${NSCP_INCLUDEDIR}/wmi/wmi_query.hpp

    ${NSCP_ERROR_HPP}
    
		${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.hpp

		${NSCP_DEF_PLUGIN_HPP}
		${NSCP_FILTER_HPP}

		${NSCP_INCLUDEDIR}/compat.hpp

	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${NSCP_FILTER_LIB}
	${JSON_LIB}
	expression_parser
	winsys_info
	psapi
	Wbemuuid.lib
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
SOURCE_GROUP("pdh" REGULAR_EXPRESSION .*include/pdh/.*)
