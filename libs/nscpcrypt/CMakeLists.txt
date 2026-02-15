cmake_minimum_required(VERSION 3.10)

list(APPEND cryptopp_SOURCES nscpcrypt.cpp)
if(WIN32)
    list(APPEND cryptopp_SOURCES ${NSCP_INCLUDEDIR}/nscpcrypt/nscpcrypt.hpp)
endif(WIN32)

if(CRYPTOPP_FOUND)
    add_definitions(-DHAVE_LIBCRYPTOPP)
    include_directories(${CRYPTOPP_INCLUDE_DIR})
else(CRYPTOPP_FOUND)
    message(STATUS "   - Crypto++ not found (no encryption for NSCA)")
endif(CRYPTOPP_FOUND)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100 /wd4244 /wd4127 /wd4512")
endif(MSVC)

add_library(nscpcrypt STATIC ${cryptopp_SOURCES})
set_target_properties(
    nscpcrypt
    PROPERTIES
        FOLDER
            "libraries"
)

if(CRYPTOPP_FOUND)
    target_link_libraries(nscpcrypt ${CRYPTOPP_LIBRARIES})
endif(CRYPTOPP_FOUND)

if(WIN32)
    target_link_libraries(nscpcrypt Ws2_32.lib)
endif(WIN32)

if(CMAKE_COMPILER_IS_GNUCXX)
    if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
        set_target_properties(
            nscpcrypt
            PROPERTIES
                COMPILE_FLAGS
                    -fPIC
        )
    endif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
endif(CMAKE_COMPILER_IS_GNUCXX)
