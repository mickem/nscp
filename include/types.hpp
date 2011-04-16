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
