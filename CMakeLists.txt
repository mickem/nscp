cmake_minimum_required(VERSION 3.5)

project(NSCP CXX C)

enable_testing()
if(COMMAND CMAKE_POLICY)
  cmake_policy(SET CMP0011 NEW)
endif(COMMAND CMAKE_POLICY)

set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>")

# ##############################################################################
#
# Setup cmake enviornment and include custom config overrides
#
# ##############################################################################
if(NSCP_CMAKE_CONFIG)
  message(STATUS "Reading custom variables from: ${NSCP_CMAKE_CONFIG}")
  include(${NSCP_CMAKE_CONFIG})
elseif(EXISTS ${CMAKE_SOURCE_DIR}/build.cmake)
  message(
    STATUS "Reading custom variables from: ${CMAKE_SOURCE_DIR}/build.cmake")
  include(${CMAKE_SOURCE_DIR}/build.cmake)
elseif(EXISTS ${PROJECT_BINARY_DIR}/build.cmake)
  message(
    STATUS "Reading custom variables from: ${PROJECT_BINARY_DIR}/build.cmake")
  include(${PROJECT_BINARY_DIR}/build.cmake)
elseif(EXISTS ${PROJECT_BINARY_DIR}/../build.cmake)
  message(
    STATUS "Reading custom variables from: ${PROJECT_BINARY_DIR}/../build.cmake"
  )
  include(${PROJECT_BINARY_DIR}/../build.cmake)
else(NSCP_CMAKE_CONFIG)
  message(
    STATUS
      "NO build configuration found, see NSCP_CMAKE_CONFIG=${NSCP_CMAKE_CONFIG}"
  )
endif(NSCP_CMAKE_CONFIG)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build/cmake;${CMAKE_MODULE_PATH}")
set(MODULE_SUBFOLDER "modules")

if(WIN32)
  set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})
  include(CheckIncludeFileCXX)
  check_include_file_cxx(atlbase.h FOUND_ATL)
  if(FOUND_ATL)
    set(VSTUDIO_EXPRESS OFF)
  else(FOUND_ATL)
    message(
      WARNING
        "Visual studio express seem to be used, some modules will be missing")
    set(VSTUDIO_EXPRESS ON)
  endif(FOUND_ATL)
endif()

set(BUILD_PYTHON_FOLDER "${CMAKE_SOURCE_DIR}/build/python")
set(BUILD_CMAKE_FOLDER "${CMAKE_SOURCE_DIR}/build/cmake")
set(BUILD_BATCH_FOLDER "${CMAKE_SOURCE_DIR}/build/batch")
set(BUILD_ROOT_FOLDER "${CMAKE_SOURCE_DIR}")
set(BUILD_TARGET_EXE_PATH "${PROJECT_BINARY_DIR}")
set(BUILD_TARGET_ROOT_PATH "${PROJECT_BINARY_DIR}")
set(BUILD_TARGET_LIB_PATH "${BUILD_TARGET_EXE_PATH}/${MODULE_SUBFOLDER}")
set(BUILD_TARGET_INSTLIB_PATH "${BUILD_TARGET_EXE_PATH}/installer-dlls")
set(NSCP_PROJECT_BINARY_DIR ${PROJECT_BINARY_DIR})
set(NSCP_PROJECT_BINARY_DIR_NATIVE ${PROJECT_BINARY_DIR})
if(WIN32)
  string(REPLACE "/" "\\" NSCP_PROJECT_BINARY_DIR_NATIVE
                 ${NSCP_PROJECT_BINARY_DIR_NATIVE})
endif()
if(NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr")
endif()
set(MODULE_TARGET_FOLDER lib/nsclient/modules)
set(LIB_TARGET_FOLDER lib)
set(SYSTEMD_TARGET_FOLDER /lib/systemd/system)
set(SBIN_TARGET_FOLDER sbin)
set(BIN_TARGET_FOLDER bin)
set(LOG_TARGET_FOLDER /var/log)
set(ETC_TARGET_FOLDER /etc)

set(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DBOOST_PHOENIX_STL_TUPLE_H_
                        -DBOOST_LIB_DIAGNOSTIC -DBOOST_AUTO_LINK_SYSTEM)
if(NOT DEFINED USE_STATIC_RUNTIME)
  set(USE_STATIC_RUNTIME FALSE)
endif(NOT DEFINED USE_STATIC_RUNTIME)
if(USE_STATIC_RUNTIME)
  message(STATUS "Using static runtime")
  set(BUILD_SHARED_LIBS OFF)
  set(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DBOOST_ALL_NO_LIB)
  add_definitions(-Dnscp_protobuf_NOLIB -Dplugin_api_NOLIB -Dwhere_filter_NOLIB
                  -Djson_protobuf_NOLIB -Dlua_protobuf_NOLIB)
  if(WIN32)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME ON)
    set(BOOST_USE_MULTITHREADED ON)
  endif(WIN32)
  set(CRYPTOPP_RUNTIME_TYPE "STATIC")
  set(JSON_SPIRIT_LIBRARY_TYPE "STATIC")
  set(BUILD_STATIC_LIBS ON)
