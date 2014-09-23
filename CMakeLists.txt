cmake_minimum_required(VERSION 2.6)

PROJECT(NSCP CXX C)

ENABLE_TESTING()
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0011 NEW)
ENDIF(COMMAND CMAKE_POLICY)

#############################################################################
#
# Setup cmake enviornment and include custom config overrides
#
#############################################################################
IF(NSCP_CMAKE_CONFIG)
	MESSAGE(STATUS "Reading custom variables from: ${NSCP_CMAKE_CONFIG}")
	INCLUDE(${NSCP_CMAKE_CONFIG})
ELSEIF(EXISTS ${CMAKE_SOURCE_DIR}/build.cmake)
	MESSAGE(STATUS "Reading custom variables from: ${CMAKE_SOURCE_DIR}/build.cmake")
	INCLUDE(${CMAKE_SOURCE_DIR}/build.cmake)
ELSEIF(EXISTS ${PROJECT_BINARY_DIR}/build.cmake)
	MESSAGE(STATUS "Reading custom variables from: ${PROJECT_BINARY_DIR}/build.cmake")
	INCLUDE(${PROJECT_BINARY_DIR}/build.cmake)
ELSEIF(EXISTS ${PROJECT_BINARY_DIR}/../build.cmake)
	MESSAGE(STATUS "Reading custom variables from: ${PROJECT_BINARY_DIR}/../build.cmake")
	INCLUDE(${PROJECT_BINARY_DIR}/../build.cmake)
ELSE(NSCP_CMAKE_CONFIG)
	MESSAGE(STATUS "NO build configuration found")
ENDIF(NSCP_CMAKE_CONFIG)
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build/cmake;${CMAKE_MODULE_PATH}")
SET(MODULE_SUBFOLDER "modules")

SET(BUILD_PYTHON_FOLDER "${CMAKE_SOURCE_DIR}/build/python")
SET(BUILD_CMAKE_FOLDER "${CMAKE_SOURCE_DIR}/build/cmake")
SET(BUILD_ROOT_FOLDER "${CMAKE_SOURCE_DIR}")
SET(BUILD_TARGET_EXE_PATH "${PROJECT_BINARY_DIR}")
SET(BUILD_TARGET_ROOT_PATH "${PROJECT_BINARY_DIR}")
SET(BUILD_TARGET_LIB_PATH "${BUILD_TARGET_EXE_PATH}/${MODULE_SUBFOLDER}")
SET(BUILD_TARGET_INSTLIB_PATH "${BUILD_TARGET_EXE_PATH}/installer-dlls")
SET(NSCP_PROJECT_BINARY_DIR ${PROJECT_BINARY_DIR})
IF(NOT CMAKE_INSTALL_PREFIX)
	SET(CMAKE_INSTALL_PREFIX "/usr/local")
ENDIF()
SET(MODULE_TARGET_FOLDER lib/nsclient/modules)
SET(LIB_TARGET_FOLDER lib)

IF(NOT DEFINED USE_STATIC_RUNTIME)
	SET(USE_STATIC_RUNTIME FALSE)
ENDIF(NOT DEFINED USE_STATIC_RUNTIME)
IF(USE_STATIC_RUNTIME)
	MESSAGE(STATUS "Using static runtime")
	SET(BUILD_SHARED_LIBS			OFF)
	SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DBOOST_ALL_NO_LIB)
	add_definitions(-Dnscp_protobuf_NOLIB -Dplugin_api_NOLIB -Dwhere_filter_NOLIB -Djson_protobuf_NOLIB -Dlua_protobuf_NOLIB)
IF(WIN32)
	SET(Boost_USE_STATIC_LIBS		ON)
	SET(Boost_USE_STATIC_RUNTIME	ON)
	SET(BOOST_USE_MULTITHREADED		ON)
ENDIF(WIN32)
	SET(CRYPTOPP_RUNTIME_TYPE		"STATIC")
	SET(JSON_SPIRIT_LIBRARY_TYPE	"STATIC")
	SET(BUILD_STATIC_LIBS			ON)
