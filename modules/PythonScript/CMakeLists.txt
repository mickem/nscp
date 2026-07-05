cmake_minimum_required(VERSION 3.10)

set(TARGET PythonScript)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

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

add_definitions(
    ${NSCP_GLOBAL_DEFINES}
    -DBOOST_PYTHON_STATIC_LIB
)

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
    # Derive the runtime python library name (the default of the "python lib"
    # setting) from the Boost.Python component name, e.g. python314 ->
    # libpython3.14.so.1.0, so new python releases work without touching this
    # list. Use the soname: the bare .so symlink only exists on systems with
    # the python development package installed, while the soname is shipped
    # with the runtime library the module already links against.
    if(NSCP_BOOST_PYTHON_VERSION MATCHES "^python3([0-9]+)$")
        add_definitions(
            ${NSCP_GLOBAL_DEFINES}
            -DDEFAULT_PYTHON_LIB="libpython3.${CMAKE_MATCH_1}.so.1.0"
        )
    else()
        message(
            FATAL_ERROR
            "Unknown python version: ${NSCP_BOOST_PYTHON_VERSION}"
        )
    endif()
endif()
include_directories(${Python3_INCLUDE_DIRS})

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

# Boost.Python's instance_dealloc destroys a holder and THEN dynamic_casts it
# (post-destruction UB that is benign in practice). With a non-recoverable
# vptr sanitizer (-fsanitize=undefined -fno-sanitize-recover=all) GCC plants
# `vptr = nullptr` at the end of every complete destructor, so that
# dynamic_cast reads offset-to-top through a null vtable and SEGVs at
# 0xfffffffffffffff0 whenever any wrapped object is destroyed (e.g. a script's
# shutdown() dropping Settings/Registry/Core). The UB lives in libboost_python,
# which we cannot rebuild, so exempt this module from the vptr check.
if(
        NOT NSCP_SANITIZE
        STREQUAL
        "off"
        AND NSCP_SANITIZE
        MATCHES
        "undefined"
)
    target_compile_options(${TARGET} PRIVATE -fno-sanitize=vptr)
endif()

string(TOUPPER ${NSCP_BOOST_PYTHON_VERSION} BOOST_PYTHON_VERSION_UPPER)
set(BOOST_PYTHON_LIBRARY ${Boost_${BOOST_PYTHON_VERSION_UPPER}_LIBRARY})

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

# Test for the parse-strip-restamp helper used by command_wrapper::query.
# The helper only depends on protobuf - no Python, no Boost.Python - so
# the test target is independent of the module build.
NSCP_CREATE_TEST(
    PythonScript_test
    SOURCES
        script_identity_test.cpp
        script_identity.hpp
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
