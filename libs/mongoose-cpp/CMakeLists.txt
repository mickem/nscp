cmake_minimum_required (VERSION 2.6)
project (lib-mongoose)

set (SOURCES
    Controller.cpp
    Request.cpp
    Response.cpp
    Server.cpp
    StreamResponse.cpp
	
    ${NSCP_INCLUDEDIR}/has-threads.cpp

    ext/mongoose.c
	
    )

IF(USE_STATIC_RUNTIME)
	ADD_DEFINITIONS(-Dlib_mongoose_NOLIB)
ELSE(USE_STATIC_RUNTIME)
	ADD_DEFINITIONS(-Dlib_mongoose_EXPORTS)
ENDIF(USE_STATIC_RUNTIME)

add_definitions(-DMG_ENABLE_HTTP_WEBSOCKET=0)
add_definitions(-DMG_ENABLE_BROADCAST=1)
add_definitions(${NSCP_GLOBAL_DEFINES})

if (OPENSSL_FOUND)
    include_directories("${OPENSSL_INCLUDE_DIR}")
    add_definitions(-DMG_ENABLE_SSL=1)
	add_definitions(-DMG_DISABLE_PFS=1)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
endif (OPENSSL_FOUND)

# Adding dl
if (NOT WIN32)
    set (EXTRA_LIBS ${EXTRA_LIBS} dl)
endif (NOT WIN32)

# Adding sockets for Win32
if (WIN32)
    set (EXTRA_LIBS ${EXTRA_LIBS} ws2_32)
endif (WIN32)

# Compiling library
NSCP_MAKE_LIBRARY(lib-mongoose "${SOURCES}")
#SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "libraries")

target_link_libraries(lib-mongoose 
  ${EXTRA_LIBS} 
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  )


