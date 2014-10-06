#pragma once

// We are using the Visual Studio Compiler and building Shared libraries

#if defined (_WIN32) 
  #if defined(plugin_api_NOLIB)
    #define NSCAPI_EXPORT
  #else
    #if defined(plugin_api_EXPORTS)
      #define NSCAPI_EXPORT __declspec(dllexport)
    #else
      #define NSCAPI_EXPORT __declspec(dllimport)
    #endif /* plugin_api_EXPORTS */
  #endif /* plugin_api_NOLIB */
#else /* defined (_WIN32) */
  #if defined(plugin_api_NOLIB)
    #define NSCAPI_EXPORT
  #else
    #if defined(plugin_api_EXPORTS)
//      #define NSCAPI_EXPORT __attribute__ ((visibility("default")))
      #define NSCAPI_EXPORT
    #else
      #define NSCAPI_EXPORT
    #endif /* plugin_api_EXPORTS */
  #endif /* plugin_api_NOLIB */
#endif
