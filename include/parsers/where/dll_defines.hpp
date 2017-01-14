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
