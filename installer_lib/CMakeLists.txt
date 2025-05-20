cmake_minimum_required(VERSION 3.5)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(TARGET installer_lib)
project(${TARGET} CXX)

set(NSCP_INCLUDEDIR ${CMAKE_SOURCE_DIR}/../include)
set(BUILD_PYTHON_FOLDER "${CMAKE_SOURCE_DIR}/../build/python")
set(BUILD_CMAKE_FOLDER "${CMAKE_SOURCE_DIR}/../build/cmake")

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost COMPONENTS system filesystem)
if(Boost_FOUND)
    message(
            STATUS " - boost found in: ${Boost_INCLUDE_DIRS} / ${Boost_LIBRARY_DIRS}")
else(Boost_FOUND)
    message(FATAL_ERROR " ! boost not found: BOOST_ROOT=${BOOST_ROOT}")
endif(Boost_FOUND)
SET(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL)
if(OPENSSL_FOUND)
    message(
            STATUS " - OpenSSL found in: ${OPENSSL_INCLUDE_DIR} / ${OPENSSL_LIBRARIES}")
else(OPENSSL_FOUND)
    message(FATAL_ERROR " ! OpenSSL not found TODO=${OPENSSL_INCLUDE_DIR}")
endif(OPENSSL_FOUND)

include_directories(${NSCP_INCLUDEDIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PROJECT_BINARY_DIR}/include)
include(${BUILD_CMAKE_FOLDER}/functions.cmake)
add_definitions(-DUNICODE -D_UNICODE -DBOOST_ALL_NO_LIB)

# ##############################################################################
#
# Parse version
#
# ##############################################################################
set(BUILD_VERSION "0.4.0" CACHE STRING "The version of NSClient++")
message(STATUS "BUILD_VERSION: ${BUILD_VERSION}")
if("${BUILD_VERSION}" STREQUAL "0.4.0")
  message(WARNING "Version not specified using incorrect version ${BUILD_VERSION}")
endif()

set(NSCP_LIB_VERSION "${BUILD_VERSION}")
set(NSCP_FULL_VERSION "${BUILD_VERSION}")

if("${BUILD_VERSION}" STREQUAL "")
  message(FATAL_ERROR "No version specified")
endif()
if(BUILD_VERSION MATCHES ".*-.*")
  # Parse 0.6.4-addedautomaticversionst2.8.70491e2f
  message(STATUS "Parsing branch version: ${BUILD_VERSION}")
  string(REGEX
          REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)-([^\.]*)\.([0-9*]*)\.([^\.]*)"
          "\\1.\\2.\\3" NSCP_VERSION_NUMBER "${BUILD_VERSION}")
  string(REGEX
          REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)-([^\.]*)\.([0-9*]*)\.([^\.]*)"
          "\\5" VERSION_BUILD "${BUILD_VERSION}")
else()
  # Parse 1.2.3
  set(NSCP_VERSION_NUMBER ${BUILD_VERSION})
  set(VERSION_BUILD 0)
endif()
message(STATUS "Version: ${BUILD_VERSION} decoded as ${NSCP_VERSION_NUMBER}.${VERSION_BUILD}")
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1" VERSION_SERIES ${NSCP_VERSION_NUMBER})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\2" VERSION_MAJOR ${NSCP_VERSION_NUMBER})
STRING(REGEX REPLACE ".*([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\3" VERSION_MINOR ${NSCP_VERSION_NUMBER})
message(STATUS "Version: ${NSCP_VERSION_NUMBER} decoded as ${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}")

string(REPLACE "." "," NSCP_VERSION_COMMA ${NSCP_VERSION_NUMBER})
string(TIMESTAMP NSCP_VERSION_DATE "%Y-%m-%d" UTC)
if(WIN32)
  set(VERSION_ARCH Win32)
  if(CMAKE_CL_64)
    set(VERSION_ARCH x64)
  endif(CMAKE_CL_64)
else(WIN32)
  set(VERSION_ARCH Unknown)
endif(WIN32)

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
set(CONFIG_DEFAULT_CONF_INI_LOCATION
        "${CONFIG_DEFAULT_CONF_INI_BASE}nsclient.ini"
        CACHE STRING "Default location for ini files")
set(CONFIG_DEFAULT_CONF_REG_LOCATION
        "registry://HKEY_LOCAL_MACHINE/software/NSClient++"
        CACHE STRING "Default location for registry based settings")
set(CONFIG_DEFAULT_CONF_OLD_LOCATION
        "old://\${exe-path}/nsc.ini"
        CACHE STRING "Default location for old config file")

configure_file(${NSCP_INCLUDEDIR}/config.h.in
               ${PROJECT_BINARY_DIR}/include/config.h @ONLY)
configure_file(${NSCP_INCLUDEDIR}/version.hpp.in
               ${PROJECT_BINARY_DIR}/include/version.hpp @ONLY)

include_directories(${OPENSSL_INCLUDE_DIR})
add_definitions(-DUSE_SSL)

set(SRCS
    ${TARGET}.cpp
    ${NSCP_INCLUDEDIR}/ServiceCmd.cpp
    ${NSCP_INCLUDEDIR}/error/error_w32.cpp
    ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
    ../libs/settings_manager/settings_manager_impl.cpp
    ../libs/settings_manager/settings_handler_impl.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    ${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
    ${NSCP_INCLUDEDIR}/utf8.cpp
    ${NSCP_INCLUDEDIR}/win/credentials.cpp
    ${NSCP_INCLUDEDIR}/win/credentials.hpp
    "${TARGET}.def"
    installer_helper.hpp
    ${NSCP_INCLUDEDIR}/ServiceCmd.h
    ${NSCP_ERROR_CPP}
    ${NSCP_ERROR_HPP}
    # Logger
    ${NSCP_INCLUDEDIR}/nsclient/logger/log_level.cpp
    ${NSCP_INCLUDEDIR}/nsclient/logger/base_logger_impl.cpp)

add_definitions(${NSCP_GLOBAL_DEFINES})

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(${TARGET} SHARED ${SRCS})

if(WIN32 AND NOT CMAKE_CL_64)
    openssl_link_fix(${TARGET})
endif(WIN32 AND NOT CMAKE_CL_64)

target_link_libraries(${TARGET} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${OPENSSL_LIBRARIES} CRYPT32 Msi.lib)
# SET_LIBRARY_OUT_FOLDER_MODULE(${TARGET})
