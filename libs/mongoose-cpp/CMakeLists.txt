cmake_minimum_required (VERSION 2.6)
project (mongoose)

set (SOURCES
    mongoose.c
    )

set (MONGOOSE_CPP "${PROJECT_SOURCE_DIR}/mongoose")

include_directories ("${PROJECT_SOURCE_DIR}")

if (OPENSSL_FOUND)
    include_directories("${OPENSSL_INCLUDE_DIR}")
    add_definitions("-DNS_ENABLE_SSL")
    add_definitions("-DMG_ENABLE_SSL")
endif (OPENSSL_FOUND)

set (SOURCES
    ${SOURCES}

    ${NSCP_INCLUDEDIR}/has-threads.cpp

    ${MONGOOSE_CPP}/Controller.cpp
    ${MONGOOSE_CPP}/Request.cpp
    ${MONGOOSE_CPP}/Response.cpp
    ${MONGOOSE_CPP}/Server.cpp
    ${MONGOOSE_CPP}/StreamResponse.cpp
    )

add_definitions(-DMG_ENABLE_HTTP_WEBSOCKET=0)
add_definitions(-DMG_ENABLE_BROADCAST)
#add_definitions(-DMG_ENABLE_THREADS=0)
add_definitions(-DMG_ENABLE_SSL=1)
add_definitions(${NSCP_GLOBAL_DEFINES})

# Adding dl
if (NOT WIN32)
    set (EXTRA_LIBS ${EXTRA_LIBS} dl)
endif (NOT WIN32)

# Adding sockets for Win32
if (WIN32)
    set (EXTRA_LIBS ${EXTRA_LIBS} ws2_32)
endif (WIN32)

# Compiling library
ADD_LIBRARY(_mongoose STATIC ${SOURCES})
SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "libraries")

target_link_libraries (_mongoose 
  ${EXTRA_LIBS} 
  ${Boost_THREAD_LIBRARY}
  )


