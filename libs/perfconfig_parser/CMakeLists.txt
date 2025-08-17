cmake_minimum_required(VERSION 3.10)

set(TARGET perfconfig_parser)
add_definitions(${NSCP_GLOBAL_DEFINES})

set(SRCS ${NSCP_INCLUDEDIR}/parsers/perfconfig/perfconfig.cpp)
if(WIN32)
    set(SRCS ${SRCS} ${NSCP_INCLUDEDIR}/parsers/perfconfig/perfconfig.hpp)
endif(WIN32)

add_library(${TARGET} STATIC ${SRCS})
nscp_debug_symbols(${TARGET})

set_target_properties(${TARGET} PROPERTIES FOLDER "libraries")
if(CMAKE_COMPILER_IS_GNUCXX)
    if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
        set_target_properties(${TARGET} PROPERTIES COMPILE_FLAGS -fPIC)
    endif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
endif(CMAKE_COMPILER_IS_GNUCXX)

if(GTEST_FOUND)
    include_directories(${GTEST_INCLUDE_DIR})
    set(SRCS perfconfig_test.cpp)
    nscp_make_exe_test(${TARGET}_test ${SRCS})
    nscp_add_test(${TARGET}_test ${TARGET}_test)
    target_link_libraries(
        ${TARGET}_test
        ${GTEST_GTEST_LIBRARY}
        ${GTEST_GTEST_MAIN_LIBRARY}
        ${TARGET}
    )
endif(GTEST_FOUND)