else(USE_STATIC_RUNTIME)
  message(STATUS "Using dynamic runtime")
  set(NSCP_GLOBAL_DEFINES
      ${NSCP_GLOBAL_DEFINES} -DBOOST_ALL_NO_LIB -DBOOST_THREAD_DYN_LINK
      -DBOOST_PROGRAM_OPTIONS_DYN_LINK)
  set(BUILD_SHARED_LIBS ON)
  if(WIN32)
    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_RUNTIME OFF)
    set(BOOST_USE_MULTITHREADED ON)
  endif(WIN32)
  set(CRYPTOPP_RUNTIME_TYPE "SHARED")
  set(JSON_SPIRIT_LIBRARY_TYPE "SHARED")
  set(BUILD_STATIC_LIBS OFF)
endif(USE_STATIC_RUNTIME)

if(WIN32)
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
  set(CPACK_GENERATOR ZIP)
else()
  set(CMAKE_INSTALL_PREFIX "/usr")
  execute_process(
    COMMAND lsb_release -si
    OUTPUT_VARIABLE DISTRIBUTION
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(DISTRIBUTION STREQUAL "Fedora" OR DISTRIBUTION STREQUAL "CentOS")
    execute_process(
      COMMAND rpm -qa *-release
      OUTPUT_VARIABLE LINUXVERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX REPLACE ".*\\.(el[0-9]+)\\..*" "\\1" LINUXDIST ${LINUXVERSION})
    if(LINUXDIST STREQUAL "el7")
      message(
        STATUS "Linux detected as redhat clone with systemd (${LINUXDIST})")
      set(SYSTEMD ON)
      set(INITD OFF)
    else()
      message(STATUS "Linux detected as redhat clone with initd (${LINUXDIST})")
      set(SYSTEMD OFF)
      set(INITD ON)
    endif()
    set(REDHAT_CLONE ON)
    set(DEBIAN_CLONE OFF)
  else()
    message(STATUS "Linux detected as debian clone with initd")
    set(SYSTEMD OFF)
    set(INITD ON)
    set(REDHAT_CLONE OFF)
    set(DEBIAN_CLONE ON)
  endif()
endif()

include(${BUILD_CMAKE_FOLDER}/SetMultiThread.cmake)
include(${BUILD_CMAKE_FOLDER}/GoogleProtoBuf.cmake)
include(${BUILD_CMAKE_FOLDER}/functions.cmake)

# ##############################################################################
#
# Find all dependencies and report anything missing.
#
# ##############################################################################
include(${BUILD_CMAKE_FOLDER}/dependencies.cmake)

# ##############################################################################
#
# Validate all required dependencies and terminate on failures.
#
# ##############################################################################
message(STATUS "Validating required dependencies:")
set(HAVE_REQUIRED_DEPENDENCIES TRUE)
if(NOT Python3_Interpreter_FOUND)
  set(HAVE_REQUIRED_DEPENDENCIES FALSE)
  message(WARNING " - python(exe) not found! (Required by the build process)")
endif()
if(NOT PROTOBUF_FOUND)
  set(HAVE_REQUIRED_DEPENDENCIES FALSE)
  message(
    WARNING
      " - protocol buffers not found! XXX=${PROTOBUF_LIBRARYDIR}! (Required by nsclient++)"
  )
endif(NOT PROTOBUF_FOUND)
if(NOT Boost_FOUND)
  set(HAVE_REQUIRED_DEPENDENCIES FALSE)
  message(
    WARNING
      " - boost libraries was not found: ${Boost_ERROR_REASON} (Required by nsclient++)"
  )
endif(NOT Boost_FOUND)
if(NOT WIN32)
  # IF(NOT CMAKE_USE_PTHREADS_INIT) SET(HAVE_REQUIRED_DEPENDENCIES FALSE)
  # MESSAGE(WARNING " - Could not find POSIX threads") ENDIF(NOT
  # CMAKE_USE_PTHREADS_INIT)
endif(NOT WIN32)

if(NOT HAVE_REQUIRED_DEPENDENCIES)
  message(FATAL_ERROR " - missing required dependencies found!")
else(NOT HAVE_REQUIRED_DEPENDENCIES)
  message(
    STATUS
      " - All required dependecies found! (some modules and features migh stil be missing though)"
  )
endif(NOT HAVE_REQUIRED_DEPENDENCIES)

install(
  FILES
    ${PROJECT_BINARY_DIR}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.zip
  DESTINATION .)

if(USE_STATIC_RUNTIME)

else(USE_STATIC_RUNTIME)
  if(WIN32)

    string(REPLACE ".lib" ".dll" PY_LIB_RELEASE_DLL ${Python3_LIBRARIES})
    get_filename_component(_NAME "${PY_LIB_RELEASE_DLL}" NAME)
    message(
      STATUS
        "Assuming python lib is in: ${Python3_RUNTIME_LIBRARY_DIRS}/${_NAME}")

    message(STATUS "Copying redist libraries to target")
    message(STATUS " + ${_NAME}")

    copy_file(${Python3_RUNTIME_LIBRARY_DIRS}/${_NAME} ${PROJECT_BINARY_DIR})
    install(FILES "${Python3_RUNTIME_LIBRARY_DIRS}/${_NAME}" DESTINATION .)

    find_redist(REDIST_LIST)
    foreach(LIB ${REDIST_LIST})
      get_filename_component(_NAME ${LIB} NAME)
      message(STATUS " +  ${_NAME}")
      copy_file(${LIB} ${PROJECT_BINARY_DIR})
      install(FILES "${LIB}" DESTINATION .)
    endforeach()
    set(LIBS
        DATE_TIME
        FILESYSTEM
        PROGRAM_OPTIONS
        PYTHON311
        "REGEX"
        SYSTEM
        THREAD
        CHRONO)
    foreach(LIB ${LIBS})
      if(Boost_${LIB}_LIBRARY_RELEASE)
        string(REPLACE ".lib" ".dll" Boost_${LIB}_LIBRARY_RELEASE_DLL
                       ${Boost_${LIB}_LIBRARY_RELEASE})
        get_filename_component(_NAME "${Boost_${LIB}_LIBRARY_RELEASE_DLL}" NAME)
        message(STATUS " + ${_NAME} - ${LIB}")
        copy_file(${Boost_${LIB}_LIBRARY_RELEASE_DLL} ${PROJECT_BINARY_DIR})
        string(REPLACE ".lib" ".dll" Boost_${LIB}_LIBRARY_DEBUG_DLL
                       ${Boost_${LIB}_LIBRARY_DEBUG})
        # COPY_FILE(${Boost_${LIB}_LIBRARY_DEBUG_DLL} ${PROJECT_BINARY_DIR})
        install(FILES ${Boost_${LIB}_LIBRARY_RELEASE_DLL} DESTINATION .)
      endif()
    endforeach()
    set(LIBS LIB_EAY SSL_EAY PROTOBUF_LIBRARY)
    foreach(LIB ${LIBS})
      string(REPLACE ".lib" ".dll" ${LIB}_RELEASE_DLL "${${LIB}_RELEASE}")
      if(EXISTS "${${LIB}_RELEASE_DLL}")
        get_filename_component(_NAME "${${LIB}_RELEASE_DLL}" NAME)
        message(STATUS " + ${_NAME}")
        copy_file("${${LIB}_RELEASE_DLL}" ${PROJECT_BINARY_DIR})
        install(FILES "${${LIB}_RELEASE_DLL}" DESTINATION .)
      else()
        get_filename_component(_PATH "${${LIB}_RELEASE_DLL}" DIRECTORY)
        string(REPLACE ".lib" "*.dll" LIB_PATTERN "${${LIB}_RELEASE}")
        file(GLOB FILES ${LIB_PATTERN})
        foreach(FILE ${FILES})
          get_filename_component(_NAME "${FILE}" NAME)
          message(STATUS " + ${_NAME}")
          copy_file("${FILE}" ${PROJECT_BINARY_DIR})
          install(FILES "${FILE}" DESTINATION .)
        endforeach()
      endif()
    endforeach()

  endif()
endif(USE_STATIC_RUNTIME)

if(PYTHON_DIST)
  set(LIBS _socket.pyd python27.dll python27.zip unicodedata.pyd)
  foreach(LIB ${LIBS})
    copy_file(${PYTHON_DIST}/${LIB} ${PROJECT_BINARY_DIR})
    install(FILES ${PYTHON_DIST}/${LIB} DESTINATION .)
  endforeach()
endif(PYTHON_DIST)

# ##############################################################################
#
# Bump version number (if requested) and set up variables with current version
#
# ##############################################################################
if(NOT NSCP_VERSION)
  set(NSCP_VERSION "0.0.0")
  message(WARNING "Version not specified, building ${NSCP_VERSION}")
endif(NOT NSCP_VERSION)

set(NSCP_LIB_VERSION "${NSCP_VERSION}")
set(NSCP_FULL_VERSION "${NSCP_VERSION}")
string(REPLACE "." "," NSCP_VERSION_COMMA ${NSCP_VERSION})
string(TIMESTAMP NSCP_VERSION_DATE "%Y-%m-%d" UTC)
if(WIN32)
  set(VERSION_ARCH Win32)
  if(CMAKE_CL_64)
    set(VERSION_ARCH x64)
  endif(CMAKE_CL_64)
else(WIN32)
  set(VERSION_ARCH Unknown)
endif(WIN32)

# ##############################################################################
#
# Setup c/C++ build enviornment and flags
#
# ##############################################################################
set(NSCP_INCLUDEDIR ${CMAKE_SOURCE_DIR}/include)
include_directories(${NSCP_INCLUDEDIR} ${CMAKE_CURRENT_BINARY_DIR}/libs)
set(CMAKE_LIBRARY_PATH
    "${CMAKE_LIBRARY_PATH};${OPENSSL_LIBRARY_DIR};${NSCP_LIBRARYDIR};${BOOST_LIBRARYDIR}"
)

set_multithread()

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-long-long -pedantic -Wno-unused-parameter"
  )
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  message(STATUS "Added parallel build arguments")
endif()

