# SET_MULTITHREAD Macro Sets multithread switches for Visual Studio and GNU G++
# compilers Uses static runtime library on Windows Can be used multiple times,
# but overrides any switches added before the first call The /RuntimeLibrary:MT
# SET switch could be an option, if future CMake versions support it

macro(SET_MULTITHREAD)
    if(NOT MT_SET)
        set(MT_SET 1)
        if(USE_STATIC_RUNTIME)
            set(RUNTIME "MT")
        else(USE_STATIC_RUNTIME)
            set(RUNTIME "MD")
        endif(USE_STATIC_RUNTIME)

        # Threads compatibility
        if(MSVC)
            message(STATUS "Setting MSVC MT switches")
            set(CMAKE_CXX_FLAGS_DEBUG
                "/D_DEBUG /${RUNTIME}d /Zi  /Ob0 /Od /RTC1"
                CACHE STRING
                "MSVC MT flags "
                FORCE
            )
            set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})

            set(CMAKE_CXX_FLAGS_RELEASE
                "/${RUNTIME} /O2 /Ob2 /D NDEBUG"
                CACHE STRING
                "MSVC MT flags "
                FORCE
            )
            set(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})

            set(CMAKE_CXX_FLAGS_MINSIZEREL
                "/${RUNTIME} /O1 /Ob1 /D NDEBUG"
                CACHE STRING
                "MSVC MT flags "
                FORCE
            )
            set(CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_MINSIZEREL})

            set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
                "/${RUNTIME} /Zi /O2 /Ob1 /D NDEBUG"
                CACHE STRING
                "MSVC MT flags "
                FORCE
            )
            set(CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})

            # Maybe future CMake versions will implement this SET (
            # CMAKE_CXX_FLAGS_RELEASE "/RuntimeLibrary:MT ${CMAKE_CXX_FLAGS_RELEASE}"
            # )
        endif(MSVC)

        if(CMAKE_COMPILER_IS_GNUCXX)
            message(STATUS "Setting GCC MT switches")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
        endif(CMAKE_COMPILER_IS_GNUCXX)
    endif(NOT MT_SET)
endmacro(SET_MULTITHREAD)
