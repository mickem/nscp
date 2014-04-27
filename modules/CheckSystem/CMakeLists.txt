cmake_minimum_required(VERSION 2.6)

SET(TARGET CheckSystem)
	
PROJECT(${TARGET})

SET(SRCS
	stdafx.cpp
	"${TARGET}.cpp"
	"PDHCollector.cpp"
	${NSCP_DEF_PLUGIN_CPP}
	check_pdh.cpp
	
	${NSCP_INCLUDEDIR}/pdh/basic_impl.cpp
	${NSCP_INCLUDEDIR}/pdh/core.cpp

	${NSCP_INCLUDEDIR}/EnumProcess.cpp
	${NSCP_INCLUDEDIR}/EnumNtSrv.cpp
	${NSCP_INCLUDEDIR}/sysinfo.cpp
	
)

ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

SET(SRCS ${SRCS}
	stdafx.h
	"${TARGET}.h"
	"${TARGET}.def"
	"PDHCollector.h"
	check_pdh.hpp

	${NSCP_INCLUDEDIR}/EnumProcess.h
	${NSCP_INCLUDEDIR}/EnumNtSrv.h
	${NSCP_INCLUDEDIR}/sysinfo.h

	${NSCP_INCLUDEDIR}/pdh/basic_impl.hpp
	${NSCP_INCLUDEDIR}/pdh/collectors.hpp
	${NSCP_INCLUDEDIR}/pdh/core.hpp
	${NSCP_INCLUDEDIR}/pdh/counters.hpp
	${NSCP_INCLUDEDIR}/pdh/enumerations.hpp
	${NSCP_INCLUDEDIR}/pdh/query.hpp
	${NSCP_INCLUDEDIR}/pdh/resolver.hpp
	${NSCP_INCLUDEDIR}/pdh/thread_safe_impl.hpp
	
	${NSCP_DEF_PLUGIN_HPP}
)

add_library(${TARGET} MODULE ${SRCS})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