if(NOT SHARED_LIBS)
  if(WIN32)
    option(SHARED_LIBS "Compile shared libraries" OFF)
  else(WIN32)
    option(SHARED_LIBS "Compile shared libraries" ON)
  endif(WIN32)
endif(NOT SHARED_LIBS)

include_directories(${Boost_INCLUDE_DIRS})
include_directories("${PROTOBUF_INCLUDE_DIR}")
include_directories("${PROJECT_BINARY_DIR}")
add_definitions(-DUNICODE -D_UNICODE)

if(WIN32)
  set(RUNDIR
      "c:/nscp"
      CACHE PATH "Not really used in WIN32")
else(WIN32)
  set(USERLIB_ROOT
      /usr
      CACHE PATH "Other installation prefix for dependent libraries")
  set(LIB_INSTALL_DIR
      "lib"
      CACHE STRING "Default path for libraries within ${CMAKE_INSTALL_PREFIX}")
  set(RUNDIR
      "/usr/wt/run"
      CACHE
        PATH
        "Default path for wt session management (only used by FCGI connector; not relative to CMAKE_INSTALL_PREFIX)"
  )
  set(CONFIGURATION
      /etc/nscp/boot.ini
      CACHE PATH "Path to the boot configuration file")
  set(MAIN_CONFIGURATION
      /etc/nscp/nscp.xml
      CACHE PATH "Path for the client configurtion file")
  set(DEPLOYROOT
      /usr/bin
      CACHE PATH "Path to deploy examples into)")
