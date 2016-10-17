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

//typedef wchar_t TCHAR;


#ifndef WIN32
typedef unsigned long DWORD;
typedef void* LPVOID;
typedef int BOOL;
#endif
#ifdef WIN32
typedef short int16_t;
typedef unsigned long u_int32_t;
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