ELSE(USE_STATIC_RUNTIME)
	SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DBOOST_ALL_NO_LIB -DBOOST_PROGRAM_OPTIONS_DYN_LINK)
	MESSAGE(STATUS "Using dynamic runtime")
	SET(BUILD_SHARED_LIBS			ON)
IF(WIN32)
	SET(Boost_USE_STATIC_LIBS		OFF)
	SET(Boost_USE_STATIC_RUNTIME	OFF)
	SET(BOOST_USE_MULTITHREADED		ON)
ENDIF(WIN32)
	SET(CRYPTOPP_RUNTIME_TYPE		"SHARED")
	SET(JSON_SPIRIT_LIBRARY_TYPE	"SHARED")
	SET(BUILD_STATIC_LIBS			OFF)
ENDIF(USE_STATIC_RUNTIME)

INCLUDE(${BUILD_CMAKE_FOLDER}/SetMultiThread.cmake)
INCLUDE(${BUILD_CMAKE_FOLDER}/GoogleProtoBuf.cmake)
INCLUDE(${BUILD_CMAKE_FOLDER}/functions.cmake)

#############################################################################
#
# Find all dependencies and report anything missing.
#
#############################################################################
INCLUDE(${BUILD_CMAKE_FOLDER}/dependencies.cmake)

#############################################################################
#
# Validate all required dependencies and terminate on failures.
#
#############################################################################
MESSAGE(STATUS "Validating required dependencies:")
SET(HAVE_REQUIRED_DEPENDENCIES TRUE)
IF(NOT PYTHONINTERP_FOUND)
	SET(HAVE_REQUIRED_DEPENDENCIES FALSE)
	MESSAGE(WARNING " - python(exe) not found! (Required by the build process)")
ENDIF(NOT PYTHONINTERP_FOUND)
IF(NOT PROTOBUF_FOUND)
	SET(HAVE_REQUIRED_DEPENDENCIES FALSE)
	MESSAGE(WARNING " - protocol buffers not found! XXX=${PROTOBUF_LIBRARYDIR}! (Required by nsclient++)")
ENDIF(NOT PROTOBUF_FOUND)
IF(NOT Boost_FOUND)
	SET(HAVE_REQUIRED_DEPENDENCIES FALSE)
	MESSAGE(WARNING " - boost libraries was not found: ${Boost_ERROR_REASON} (Required by nsclient++)")
ENDIF(NOT Boost_FOUND)
IF(NOT WIN32)
#	IF(NOT CMAKE_USE_PTHREADS_INIT)
#		SET(HAVE_REQUIRED_DEPENDENCIES FALSE)
#		MESSAGE(WARNING " - Could not find POSIX threads")
#	ENDIF(NOT CMAKE_USE_PTHREADS_INIT)
ENDIF(NOT WIN32)

IF(NOT HAVE_REQUIRED_DEPENDENCIES)
	MESSAGE(FATAL_ERROR " - missing required dependencies found!")
ELSE(NOT HAVE_REQUIRED_DEPENDENCIES)
	MESSAGE(STATUS " - All required dependecies found! (some modules and features migh stil be missing though)")
ENDIF(NOT HAVE_REQUIRED_DEPENDENCIES)

