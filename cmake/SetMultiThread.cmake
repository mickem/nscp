# SET_MULTITHREAD Macro
# Sets multithread switches for Visual Studio and GNU G++ compilers
# Uses static runtime library on Windows
# Can be used multiple times, but overrides any switches added before the first call
# The /RuntimeLibrary:MT SET switch could be an option, if future CMake versions support it

MACRO ( SET_MULTITHREAD )

  IF ( NOT MT_SET )
  	
  	SET ( MT_SET 1 )
  	
    # Threads compatibility
    IF ( MSVC )
      MESSAGE ( STATUS "Setting MSVC MT switches")
      SET (
        CMAKE_CXX_FLAGS_DEBUG
          "/D_DEBUG /MTd /Zi  /Ob0 /Od /GZ"
          CACHE STRING "MSVC MT flags " FORCE
      )
  
      SET (
        CMAKE_CXX_FLAGS_RELEASE
          "/MT /O2 /Ob2 /D NDEBUG"
          CACHE STRING "MSVC MT flags " FORCE
      )
  
      SET (
        CMAKE_CXX_FLAGS_MINSIZEREL
          "/MT /O1 /Ob1 /D NDEBUG"
          CACHE STRING "MSVC MT flags " FORCE
      )
  
      SET (
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
          "/MT /Zi /O2 /Ob1 /D NDEBUG"
          CACHE STRING "MSVC MT flags " FORCE
      )
      
      # Maybe future CMake versions will implement this
      #SET (
      #  CMAKE_CXX_FLAGS_RELEASE
      #    "/RuntimeLibrary:MT ${CMAKE_CXX_FLAGS_RELEASE}"
      #)
    ENDIF ( MSVC )
    
    IF ( CMAKE_COMPILER_IS_GNUCXX )
      MESSAGE ( STATUS "Setting GCC MT switches" )
      SET (
        CMAKE_CXX_FLAGS
          "${CMAKE_CXX_FLAGS} -pthread"
      )
    ENDIF ( CMAKE_COMPILER_IS_GNUCXX )
  
  ENDIF ( NOT MT_SET )

ENDMACRO ( SET_MULTITHREAD )
