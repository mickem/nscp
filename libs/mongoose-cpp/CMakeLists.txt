cmake_minimum_required (VERSION 2.6)
project (nscp_mongoose)

set (SOURCES
    Controller.cpp
    Helpers.cpp
    MatchController.cpp
    RegexController.cpp
    Request.cpp
    Response.cpp
    Server.cpp
    ServerImpl.cpp
    Client.cpp
    StreamResponse.cpp
    
    string_response.cpp
	
    ${NSCP_INCLUDEDIR}/has-threads.cpp

    ext/mongoose.c
	
    )
if(WIN32)
	set (SOURCES 
		${SOURCES}
		Controller.h
		Helpers.h
		MatchController.h
		RegexController.h
		Request.h
		Response.h
		Server.h
		ServerImpl.h
		Client.hpp
		StreamResponse.h
		RegexRequestHandler.h
		RequestHandler.h

		dll_defines.hpp
		mcp_exception.hpp
		string_response.hpp	

		${NSCP_INCLUDEDIR}/has-threads.hpp

		ext/mongoose.h
    )
endif()

IF(USE_STATIC_RUNTIME)
	ADD_DEFINITIONS(-Dlib_mongoose_NOLIB)
ELSE(USE_STATIC_RUNTIME)
	ADD_DEFINITIONS(-Dlib_mongoose_EXPORTS)
ENDIF(USE_STATIC_RUNTIME)

add_definitions(-DMG_ENABLE_HTTP_WEBSOCKET=0)
add_definitions(-DMG_ENABLE_BROADCAST=1)
add_definitions(-DMG_ENABLE_THREADS)
add_definitions(${NSCP_GLOBAL_DEFINES})

if (OPENSSL_FOUND)
    include_directories("${OPENSSL_INCLUDE_DIR}")
    add_definitions(-DMG_ENABLE_OPENSSL=1)
	add_definitions(-DMG_DISABLE_PFS=1)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
	IF(WIN32)
		SET(EXTRA_LIBS ${EXTRA_LIBS} CRYPT32)
	ENDIF(WIN32)
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
NSCP_MAKE_LIBRARY(nscp_mongoose "${SOURCES}")
#SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "libraries")

target_link_libraries(nscp_mongoose 
  ${EXTRA_LIBS} 
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  )


