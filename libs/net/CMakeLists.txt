cmake_minimum_required(VERSION 3.10)

# Socket plumbing shared by every network-facing module and client.
#
# socket_helpers.cpp is 1100 lines of ordinary (non-template) code - connection
# strings, TLS context setup, certificate subject formatting - that twenty-odd
# targets used to compile privately, at ~520 KB of object code each.
#
# USE_SSL is PRIVATE, i.e. each consumer keeps deciding for itself exactly as it
# did when it compiled socket_helpers.cpp privately. Every `#ifdef USE_SSL` in
# socket_helpers.hpp guards an include, a free function or a member function -
# never a data member - so connection_info and ssl_opts have the same layout
# either way and the two settings can be mixed safely. Making it PUBLIC is what
# an earlier revision of this file did, on the mistaken belief that the layout
# changed; it dragged asio-ssl and OpenSSL into CollectdClient and SyslogClient,
# which need neither, and cost over a megabyte in the static (XP) build where
# OpenSSL is linked in rather than loaded.

# The public surface is annotated with NSCP_NET_EXPORT (net/dll_defines.hpp)
# rather than exported wholesale with WINDOWS_EXPORT_ALL_SYMBOLS, for the reason
# libs/client/CMakeLists.txt gives: export-all turns every symbol into a root the
# linker may not discard, so /OPT:REF stops pruning. That matters more here than
# anywhere else, because the unreachable code it pins down is a statically linked
# OpenSSL.

set(TARGET nscp_net)

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})
add_definitions(${NSCP_GLOBAL_DEFINES})

set(SRCS
    ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.cpp
    # Compiled in rather than imported: utf8 is deliberately not part of
    # plugin_api's exported surface (see NSCP_DEF_PLUGIN_CPP in the top-level
    # CMakeLists), and on Windows a DLL must resolve all of its symbols.
    ${NSCP_INCLUDEDIR}/str/utf8.cpp
)

if(WIN32)
    set(SRCS
        ${SRCS}
        ${NSCP_INCLUDEDIR}/net/socket/socket_helpers.hpp
        ${NSCP_INCLUDEDIR}/net/socket/allowed_hosts.hpp
    )
endif(WIN32)

NSCP_MAKE_LIBRARY(${TARGET} "${SRCS}")

# No plugin_api here: neither socket_helpers.cpp nor allowed_hosts.cpp includes
# an nscapi header. It was listed while utf8 came from plugin_api; utf8.cpp is
# compiled in above now, so the dependency is gone. Keeping it would hand every
# consumer a load-time DLL it may not want - the service compiles its own
# settings helper under plugin_api_NOLIB and had no such dependency before.
target_link_libraries(
    ${TARGET}
    PUBLIC
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
)

if(OPENSSL_FOUND)
    target_compile_definitions(${TARGET} PRIVATE USE_SSL)
    target_include_directories(${TARGET} PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(${TARGET} PRIVATE ${OPENSSL_LIBRARIES})
    if(WIN32)
        target_link_libraries(${TARGET} PRIVATE CRYPT32)
    endif(WIN32)
endif(OPENSSL_FOUND)

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "libraries"
)