endif(WIN32)

if(CMAKE_COMPILER_IS_GNUCXX)
  if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC")
  endif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
endif(CMAKE_COMPILER_IS_GNUCXX)

set(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DUNICODE -D_UNICODE)
if(WIN32)
  set(NSCP_GLOBAL_DEFINES
      "${NSCP_GLOBAL_DEFINES} -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501 -DWINVER=0x0501"
  )
endif(WIN32)

if(WIN32)
  set(EXTRA_LIBS ${EXTRA_LIBS} Userenv ole32)
endif(WIN32)

if(CMAKE_HOST_UNIX)
  set(UNIX_FLAVOUR
      LINUX
      CACHE STRING "Choose your unix flavour UNIX, AIX, HPUX" FORCE)
  if(UNIX_FLAVOUR STREQUAL LINUX)
    set(NSCP_GLOBAL_DEFINES ${NSCP_GLOBAL_DEFINES} -DLINUX)
  endif(UNIX_FLAVOUR STREQUAL LINUX)

  set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_DL_LIBS})
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      RelWithDebInfo
      CACHE
        STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif(NOT CMAKE_BUILD_TYPE)

link_directories(${CMAKE_LIBRARY_PATH} ${OPENSSL_LIBRARY_DIR}
                 ${NSCP_LIBRARYDIR} ${BOOST_LIBRARYDIR})

