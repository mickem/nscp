/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

// We are using the Visual Studio Compiler and building Shared libraries

#if defined (_WIN32) 
  #if defined(lib_mongoose_NOLIB)
    #define NSCAPI_EXPORT
  #else
    #if defined(lib_mongoose_EXPORTS)
      #define NSCAPI_EXPORT __declspec(dllexport)
    #else
      #define NSCAPI_EXPORT __declspec(dllimport)
    #endif /* lib_mongoose_EXPORTS */
  #endif /* lib_mongoose_NOLIB */
#else /* defined (_WIN32) */
  #if defined(lib_mongoose_NOLIB)
    #define NSCAPI_EXPORT
  #else
    #if defined(lib_mongoose_EXPORTS)
//      #define NSCAPI_EXPORT __attribute__ ((visibility("default")))
      #define NSCAPI_EXPORT
    #else
      #define NSCAPI_EXPORT
    #endif /* lib_mongoose_EXPORTS */
  #endif /* lib_mongoose_NOLIB */
#endif