IF(USE_STATIC_RUNTIME)
ELSE(USE_STATIC_RUNTIME)
	SET (LIBS DATE_TIME FILESYSTEM PROGRAM_OPTIONS PYTHON "REGEX" SYSTEM THREAD)
	FOREACH (LIB ${LIBS})
		STRING(REPLACE ".lib" ".dll" Boost_${LIB}_LIBRARY_RELEASE_DLL ${Boost_${LIB}_LIBRARY_RELEASE})
		COPY_FILE(${Boost_${LIB}_LIBRARY_RELEASE_DLL} ${PROJECT_BINARY_DIR})
		if(WIN32)
			INSTALL(FILES ${Boost_${LIB}_LIBRARY_RELEASE_DLL} DESTINATION .)
		endif(WIN32)
		STRING(REPLACE ".lib" ".dll" Boost_${LIB}_LIBRARY_DEBUG_DLL ${Boost_${LIB}_LIBRARY_DEBUG})
		COPY_FILE(${Boost_${LIB}_LIBRARY_DEBUG_DLL} ${PROJECT_BINARY_DIR})
	ENDFOREACH()
	SET (LIBS LIB SSL)
	FOREACH (LIB ${LIBS})
		STRING(REPLACE ".lib" ".dll" ${LIB}_EAY_RELEASE_DLL "${${LIB}_EAY_RELEASE}")
		COPY_FILE("${${LIB}_EAY_RELEASE_DLL}" ${PROJECT_BINARY_DIR})
		if(WIN32)
			INSTALL(FILES "${${LIB}_EAY_RELEASE_DLL}" DESTINATION .)
		endif(WIN32)
	ENDFOREACH()
ENDIF(USE_STATIC_RUNTIME)

IF(PYTHON_DIST)
SET (LIBS _socket.pyd python27.dll python27.zip unicodedata.pyd)
FOREACH (LIB ${LIBS})
	COPY_FILE(${PYTHON_DIST}/${LIB} ${PROJECT_BINARY_DIR})
	INSTALL(FILES ${PYTHON_DIST}/${LIB} DESTINATION .)
ENDFOREACH()
ENDIF(PYTHON_DIST)

#############################################################################
#
# Bump version number (if requested) and set up variables with current version
#
#############################################################################
SET(VERSION_TXT ${CMAKE_CURRENT_SOURCE_DIR}/version.txt)
IF("${INCREASE_BUILD}" STREQUAL "1")
EXECUTE_PROCESS(
	COMMAND ${PYTHON_EXECUTABLE}
		"${BUILD_PYTHON_FOLDER}/version.py" 
		--file ${VERSION_TXT} 
		--display 
		--update build
		--generate-hpp ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp
	OUTPUT_VARIABLE TMP_VERSION_OUT
	)
ENDIF("${INCREASE_BUILD}" STREQUAL "1")

EXECUTE_PROCESS(
	COMMAND ${PYTHON_EXECUTABLE}
		"${BUILD_PYTHON_FOLDER}/version.py" 
		--file ${VERSION_TXT} 
		--display 
		--generate-hpp ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp
	OUTPUT_VARIABLE TMP_VERSION_OUT
	)
	
IF(TMP_VERSION_OUT STREQUAL "")
	MESSAGE(FATA_ERROR, "Failed to generate version from: ${PYTHON_EXECUTABLE} ${BUILD_PYTHON_FOLDER}/version.py --file ${VERSION_TXT} --display ")
ENDIF(TMP_VERSION_OUT STREQUAL "")
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1" VERSION_SERIES ${TMP_VERSION_OUT})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\2" VERSION_MAJOR ${TMP_VERSION_OUT})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\3" VERSION_MINOR ${TMP_VERSION_OUT})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\4" VERSION_BUILD ${TMP_VERSION_OUT})
MESSAGE(STATUS "Version: ${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}")
SET(NSCP_LIB_VERSION "${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}")
SET(NSCP_FULL_VERSION "${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}")
IF(WIN32)
	SET(VERSION_ARCH Win32)
	IF(CMAKE_CL_64)
		SET(VERSION_ARCH x64)
	ENDIF(CMAKE_CL_64)
ELSE(WIN32)
	SET(VERSION_ARCH Unknown)
ENDIF(WIN32)

#############################################################################
#
# Setup c/C++ build enviornment and flags
#
#############################################################################
SET(NSCP_INCLUDEDIR ${CMAKE_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR} ${CMAKE_CURRENT_BINARY_DIR}/libs)
SET(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH};${OPENSSL_LIBRARY_DIR};${NSCP_LIBRARYDIR};${BOOST_LIBRARYDIR}")