# Compile time constants & make sure our build finds it

set(CONFIG_DEFAULT_SERVICE_NAME
    "NSCP"
    CACHE STRING "The name of the service")
set(CONFIG_DEFAULT_SERVICE_DESC
    "Monitoring Agent (Provides performance data for monitoring solutions like Nagios)"
    CACHE STRING "The description used for the service")
set(CONFIG_DEFAULT_SERVICE_DEPS
    ""
    CACHE STRING "Any service dependencies to use for the default service")

set(CONFIG_CRASH_SUBMIT_URL
    "https://crash.nsclient.org/post"
    CACHE STRING "Where to submit crash dump files")
set(CONFIG_CRASH_ARCHIVE_FOLDER
    "\${shared-path}/crash-dumps"
    CACHE STRING "Where to archive chras dump files on the local filesystem")

set(CONFIG_DEFAULT_CONF_REG_LOCATION
    "registry://HKEY_LOCAL_MACHINE/software/NSClient++"
    CACHE STRING "Default location for registry based settings")
set(CONFIG_DEFAULT_CONF_OLD_LOCATION
    "old://\${exe-path}/nsc.ini"
    CACHE STRING "Default location for old config file")
if(WIN32)
  set(INSTALL_FILES_BASE "")

  set(CONFIG_BOOT_CONF_LOCATION
      "\${exe-path}/boot.ini"
      CACHE
        STRING
        "Location of boot ini (the file which specified where to load the config)"
  )
  set(CONFIG_DEFAULT_CONF_LOCATION
      "old"
      CACHE STRING "Default configuration store")
  set(CONFIG_DEFAULT_CONF_INI_BASE
      "ini://\${shared-path}/"
      CACHE STRING "Base location for ini files")
  set(CONFIG_MODULE_FOLDER
      "\${exe-path}/modules"
      CACHE STRING "modules folder")
  set(CONFIG_WEB_FOLDER
      "\${shared-path}/web"
      CACHE STRING "web folder")
  set(CONFIG_SCRIPTS_FOLDER
      "\${exe-path}/scripts"
      CACHE STRING "script folder")
  set(CONFIG_CERT_FOLDER
      "\${shared-path}/security"
      CACHE STRING "securoty (certificates) folder")
  set(CONFIG_DEFAULT_CACHE_PATH
      "\${shared-path}/cache"
      CACHE STRING "Path for caching stuff")
  set(CONFIG_LOG_FOLDER
      "\${shared-path}/log"
      CACHE STRING "Path for log files")
  set(CONFIG_DEFAULT_LOG_LOCATION
      "\${exe-path}/nsclient.log"
      CACHE STRING "PAth for log file")
else()
  set(INSTALL_FILES_BASE "share/nsclient/")

  set(CONFIG_BOOT_CONF_LOCATION
      "/etc/nsclient/boot.ini"
      CACHE
        STRING
        "Location of boot ini (the file which specified where to load the config)"
  )
  set(CONFIG_DEFAULT_CONF_LOCATION
      "ini"
      CACHE STRING "Default configuration store")
  set(CONFIG_DEFAULT_CONF_INI_BASE
      "ini:///etc/nsclient/"
      CACHE STRING "Base location for ini files")
  set(CONFIG_MODULE_FOLDER
      "\${shared-path}/modules"
      CACHE STRING "modules folder")
  set(CONFIG_WEB_FOLDER
      "\${shared-path}/web"
      CACHE STRING "web folder")
  set(CONFIG_SCRIPTS_FOLDER
      "\${shared-path}/scripts"
      CACHE STRING "script folder")
  set(CONFIG_CERT_FOLDER
      "\${shared-path}/security"
      CACHE STRING "securoty (certificates) folder")
  set(CONFIG_DEFAULT_CACHE_PATH
      "\${shared-path}/cache"
      CACHE STRING "Path for caching stuff")
  set(CONFIG_LOG_FOLDER
      "\${shared-path}/log"
      CACHE STRING "Path for log files")
  set(CONFIG_DEFAULT_LOG_LOCATION
      "\${shared-path}/log/nsclient.log"
      CACHE STRING "PAth for log file")
