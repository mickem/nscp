cmake_minimum_required(VERSION 3.10)

set(TARGET PythonScript)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    python_script.cpp
    script_wrapper.cpp
    extscr_cli.cpp
    script_provider.cpp
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES} -DBOOST_PYTHON_STATIC_LIB)

if(WIN32)
    set(SRCS
        ${SRCS}
        script_wrapper.hpp
        "${TARGET}.h"
        extscr_cli.h
        script_interface.hpp
        python_script.hpp
        script_provider.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
else()
    if(NSCP_BOOST_PYTHON_VERSION STREQUAL "python313")
        add_definitions(${NSCP_GLOBAL_DEFINES} -DDEFAULT_PYTHON_LIB="libpython3.13.so")
    elseif(NSCP_BOOST_PYTHON_VERSION STREQUAL "python312")
        add_definitions(${NSCP_GLOBAL_DEFINES} -DDEFAULT_PYTHON_LIB="libpython3.12.so")
    elseif(NSCP_BOOST_PYTHON_VERSION STREQUAL "python311")
        add_definitions(${NSCP_GLOBAL_DEFINES} -DDEFAULT_PYTHON_LIB="libpython3.11.so")
    else()
        message(FATAL_ERROR "Unknown python version: ${NSCP_BOOST_PYTHON_VERSION}")
    endif()
endif()
include_directories(${Python3_INCLUDE_DIRS})

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

if(WIN32)
    set(BOOST_PYTHON_LIBRARY ${Boost_PYTHON311_LIBRARY})
else(WIN32)
    string(TOUPPER ${NSCP_BOOST_PYTHON_VERSION} BOOST_PYTHON_VERSION_UPPER)
    set(BOOST_PYTHON_LIBRARY ${Boost_${BOOST_PYTHON_VERSION_UPPER}_LIBRARY})
endif(WIN32)

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${BOOST_PYTHON_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${Python3_LIBRARIES}
)


include(${BUILD_CMAKE_FOLDER}/module.cmake)