SET_MULTITHREAD()

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-long-long -pedantic -Wno-unused-parameter")
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  message(STATUS "Added parallel build arguments to CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
endif()

IF(NOT SHARED_LIBS)
  IF(WIN32)
    OPTION(SHARED_LIBS "Compile shared libraries" OFF)
  ELSE(WIN32)
    OPTION(SHARED_LIBS "Compile shared libraries" ON)
  ENDIF(WIN32)
ENDIF(NOT SHARED_LIBS)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
INCLUDE_DIRECTORIES("${PROTOBUF_INCLUDE_DIR}")
INCLUDE_DIRECTORIES("${PROJECT_BINARY_DIR}")
add_definitions(-DUNICODE -D_UNICODE)

IF(WIN32)
  SET(RUNDIR "c:/nscp" CACHE PATH "Not really used in WIN32")
ELSE(WIN32)
  SET(USERLIB_ROOT /usr CACHE PATH "Other installation prefix for dependent libraries")
  SET(LIB_INSTALL_DIR "lib" CACHE STRING "Default path for libraries within ${CMAKE_INSTALL_PREFIX}")
  SET(RUNDIR "/usr/wt/run" CACHE PATH "Default path for wt session management (only used by FCGI connector; not relative to CMAKE_INSTALL_PREFIX)")
  SET(CONFIGURATION /etc/nscp/boot.ini CACHE PATH "Path to the boot configuration file")
  SET(MAIN_CONFIGURATION /etc/nscp/nscp.xml CACHE PATH "Path for the client configurtion file")
  SET(DEPLOYROOT /usr/bin CACHE PATH "Path to deploy examples into)")
ENDIF(WIN32)

IF(CMAKE_COMPILER_IS_GNUCXX)
	IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
		SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC"  )
		SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC"  )
	ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DUNICODE -D_UNICODE)
IF (WIN32)
	SET(NSCP_GLOBAL_DEFINES "${NSCP_GLOBAL_DEFINES} -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501 -DWINVER=0x0501")
ENDIF (WIN32)

IF(WIN32)
  SET(EXTRA_LIBS ${EXTRA_LIBS} Userenv ole32)
ENDIF(WIN32)

IF(CMAKE_HOST_UNIX)
	SET(UNIX_FLAVOUR LINUX CACHE STRING "Choose your unix flavour UNIX, AIX, HPUX" FORCE)
	IF(UNIX_FLAVOUR STREQUAL LINUX)
		SET(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DLINUX)
	ENDIF(UNIX_FLAVOUR STREQUAL LINUX)
	
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_DL_LIBS})
ENDIF()

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

LINK_DIRECTORIES(${CMAKE_LIBRARY_PATH} ${OPENSSL_LIBRARY_DIR} ${NSCP_LIBRARYDIR} ${BOOST_LIBRARYDIR})

# Compile time constants & make sure our build finds it

SET(CONFIG_DEFAULT_SERVICE_NAME "NSCP" CACHE STRING "The name of the service")
SET(CONFIG_DEFAULT_SERVICE_DESC "Monitoring Agent (Provides performance data for monitoring solutions like Nagios)" CACHE STRING "The description used for the service")
SET(CONFIG_DEFAULT_SERVICE_DEPS "" CACHE STRING "Any service dependencies to use for the default service")

SET(CONFIG_CRASH_SUBMIT_URL "http://crash.nsclient.org/submit" CACHE STRING "Where to submit crash dump files")
SET(CONFIG_CRASH_ARCHIVE_FOLDER "\${shared-path}crash-dumps" CACHE STRING "Where to archive chras dump files on the local filesystem")

IF(WIN32)
	SET(CONFIG_DEFAULT_CONF_LOCATION "old" CACHE STRING "Default configuration store")
ELSE(WIN32)
	SET(CONFIG_DEFAULT_CONF_LOCATION "ini" CACHE STRING "Default configuration store")
