cmake_minimum_required(VERSION 3.10)

# Socket plumbing shared by every network-facing module and client.
#
# socket_helpers.cpp is 1100 lines of ordinary (non-template) code - connection
# strings, TLS context setup, certificate subject formatting - that twenty-odd
# targets used to compile privately, at ~520 KB of object code each.
#
# USE_SSL is PUBLIC and set once here rather than per consumer. It is not just a
# feature switch: it changes the layout of socket_helpers::connection_info, so
# a consumer that compiled the header without it while the library was built
# with it would have a silent ODR violation. Before this library existed each
# module carried its own copy, which hid the mismatch - CollectdClient and
# SyslogClient compiled socket_helpers.cpp with SSL off while everyone else
# compiled it with SSL on.

set(TARGET nscp_net)

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})
add_definitions(${NSCP_GLOBAL_DEFINES})

set(SRCS
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.cpp
)

if(WIN32)
    set(SRCS
        ${SRCS}
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.hpp
    )
endif(WIN32)

NSCP_MAKE_LIBRARY(${TARGET} "${SRCS}")

target_link_libraries(
    ${TARGET}
    PUBLIC
        # utf8::to_unicode / utf8::wstring_to_string live in plugin_api; on
        # Windows a DLL must resolve all of its symbols at link time.
        ${PLUGIN_API_TARGET}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
)

if(OPENSSL_FOUND)
    target_compile_definitions(${TARGET} PUBLIC USE_SSL)
    target_include_directories(${TARGET} PUBLIC ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(${TARGET} PUBLIC ${OPENSSL_LIBRARIES})
    if(WIN32)
        target_link_libraries(${TARGET} PUBLIC CRYPT32)
    endif(WIN32)
endif(OPENSSL_FOUND)

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "libraries"
)
