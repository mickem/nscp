cmake_minimum_required(VERSION 3.10)

if(LUA_FOUND OR LUA_SOURCE_FOUND)
    set(TARGET lua_nscp)

    set(SRCS
        lua_core.cpp
        lua_cpp.cpp
        lua_script.cpp
        ${NSCP_INCLUDEDIR}/scripts/script_nscp.cpp
    )

    if(WIN32)
        set(SRCS
            ${SRCS}
            ${NSCP_INCLUDEDIR}/lua/lua_core.hpp
            ${NSCP_INCLUDEDIR}/lua/lua_cpp.hpp
            ${NSCP_INCLUDEDIR}/lua/lua_script.hpp
            ${NSCP_INCLUDEDIR}/scripts/script_interface.hpp
            ${NSCP_INCLUDEDIR}/scripts/script_nscp.hpp
        )
    endif(WIN32)
    include_directories(${LUA_INCLUDE_DIR})

    if(PROTOC_GEN_LUA_FOUND)
        add_definitions(-DHAVE_LUA_PB)
        include_directories(${NSCP_PROJECT_BINARY_DIR}/libs/lua_pb)
        include_directories(${NSCP_PROJECT_BINARY_DIR}/libs/protobuf)
    endif()

    add_definitions(${NSCP_GLOBAL_DEFINES})

    add_library(${TARGET} STATIC ${SRCS})
    NSCP_DEBUG_SYMBOLS(${TARGET})

    if(PROTOC_GEN_LUA_FOUND)
        target_link_libraries(
            ${TARGET}
            nscp_lua_pb
            ${Boost_THREAD_LIBRARY}
            ${LUA_LIBRARIES}
        )
        add_dependencies(${TARGET} nscp_lua_pb)
    else()
        target_link_libraries(${TARGET} ${LUA_LIBRARIES})
    endif(PROTOC_GEN_LUA_FOUND)
    add_dependencies(${TARGET} nscp_protobuf)

    set_target_properties(
        ${TARGET}
        PROPERTIES
            FOLDER
                "libraries"
    )

    if(CMAKE_COMPILER_IS_GNUCXX)
        if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
            set_target_properties(
                ${TARGET}
                PROPERTIES
                    COMPILE_FLAGS
                        -fPIC
            )
        endif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
    endif(CMAKE_COMPILER_IS_GNUCXX)
endif()
