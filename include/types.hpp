#pragma once

typedef wchar_t TCHAR;


#ifndef WIN32
typedef unsigned long DWORD;
typedef void* LPVOID;
typedef int BOOL;
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