endif()
set(CONFIG_DEFAULT_CONF_INI_LOCATION
    "${CONFIG_DEFAULT_CONF_INI_BASE}nsclient.ini"
    CACHE STRING "Default location for ini files")

configure_file(${NSCP_INCLUDEDIR}/config.h.in
               ${PROJECT_BINARY_DIR}/include/config.h @ONLY)
configure_file(${NSCP_INCLUDEDIR}/version.hpp.in
               ${PROJECT_BINARY_DIR}/include/version.hpp @ONLY)
if(NOT WIN32)
  configure_file(${CMAKE_SOURCE_DIR}/files/nscp.spec.in
                 ${PROJECT_BINARY_DIR}/SPECS/nscp.spec @ONLY)
endif()

# INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${NSCP_LOCAL_INCLUDE_PATH})
# WHat does this do? INSTALL_FILES(/include FILES ${WCONFIG_H_PATH})

if(WIN32)
  set(NSCP_ERROR_CPP ${NSCP_INCLUDEDIR}/error/error_w32.cpp)
  set(NSCP_ERROR_HPP ${NSCP_INCLUDEDIR}/error/error_w32.hpp
                     ${NSCP_INCLUDEDIR}/error/error.hpp)
else()
  set(NSCP_ERROR_CPP)
  set(NSCP_ERROR_HPP ${NSCP_INCLUDEDIR}/error/error.hpp)
endif()

# ##############################################################################
#
# Build all included and required libraries
#
# ##############################################################################

message(STATUS "Building support libraries")

if(JSON_SPIRIT_CMAKE)
  message(STATUS " - Json-Spirit")
  add_definitions(-DBOOST_AUTO_LINK_SYSTEM)
  add_subdirectory(${JSON_SPIRIT_CMAKE})
  set_target_properties(json_spirit PROPERTIES FOLDER "libraries")
endif(JSON_SPIRIT_CMAKE)
if(CRYPTOPP_CMAKE)
  message(STATUS " - Crypto++")
  set(CRYPTOPP_BUILD_TESTS FALSE)
  set(CRYPTOPP_LIBRARY_TYPE "STATIC")
  add_subdirectory(${CRYPTOPP_CMAKE})
  set_target_properties(cryptlib PROPERTIES FOLDER "libraries")
endif(CRYPTOPP_CMAKE)

if(GTEST_FOUND AND (NOT GTEST_GTEST_FOUND OR NOT GTEST_GTEST_MAIN_FOUND))
  message(STATUS " - google test++")
  if(BUILD_SHARED_LIBS)
    set(gtest_force_shared_crt
        ON
        CACHE BOOL "Override shared crt")
  endif(BUILD_SHARED_LIBS)
  set(OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF)
  add_subdirectory(${GTEST_SRC_DIR} ${PROJECT_BINARY_DIR}/ext/gtest)
  set(BUILD_SHARED_LIBS ${OLD_BUILD_SHARED_LIBS})
  set_target_properties(gtest_main PROPERTIES FOLDER "libraries")
  set_target_properties(gtest PROPERTIES FOLDER "libraries")
  set_target_properties(gtest_main PROPERTIES COMPILE_DEFINITIONS
                                              "_VARIADIC_MAX=10")
  set_target_properties(gtest PROPERTIES COMPILE_DEFINITIONS "_VARIADIC_MAX=10")
  set(GTEST_GTEST_FOUND TRUE)
  set(GTEST_GTEST_LIBRARY gtest)
  set(GTEST_GTEST_MAIN_FOUND TRUE)
  set(GTEST_GTEST_MAIN_LIBRARY gtest_main)
endif()

