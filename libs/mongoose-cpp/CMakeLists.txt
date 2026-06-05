cmake_minimum_required(VERSION 3.10)
project(nscp_mongoose)

# Only build the library when the selected web backend's dependencies are
# satisfied (see NSCP_MONGOOSE_AVAILABLE in build/cmake/dependencies.cmake).
# When they are not, skip the target entirely; the WEBServer module raises a
# targeted error if it is asked to build without it.
if(NOT NSCP_MONGOOSE_AVAILABLE)
    message(
        STATUS
        "Skipping nscp_mongoose: NSCP_WEB_BACKEND='${NSCP_WEB_BACKEND}' "
        "dependencies are not satisfied."
    )
    return()
endif()

# Backend-independent sources.
set(SOURCES
    Controller.cpp
    Helpers.cpp
    MatchController.cpp
    RegexController.cpp
    Request.cpp
    Response.cpp
    Server.cpp
    StreamResponse.cpp
    cert_loader.cpp
    string_response.cpp
    ${NSCP_INCLUDEDIR}/bytes/base64.c
)

# Pick exactly one HTTP backend. ServerBeastImpl.cpp uses
# `boost::asio::spawn`, which pulls in Boost.Coroutine + Boost.Context —
# so it only gets compiled (and those Boost components only get linked)
# when beast is the selected backend. Mongoose-only builds (typically
# Windows today) thus avoid the extra Boost components entirely.
if(NSCP_WEB_BACKEND STREQUAL "beast")
    list(APPEND SOURCES ServerBeastImpl.cpp)
    add_definitions(-DNSCP_WEB_BACKEND_BEAST)
elseif(NSCP_WEB_BACKEND STREQUAL "mongoose")
    list(APPEND SOURCES ServerMongooseImpl.cpp ${MONGOOSE_INCLUDE_DIR}/mongoose.c)
else()
    message(FATAL_ERROR "Unknown NSCP_WEB_BACKEND='${NSCP_WEB_BACKEND}' (expected mongoose | beast)")
endif()

if(WIN32)
    set(WIN_HEADERS
        Controller.h
        Helpers.h
        MatchController.h
        RegexController.h
        Request.h
        Response.h
        Server.h
        StreamResponse.h
        cert_loader.h
        RegexRequestHandler.h
        RequestHandler.h
        dll_defines.hpp
        mcp_exception.hpp
        string_response.hpp
        ${NSCP_INCLUDEDIR}/threads/scoped_thread_group.hpp
    )
    if(NSCP_WEB_BACKEND STREQUAL "beast")
        list(APPEND WIN_HEADERS ServerBeastImpl.h)
    else()
        list(APPEND WIN_HEADERS ServerMongooseImpl.h ${MONGOOSE_INCLUDE_DIR}/mongoose.h)
    endif()
    set(SOURCES ${SOURCES} ${WIN_HEADERS})
endif()

if(USE_STATIC_RUNTIME)
    add_definitions(-Dlib_mongoose_NOLIB)
else(USE_STATIC_RUNTIME)
    add_definitions(-Dlib_mongoose_EXPORTS)
endif(USE_STATIC_RUNTIME)

add_definitions(${NSCP_GLOBAL_DEFINES})

# Mongoose-only build knobs: defines + include dir + MSVC warning silencing
# for the vendored mongoose.c.
if(NSCP_WEB_BACKEND STREQUAL "mongoose")
    add_definitions(-DMG_ENABLE_HTTP_WEBSOCKET=0)
    add_definitions(-DMG_ENABLE_BROADCAST=1)
    add_definitions(-DMG_ENABLE_THREADS)
    # mongoose.h pulls in <openssl/*.h> when MG_TLS == MG_TLS_OPENSSL, so only
    # select the OpenSSL TLS backend when OpenSSL is actually available;
    # otherwise build mongoose without TLS (HTTPS unavailable, plain HTTP works).
    if(OPENSSL_FOUND)
        add_definitions(-DMG_TLS=MG_TLS_OPENSSL)
    else()
        add_definitions(-DMG_TLS=MG_TLS_NONE)
    endif()
    include_directories("${MONGOOSE_INCLUDE_DIR}")
    if(MSVC)
        set_source_files_properties(
            ${MONGOOSE_INCLUDE_DIR}/mongoose.c
            PROPERTIES
                COMPILE_OPTIONS
                    "/wd4201;/wd4204;/wd4267;/wd4310"
        )
    endif()
endif()

if(OPENSSL_FOUND)
    include_directories("${OPENSSL_INCLUDE_DIR}")
    if(NSCP_WEB_BACKEND STREQUAL "mongoose")
        add_definitions(-DMG_ENABLE_OPENSSL=1)
        add_definitions(-DMG_DISABLE_PFS=1)
    endif()
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        ${OPENSSL_LIBRARIES}
    )
    if(WIN32)
        set(EXTRA_LIBS
            ${EXTRA_LIBS}
            CRYPT32
        )
    endif(WIN32)
endif(OPENSSL_FOUND)

# Adding dl
if(NOT WIN32)
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        dl
    )
endif(NOT WIN32)

# Adding sockets for Win32
if(WIN32)
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        ws2_32
    )
endif(WIN32)

# Compiling library
NSCP_MAKE_LIBRARY(nscp_mongoose "${SOURCES}")
# SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "libraries")

# Beast needs Boost.Coroutine + Boost.Context (transitive of
# `boost::asio::spawn`). Only link them in the beast build — the
# variables are empty otherwise because dependencies.cmake doesn't
# request the components.
set(_nscp_mongoose_backend_libs)
if(NSCP_WEB_BACKEND STREQUAL "beast")
    list(APPEND _nscp_mongoose_backend_libs ${Boost_COROUTINE_LIBRARY} ${Boost_CONTEXT_LIBRARY})
endif()

target_link_libraries(
    nscp_mongoose
    ${EXTRA_LIBS}
    ${Boost_THREAD_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${_nscp_mongoose_backend_libs}
)
