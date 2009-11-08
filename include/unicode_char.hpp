#pragma once

#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#else
#ifndef _T
#define _T(x) L ## x
#endif

#endif
