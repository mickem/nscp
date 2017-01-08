cmake_minimum_required(VERSION 2.6)

IF(WIN32)
	SET(TARGET installer_lib)

	SET(OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
	SET(BUILD_SHARED_LIBS OFF)

	PROJECT(${TARGET})

	SET(SRCS
		${TARGET}.cpp
		${NSCP_INCLUDEDIR}/ServiceCmd.cpp
			
		../settings_manager/settings_manager_impl.cpp
		../settings_manager/settings_handler_impl.cpp

		${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
		${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c

		"${TARGET}.def"
		installer_helper.hpp
		${NSCP_INCLUDEDIR}/ServiceCmd.h
    ${NSCP_ERROR_CPP}
    ${NSCP_ERROR_HPP}
		
		
		# Logger
		${NSCP_INCLUDEDIR}/nsclient/logger/log_level.cpp
		${NSCP_INCLUDEDIR}/nsclient/logger/base_logger_impl.cpp
	)

	
	SET(CompilerFlags
		CMAKE_CXX_FLAGS
		CMAKE_CXX_FLAGS_DEBUG
		CMAKE_CXX_FLAGS_RELEASE
		CMAKE_CXX_FLAGS_MINSIZEREL
		CMAKE_CXX_FLAGS_RELWITHDEBINFO
		CMAKE_C_FLAGS
		CMAKE_C_FLAGS_DEBUG
		CMAKE_C_FLAGS_RELEASE
		CMAKE_C_FLAGS_MINSIZEREL
		CMAKE_C_FLAGS_RELWITHDEBINFO
		)
	FOREACH(CompilerFlag ${CompilerFlags})
		STRING(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
		STRING(REPLACE "/MDd" "/MTd" ${CompilerFlag} "${${CompilerFlag}}")
	ENDFOREACH()

	ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

	ADD_LIBRARY(${TARGET} MODULE ${SRCS})

	TARGET_LINK_LIBRARIES(${TARGET}
		${Boost_FILESYSTEM_LIBRARY_STATIC}
		${Boost_SYSTEM_LIBRARY_STATIC}
		${EXTRA_LIBS}
		Msi.lib
	)
	SET_LIBRARY_OUT_FOLDER_MODULE(${TARGET})

	SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "installer-dll")

	SET(BUILD_SHARED_LIBS ${OLD_BUILD_SHARED_LIBS})

ENDIF(WIN32)
