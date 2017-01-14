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
