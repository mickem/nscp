#pragma once

/* Cmake will define MyLibrary_EXPORTS on Windows when it
configures to build a shared library. If you are going to use
another build system on windows or create the visual studio
projects by hand you need to define MyLibrary_EXPORTS when
building a DLL on windows.
*/
// We are using the Visual Studio Compiler and building Shared libraries

#if defined (WIN32) 
  #if defined(nscp_protobuf_NOLIB)
    #define NSCAPI_PROTOBUF_EXPORT
  #else
    #if defined(nscp_protobuf_EXPORTS)
      #define NSCAPI_PROTOBUF_EXPORT __declspec(dllexport)
    #else
      #define NSCAPI_PROTOBUF_EXPORT __declspec(dllimport)
    #endif /* protobuf_EXPORTS */
  #endif /* protobuf_NOLIB */
#else /* defined (_WIN32) */
 #define NSCAPI_PROTOBUF_EXPORT
#endif
