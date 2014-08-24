#pragma once

// We are using the Visual Studio Compiler and building Shared libraries
#if defined (_WIN32) 
  #if defined(where_filter_NOLIB)
    #define NSCAPI_EXPORT
  #else
    #if defined(where_filter_EXPORTS)
      #define NSCAPI_EXPORT __declspec(dllexport)
    #else
      #define NSCAPI_EXPORT __declspec(dllimport)
    #endif /* where_filter_EXPORTS */
  #endif /* where_filter_NOLIB */
#else /* defined (_WIN32) */
 #define NSCAPI_EXPORT
#endif
