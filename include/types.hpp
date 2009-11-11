#pragma once


#ifndef WIN32
typedef unsigned long DWORD;
typedef void* LPVOID;
#endif


#ifdef WIN32
#define wcscasecmp _wcsicmp
#endif

#ifndef __FILEW__
#define WSTR(x) _T(x)
#define __FILEW__ WSTR(__FILE__)
#endif
