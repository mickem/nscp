cmake_minimum_required(VERSION 2.6)

SET(TARGET nscp)

PROJECT(service)
INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIRS})
LINK_DIRECTORIES(${BOOST_LIB_DIRS})
#INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR})

SET(VERSION_HPP ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp)

ADD_CUSTOM_TARGET(
	"${TARGET}_VERSION"
	COMMAND ${PYTHON_EXECUTABLE}
		"${BUILD_PYTHON_FOLDER}/version.py"
		--file ${VERSION_TXT}
		--generate-hpp ${VERSION_HPP}
		--update build
	)

SET(service_SRCS
	StdAfx.cpp
	NSClient++.cpp
	NSCPlugin.cpp
	core_api.cpp
	settings_logger_impl.cpp

	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
	${NSCP_INCLUDEDIR}/settings/client/settings_client.cpp

	${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
	${NSCP_INCLUDEDIR}/b64/b64.c
	${NSCP_INCLUDEDIR}/arrayBuffer.cpp

	../resources/nsclient.ico
)

IF(WIN32)
	SET(service_SRCS ${service_SRCS}
		${NSCP_INCLUDEDIR}/ServiceCmd.cpp
	)
	SET(service_SRCS ${service_SRCS}
		StdAfx.h
		NSClient++.h
		NSCPlugin.h
		core_api.h
		simple_client.hpp
		settings_client.hpp
		cli_parser.hpp
		
		commands.hpp
		channels.hpp
		logger.hpp
		service_manager.hpp
		settings_logger_impl.hpp

		${NSCP_INCLUDEDIR}/charEx.h
		${NSCP_INCLUDEDIR}/config.h
		${NSCP_INCLUDEDIR}/com_helpers.hpp
		${NSCP_INCLUDEDIR}/dll/dll.hpp
		${NSCP_INCLUDEDIR}/dll/impl_unix.hpp
		${NSCP_INCLUDEDIR}/dll/impl_w32.hpp
#		${NSCP_INCLUDEDIR}/nsclient_session.hpp

		${NSCP_INCLUDEDIR}/service/system_service.hpp
		${NSCP_INCLUDEDIR}/service/win32_service.hpp
		${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp

	)
	SET(service_SRCS ${service_SRCS}
		${CMAKE_CURRENT_SOURCE_DIR}/NSClient++.manifest
	)
	
	ADD_DEFINITIONS(-D_WIN32_DCOM)

	
#	SET(
#		CMAKE_LINKER_FLAGS 
#		/MANIFEST 
#		/MANIFESTFILE:"${CMAKE_CURRENT_SOURCE_DIR}/NSClient++.manifest"
#	)
#	SET(
#		CMAKE_MODULE_LINKER_FLAG 
#		/MANIFEST 
#		/MANIFESTFILE:"${CMAKE_CURRENT_SOURCE_DIR}/NSClient++.manifest"
#	)

ENDIF(WIN32)
IF(BREAKPAD_FOUND)
	SET(service_SRCS ${service_SRCS} ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.cpp)
	IF(WIN32)
		SET(service_SRCS ${service_SRCS} ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.hpp)
	ENDIF(WIN32)
	SET(EXTRA_LIBS ${BREAKPAD_COMMON_LIBRARY} ${BREAKPAD_EXCEPTION_HANDLER_LIBRARY} ${BREAKPAD_CRASH_GENERATION_CLIENT_LIBRARY})
	ADD_DEFINITIONS(-DUSE_BREAKPAD)
ENDIF(BREAKPAD_FOUND)

add_executable (${TARGET} ${service_SRCS})
add_dependencies(${TARGET} ${TARGET}_VERSION)

MESSAGE(STATUS "Extra: ${EXTRA_LIBS}")

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
	settings_manager
)
SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "core")
SET_TARGET_PROPERTIES(${TARGET}_VERSION PROPERTIES FOLDER "core")
#IF(WIN32)
#	SET_TARGET_PROPERTIES(${TARGET} PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS")
#ENDIF(WIN32)

SOURCE_GROUP("Common Files" REGULAR_EXPRESSION .*include/.*)
SOURCE_GROUP("Parser" REGULAR_EXPRESSION .*include/parser/.*)
SOURCE_GROUP("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
SOURCE_GROUP("Settings" REGULAR_EXPRESSION .*include/settings/.*)

#SOURCE_GROUP("NSCP API/Sources" FILES ${NSCP_DEF_PLUGIN_CPP})
#SOURCE_GROUP("Settings" REGULAR_EXPRESSION .*settings.*)

INSTALL(TARGETS ${TARGET} RUNTIME DESTINATION .)

# First, build the full name of the EXE.
INSTALL(CODE "SET(PDB_FULL_PATH ${BUILD_TARGET_EXE_PATH}/${TARGET}.pdb)")
INSTALL(CODE "FILE(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX} TYPE EXECUTABLE FILES \${PDB_FULL_PATH})")
#INSTALL(CODE "MESSAGE(STATUS ----->\${CMAKE_INSTALL_PREFIX})")
#INSTALL(CODE "MESSAGE(STATUS ----->\${CMAKE_INSTALL_CONFIG_NAME})")
#INSTALL(CODE "MESSAGE(STATUS ----->${CMAKE_CURRENT_BINARY_DIR})")
#INSTALL(CODE "MESSAGE(STATUS ----->\${PDB_FULL_PATH})")
# Then, replace .exe with .pdb.
#INSTALL(CODE "STRING(REPLACE .exe .pdb PDB_FULL_PATH \${PDB_FULL_PATH})")
# Finally, get it installed.