if(NETLIB_FOUND)
  if(NOT (Boost_VERSION LESS 105400))
    message(STATUS " - c++-netlib")
    set(CPP-NETLIB_BUILD_SHARED_LIBS
        ON
        CACHE BOOL "Override")
    set(CPP-NETLIB_BUILD_SINGLE_LIB
        ON
        CACHE BOOL "Override")
    set(CPP-NETLIB_BUILD_TESTS
        OFF
        CACHE BOOL "Override")
    set(CPP-NETLIB_BUILD_EXAMPLES
        OFF
        CACHE BOOL "Override")
    set(CPP-NETLIB_BUILD_FOLDER
        "../../"
        CACHE STRING "Override")
    set(OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(ext/cpp-netlib)
    set(BUILD_SHARED_LIBS ${OLD_BUILD_SHARED_LIBS})
    set_target_properties(cppnetlib-client-connections PROPERTIES FOLDER
                                                                  "libraries")
    set_target_properties(cppnetlib-server-parsers PROPERTIES FOLDER
                                                              "libraries")
    set_target_properties(cppnetlib-uri PROPERTIES FOLDER "libraries")
    set_target_properties(cppnetlib-client-connections
                          PROPERTIES COMPILE_DEFINITIONS "BOOST_ALL_NO_LIB")
  endif()
endif()

set(ALL_LIB_NAMES)
message(STATUS "Adding libraries")
file(
  GLOB ALL_LIB_PROJECTS
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "libs/*/CMakeLists.txt")
foreach(CURRENT_LIB ${ALL_LIB_PROJECTS})
  get_filename_component(CURRENT_LIB_PATH ${CURRENT_LIB} PATH)
  get_filename_component(CURRENT_LIB_NAME ${CURRENT_LIB_PATH} NAME)
  if(CURRENT_LIB_NAME STREQUAL "protobuf_net" OR CURRENT_LIB_NAME STREQUAL
                                                 "dotnet-plugin-api")
    message(
      STATUS " - Skipping Library: ${CURRENT_LIB_PATH} (${CURRENT_LIB_NAME})")
    continue()
  endif()
  message(STATUS " + Library: ${CURRENT_LIB_PATH} (${CURRENT_LIB_NAME})")
  add_subdirectory("${CURRENT_LIB_PATH}")
  set(ALL_LIB_NAMES ${ALL_LIB_NAMES} ${CURRENT_LIB_NAME})
endforeach(CURRENT_LIB ${ALL_LIB_PROJECTS})

set(ALL_ZIP_MODULES)
message(STATUS "Adding zip-modules")
file(
  GLOB ALL_ZIP_PROJECTS
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "scripts/modules/*/CMakeLists.txt")
foreach(CURRENT ${ALL_ZIP_PROJECTS})
  get_filename_component(CURRENT_PATH ${CURRENT} PATH)
  get_filename_component(CURRENT_NAME ${CURRENT_PATH} NAME)
  message(STATUS " + Zip-module: ${CURRENT_PATH} (${CURRENT_NAME})")
  add_subdirectory("${CURRENT_PATH}")
  set(ALL_ZIP_MODULES ${ALL_ZIP_MODULES} ${CURRENT_NAME})
endforeach(CURRENT ${ALL_ZIP_PROJECTS})

# ##############################################################################
#
# Construct the bases for plugins
#
# ##############################################################################

set(NSCP_DEF_PLUGIN_CPP
    ${NSCP_INCLUDEDIR}/utf8.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_impl.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.cpp)
set(NSCP_DEF_PLUGIN_HPP
    ${NSCP_INCLUDEDIR}/utf8.hpp
    ${NSCP_INCLUDEDIR}/str/format.hpp
    ${NSCP_INCLUDEDIR}/str/utils.hpp
    # ${NSCP_INCLUDEDIR}/config.h
    ${NSCP_INCLUDEDIR}/NSCAPI.h
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.hpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.hpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_common_options.hpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.hpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_impl.hpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_plugin_wrapper.hpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.hpp)

set(NSCP_FILTER_CPP ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.cpp
                    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.cpp)
set(NSCP_FILTER_HPP
    ${NSCP_INCLUDEDIR}/parsers/filter/modern_filter.hpp
    ${NSCP_INCLUDEDIR}/parsers/filter/realtime_helper.hpp
    ${NSCP_INCLUDEDIR}/parsers/filter/cli_helper.hpp
    ${NSCP_INCLUDEDIR}/parsers/where.hpp)
set(NSCP_FILTER_LIB where_filter perfconfig_parser)

set(NSCP_CLIENT_CPP
    ${NSCP_INCLUDEDIR}/utf8.cpp
    ${NSCP_INCLUDEDIR}/client/command_line_parser.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.cpp)
set(NSCP_CLIENT_HPP
    ${NSCP_INCLUDEDIR}/utf8.hpp
    ${NSCP_INCLUDEDIR}/client/command_line_parser.hpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.hpp)

set(NSCP_DEF_PLUGIN_LIB
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${EXTRA_LIBS}
    ${PROTOBUF_LIBRARY}
    ${ICONV_LIBRARIES}
    nscp_protobuf
    plugin_api)

# ##############################################################################
#
# Build everything (not already built lik libraries)!
#
# ##############################################################################

set(MAIN_NSCP_TARGET "nscp" "nscp_full_config")

add_subdirectory("service")
add_subdirectory("scripts")
add_subdirectory("op5/config")
add_subdirectory("op5/scripts")
add_subdirectory("web")
add_subdirectory("files")

load_sections(ALL_MODULE_NAMES "modules/*/module.cmake" "Modules")
load_sections(ALL_CLIENT_NAMES "clients/*/module.cmake" "Clients")
if(WIN32)
  load_sections(ALL_TOOL_NAMES "tools/*/module.cmake" "Tools")
endif(WIN32)
set_target_properties(
  nscp ${ALL_CLIENT_NAMES} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES}
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BUILD_TARGET_EXE_PATH}"
             LIBRARY_OUTPUT_DIRECTORY "${BUILD_TARGET_LIB_PATH}"
             MODULE_OUTPUT_DIRECTORY "${BUILD_TARGET_LIB_PATH}")

