cmake_minimum_required(VERSION 3.5)
set(TARGET installer_lib)
project(${TARGET} CXX)

set(NSCP_INCLUDEDIR ${CMAKE_SOURCE_DIR}/../include)
set(BUILD_PYTHON_FOLDER "${CMAKE_SOURCE_DIR}/../build/python")

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost COMPONENTS system filesystem)
find_package(Python3 COMPONENTS Interpreter)

include_directories(${NSCP_INCLUDEDIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PROJECT_BINARY_DIR}/include)
add_definitions(-DUNICODE -D_UNICODE -DBOOST_ALL_NO_LIB)

if(NOT NSCP_VERSION)
  set(NSCP_VERSION "0.0.0")
  message(WARNING "Version not specified, building ${NSCP_VERSION}")
endif(NOT NSCP_VERSION)

set(NSCP_LIB_VERSION "${NSCP_VERSION}")
set(NSCP_FULL_VERSION "${NSCP_VERSION}")
string(REPLACE "." "," NSCP_VERSION_COMMA ${NSCP_VERSION})
string(TIMESTAMP NSCP_VERSION_DATE "%Y-%m-%d" UTC)
configure_file(${NSCP_INCLUDEDIR}/config.h.in
               ${PROJECT_BINARY_DIR}/include/config.h @ONLY)
configure_file(${NSCP_INCLUDEDIR}/version.hpp.in
               ${PROJECT_BINARY_DIR}/include/version.hpp @ONLY)

set(SRCS
    ${TARGET}.cpp
    ${NSCP_INCLUDEDIR}/ServiceCmd.cpp
    ${NSCP_INCLUDEDIR}/error/error_w32.cpp
    ../libs/settings_manager/settings_manager_impl.cpp
    ../libs/settings_manager/settings_handler_impl.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    ${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
    ${NSCP_INCLUDEDIR}/utf8.cpp
    "${TARGET}.def"
    installer_helper.hpp
    ${NSCP_INCLUDEDIR}/ServiceCmd.h
    ${NSCP_ERROR_CPP}
    ${NSCP_ERROR_HPP}
    # Logger
    ${NSCP_INCLUDEDIR}/nsclient/logger/log_level.cpp
    ${NSCP_INCLUDEDIR}/nsclient/logger/base_logger_impl.cpp)

add_definitions(${NSCP_GLOBAL_DEFINES})

add_library(${TARGET} SHARED ${SRCS})
message(STATUS "Boost_FILESYSTEM_LIBRARY: ${Boost_FILESYSTEM_LIBRARY}")
target_link_libraries(${TARGET} ${Boost_FILESYSTEM_LIBRARY}
                      ${Boost_SYSTEM_LIBRARY} Msi.lib)
# SET_LIBRARY_OUT_FOLDER_MODULE(${TARGET})
