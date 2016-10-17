/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