add_subdirectory("docs")

add_custom_target(
  nscp_full_config
  COMMAND ${CMAKE_BINARY_DIR}/nscp settings --settings ini://nsclient-full.ini
          --add-defaults --load-all --generate
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS nscp ${ALL_MODULE_NAMES} ${ALL_ZIP_MODULE_NAMES})

if(WIN32)
  load_sections(ALL_INSTALLERS_NAMES "installers/*/CMakeLists.txt" "Installers")
endif(WIN32)

# ##############################################################################
#
# CTest setup and add testcases
#
# ##############################################################################
foreach(
  case
  "none"
  "xor"
  "des"
  "3des"
  "cast128"
  "xtea"
  "blowfish"
  "twofish"
  "rc2"
  "aes"
  "aes256"
  "aes192"
  "aes128"
  "serpent"
  "gost"
  "3way")
  add_nscp_py_test_case(nsca test_nsca ${case})
endforeach()
add_nscp_py_test(nrpe test_nrpe)
add_nscp_lua_test(lua_nrpe test_nrpe)
add_nscp_py_test(python test_python)
add_nscp_py_test(log_file test_log_file)
add_nscp_py_test(external_script test_external_script)
add_nscp_py_test(scheduler test_scheduler)
if(WIN32)
  # Disabled as is is not a unit test per see, since it requires extenal non
  # mocked stimula add_nscp_py_test(eventlog test_eventlog) notepad no longer
  # exists add_nscp_py_test(w32_system test_w32_system)
  add_nscp_py_test(w32_system test_w32_file)
  add_nscp_py_test(w32_taskScheduler test_w32_schetask)
endif(WIN32)

# ##############################################################################
#
# CPacke setup
#
# ##############################################################################
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "NSClient++")
set(CPACK_PACKAGE_VENDOR "MySolutions NORDIC")
set(CPACK_PACKAGE_CONTACT "Michael Medin <michael@medin.name>")
# SET(CPACK_PACKAGE_DESCRIPTION_FILE
# "${CMAKE_CURRENT_SOURCE_DIR}/docs/readme.txt") SET(CPACK_RESOURCE_FILE_LICENSE
# "${CMAKE_CURRENT_SOURCE_DIR}/Copyright.txt")

set(CPACK_PACKAGE_VERSION ${NSCP_FULL_VERSION})

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Monitoring Agent")
set(CPACK_PACKAGE_NAME "nscp")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_SERIES}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_MINOR}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "nscp-folder")
set(CPACK_RPM_PACKAGE_DESCRIPTION
    "NSClient++ (nscp) aims to be a simple yet powerful and secure monitoring daemon. It was built for Naemon/Nagios/Icinga, but nothing in the daemon is Naemon/Nagios/Icinga specific and it can be used in many other scenarios where you want to receive/distribute check metrics."
)

if(WIN32)
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
  set(CPACK_PACKAGE_FILE_NAME "NSCP-${CPACK_PACKAGE_VERSION}-${VERSION_ARCH}")
  set(CPACK_GENERATOR ZIP)
else()
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
  set(CPACK_PACKAGE_FILE_NAME "NSCP-${CPACK_PACKAGE_VERSION}-${VERSION_ARCH}")
  set(CPACK_GENERATOR ZIP)
  set(CPACK_SOURCE_GENERATOR "TGZ")
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY TRUE)
endif()

include(CPack)