ENDIF(WIN32)
SET(CONFIG_BOOT_CONF_LOCATION			"\${exe-path}/boot.ini" CACHE STRING "Location of boot ini (the file which specified where to load the config)")
SET(CONFIG_DEFAULT_CONF_INI_BASE		"ini://\${shared-path}/" CACHE STRING "Base location for ini files")
SET(CONFIG_DEFAULT_CONF_REG_LOCATION	"registry://HKEY_LOCAL_MACHINE/software/NSClient++" CACHE STRING "Default location for registry based settings")
SET(CONFIG_DEFAULT_CONF_OLD_LOCATION	"old://\${exe-path}/nsc.ini" CACHE STRING "Default location for old config file")
SET(CONFIG_DEFAULT_CONF_INI_LOCATION	"${CONFIG_DEFAULT_CONF_INI_BASE}nsclient.ini" CACHE STRING "Default location for ini files")
SET(CONFIG_BOOT_CONF_LOCATION			"dummy://" CACHE STRING "Default location for dummy settings stores")


IF(WIN32)
	SET(CONFIG_MODULE_FOLDER "\${shared-path}/modules" CACHE STRING "modules folder")
	SET(CONFIG_WEB_FOLDER "\${shared-path}/web" CACHE STRING "web folder")
	SET(CONFIG_SCRIPTS_FOLDER "\${shared-path}/scripts" CACHE STRING "script folder")
	SET(CONFIG_CERT_FOLDER "\${shared-path}/security" CACHE STRING "securoty (certificates) folder")
	SET(CONFIG_DEFAULT_CACHE_PATH "\${shared-path}/cache" CACHE STRING "Path for caching stuff")
ELSE()
	SET(CONFIG_MODULE_FOLDER "${CMAKE_INSTALL_PREFIX}/lib/nsclient/modules" CACHE STRING "modules folder")
	SET(CONFIG_WEB_FOLDER "${CMAKE_INSTALL_PREFIX}/share/nsclient/web" CACHE STRING "web folder")
	SET(CONFIG_SCRIPTS_FOLDER "${CMAKE_INSTALL_PREFIX}/share/nsclient/scripts" CACHE STRING "script folder")
	SET(CONFIG_CERT_FOLDER "${CMAKE_INSTALL_PREFIX}/share/nsclient/security" CACHE STRING "securoty (certificates) folder")
	SET(CONFIG_DEFAULT_CACHE_PATH "${CMAKE_INSTALL_PREFIX}/share/nsclient/cache" CACHE STRING "Path for caching stuff")
ENDIF()

CONFIGURE_FILE(
  ${NSCP_INCLUDEDIR}/config.h.in
  ${NSCP_INCLUDEDIR}/config.h
  @ONLY
)
CONFIGURE_FILE(
  ${BUILD_PYTHON_FOLDER}/postbuild.py
  ${BUILD_TARGET_EXE_PATH}/postbuild.py
)
CONFIGURE_FILE(
  ${BUILD_PYTHON_FOLDER}/config.py
  ${BUILD_TARGET_EXE_PATH}/config.py
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${NSCP_LOCAL_INCLUDE_PATH})
INSTALL_FILES(/include FILES ${WCONFIG_H_PATH})

#############################################################################
#
# Build all included and required libraries
#
#############################################################################

MESSAGE(STATUS "Building support libraries")

IF(JSON_SPIRIT_CMAKE)
	MESSAGE(STATUS " - Json-Spirit")
	ADD_SUBDIRECTORY(${JSON_SPIRIT_CMAKE})
	SET_TARGET_PROPERTIES(json_spirit PROPERTIES FOLDER "libraries")
ENDIF(JSON_SPIRIT_CMAKE)
IF(CRYPTOPP_CMAKE)
	MESSAGE(STATUS " - Crypto++")
	SET(CRYPTOPP_BUILD_TESTS FALSE)
	SET(CRYPTOPP_LIBRARY_TYPE "STATIC")
	ADD_SUBDIRECTORY(${CRYPTOPP_CMAKE})
	SET_TARGET_PROPERTIES(cryptlib PROPERTIES FOLDER "libraries")
ENDIF(CRYPTOPP_CMAKE)

