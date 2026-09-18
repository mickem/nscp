cmake_minimum_required(VERSION 3.10)

# The client-side command line / target handling shared by every module that
# can act as a client (the NRPE, NSCA, Graphite, Syslog, ... senders) and by the
# check_nscp / check_nrpe executables. Twelve targets compiled
# command_line_parser.cpp privately at ~580 KB of object code each.
#
# simple_client.cpp deliberately stays out: it logs through the NSC_LOG_* macros,
# which expand to GET_CORE() -> nscapi::plugin_singleton, the global each module
# defines for itself in its generated module.cpp. A shared library cannot resolve
# it (the Windows link would fail outright) and must not try to.

set(TARGET nscp_client)

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})
add_definitions(${NSCP_GLOBAL_DEFINES})

set(SRCS ${NSCP_INCLUDEDIR}/client/command_line_parser.cpp)

if(WIN32)
    set(SRCS
        ${SRCS}
        ${NSCP_INCLUDEDIR}/client/command_line_parser.hpp
    )
endif(WIN32)

NSCP_MAKE_LIBRARY(${TARGET} "${SRCS}" EXPORT_ALL_SYMBOLS)

target_link_libraries(
    ${TARGET}
    PUBLIC
        nscp_protobuf
        ${PLUGIN_API_TARGET}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        # boost::this_thread::interruption_point, reached through the
        # filesystem/date_time helpers the parser uses; not header-only.
        ${Boost_THREAD_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
)

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "libraries"
)
