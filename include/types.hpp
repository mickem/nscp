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

//typedef wchar_t TCHAR;


#ifndef WIN32
typedef unsigned long DWORD;
typedef void* LPVOID;
typedef int BOOL;
#endif
#ifdef WIN32
typedef short int16_t;
#endif


#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define wcscasecmp _wcsicmp
#endif

#ifndef WIN32
#ifndef __FILEW__
#define WSTR(x) _T(x)
#define __FILEW__ WSTR(__FILE__)
#endif
#endif


#include <boost/detail/endian.hpp>

enum EEndian
{
	LITTLE_ENDIAN_ORDER,
	BIG_ENDIAN_ORDER,
#if defined(BOOST_LITTLE_ENDIAN)
	HOST_ENDIAN_ORDER = LITTLE_ENDIAN_ORDER
#elif defined(BOOST_BIG_ENDIAN)
	HOST_ENDIAN_ORDER = BIG_ENDIAN_ORDER
#else
#error "Impossible de determiner l'indianness du systeme cible."
#endif
};