IF(GTEST_FOUND AND ( NOT GTEST_GTEST_FOUND OR NOT GTEST_GTEST_MAIN_FOUND ) )
	MESSAGE(STATUS " - google test++")
	IF(BUILD_SHARED_LIBS)
		SET(gtest_force_shared_crt ON CACHE BOOL "Override shared crt")
	ENDIF(BUILD_SHARED_LIBS)
	SET(OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
	SET(BUILD_SHARED_LIBS OFF)
	ADD_SUBDIRECTORY(${GTEST_SRC_DIR} ${PROJECT_BINARY_DIR}/ext/gtest)
	SET(BUILD_SHARED_LIBS ${OLD_BUILD_SHARED_LIBS})
	SET_TARGET_PROPERTIES(gtest_main PROPERTIES FOLDER "libraries")
	SET_TARGET_PROPERTIES(gtest PROPERTIES FOLDER "libraries")
	SET_TARGET_PROPERTIES(gtest_main PROPERTIES COMPILE_DEFINITIONS "_VARIADIC_MAX=10")
	SET_TARGET_PROPERTIES(gtest PROPERTIES COMPILE_DEFINITIONS "_VARIADIC_MAX=10")
	SET(GTEST_GTEST_FOUND TRUE)
	SET(GTEST_GTEST_LIBRARY gtest)
	SET(GTEST_GTEST_MAIN_FOUND TRUE)
	SET(GTEST_GTEST_MAIN_LIBRARY gtest_main)
ENDIF()


MESSAGE(STATUS " - Mongoose")
SET(OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
SET(BUILD_SHARED_LIBS OFF)
SET(ENABLE_SSL ON CACHE BOOL "Override SSL support")
ADD_SUBDIRECTORY("ext/mongoose-cpp")
SET(BUILD_SHARED_LIBS ${OLD_BUILD_SHARED_LIBS})
MESSAGE(STATUS " - Docs")
#ADD_SUBDIRECTORY("ext/docs")

SET(ALL_LIB_NAMES)
MESSAGE(STATUS "Adding libraries")
FILE(GLOB ALL_LIB_PROJECTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "libs/*/CMakeLists.txt")
FOREACH(CURRENT_LIB ${ALL_LIB_PROJECTS})
	GET_FILENAME_COMPONENT(CURRENT_LIB_PATH ${CURRENT_LIB} PATH)
	GET_FILENAME_COMPONENT(CURRENT_LIB_NAME ${CURRENT_LIB_PATH} NAME)
	MESSAGE(STATUS " + Library: ${CURRENT_LIB_PATH} (${CURRENT_LIB_NAME})")
	ADD_SUBDIRECTORY("${CURRENT_LIB_PATH}")
	SET(ALL_LIB_NAMES ${ALL_LIB_NAMES} ${CURRENT_LIB_NAME})
ENDFOREACH(CURRENT_LIB ${ALL_LIB_PROJECTS})


#############################################################################
#
# Construct the bases for plugins
#
#############################################################################

SET(NSCP_DEF_PLUGIN_CPP
	${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_impl.cpp
)
SET(NSCP_DEF_PLUGIN_HPP
	${NSCP_INCLUDEDIR}/utf8.hpp
	${NSCP_INCLUDEDIR}/format.hpp
	${NSCP_INCLUDEDIR}/strEx.h
	${NSCP_INCLUDEDIR}/config.h
	${NSCP_INCLUDEDIR}/utils.h
	${NSCP_INCLUDEDIR}/NSCAPI.h
	${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.hpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.hpp
	${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_impl.hpp
)

SET(NSCP_FILTER_CPP
	${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
)
SET(NSCP_FILTER_HPP
	${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.hpp
	${NSCP_INCLUDEDIR}/parsers/filter/realtime_helper.hpp
	${NSCP_INCLUDEDIR}/parsers/filter/cli_helper.hpp
	${NSCP_INCLUDEDIR}/parsers/where.hpp
)
SET(NSCP_FILTER_LIB
	where_filter
	perfconfig_parser
)

SET(NSCP_DEF_PLUGIN_LIB
	${CMAKE_THREAD_LIBS_INIT}
	${Boost_SYSTEM_LIBRARY}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${EXTRA_LIBS}
	${PROTOBUF_LIBRARY}
    ${ICONV_LIBRARIES}
	nscp_protobuf
	plugin_api
)

#############################################################################
#
# Build everything (not already built lik libraries)!
#
#############################################################################

SET(MAIN_NSCP_TARGET "nscp" "nscp_full_config")

ADD_SUBDIRECTORY("service")
ADD_SUBDIRECTORY("scripts")
ADD_SUBDIRECTORY("web")
ADD_SUBDIRECTORY("files")

LOAD_SECTIONS(ALL_MODULE_NAMES "modules/*/module.cmake" "Modules")
LOAD_SECTIONS(ALL_TOOL_NAMES "tools/*/module.cmake" "Tools")
LOAD_SECTIONS(ALL_CLIENT_NAMES "clients/*/module.cmake" "Clients")
IF(WIN32)
	LOAD_SECTIONS(ALL_INSTALLERS_NAMES "installers/*/CMakeLists.txt" "Installers")
ENDIF(WIN32)
SET_TARGET_PROPERTIES(${MAIN_NSCP_TARGET} ${ALL_CLIENT_NAMES} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES} PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${BUILD_TARGET_EXE_PATH}"
		LIBRARY_OUTPUT_DIRECTORY "${BUILD_TARGET_LIB_PATH}"
        MODULE_OUTPUT_DIRECTORY  "${BUILD_TARGET_LIB_PATH}"
)

ADD_SUBDIRECTORY("docs")

#############################################################################
#
# CTest setup and add testcases
#
#############################################################################
FOREACH(case "none" "xor" "des" "3des" "cast128" "xtea" "blowfish" "twofish" "rc2" "aes" "aes256" "aes192" "aes128" "serpent" "gost" "3way")
	add_nscp_py_test_case(nsca test_nsca ${case})
ENDFOREACH()
add_nscp_py_test(nrpe test_nrpe)
add_nscp_lua_test(lua_nrpe test_nrpe)
add_nscp_py_test(python test_python)
add_nscp_py_test(external_script test_external_script)
IF(WIN32)
# Disabled as is is not a unit test per see, since it requires extenal non mocked stimula
#	add_nscp_py_test(eventlog test_eventlog)
	add_nscp_py_test(w32_system test_w32_system)
	add_nscp_py_test(w32_system test_w32_file)
ENDIF(WIN32)

#############################################################################
#
# CPacke setup (currently not used)
#
#############################################################################
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "NSClient++")
SET(CPACK_PACKAGE_VENDOR "nsclient.org")
#SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/docs/readme.txt")
#SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/Copyright.txt")

SET(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_SERIES}.${VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${VERSION_BUILD})

SET(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")
IF(WIN32 AND NOT UNIX)
	SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
	SET(CPACK_GENERATOR ZIP)
ELSE(WIN32 AND NOT UNIX)
	SET(CPACK_STRIP_FILES "bin/MyExecutable")
	SET(CPACK_SOURCE_STRIP_FILES "")
	SET(CPACK_PACKAGE_EXECUTABLES "nscp")
ENDIF(WIN32 AND NOT UNIX)

set(CPACK_PACKAGE_VERSION ${NSCP_FULL_VERSION})
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "nscp")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "Michael Medin")
set(CPACK_PACKAGE_VENDOR "MySolutions NORDIC")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

# Debian package stuff
set(CPACK_DEBIAN_PACKAGE_NAME        ${CPACK_PACKAGE_NAME})
#set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libboost-all-dev, libfftw3-dev")
set(CPACK_PACKAGE_CONTACT "Michael Medin <michael@medin.name>")
set(CPACK_DEBIAN_PACKAGE_PRIORITY    "optional")
set(CPACK_DEBIAN_PACKAGE_SECTION     "devel")

include(CPack)

