// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__6B96F953_C431_11D3_BCD2_00A0D21A1A22__INCLUDED_)
#define AFX_STDAFX_H__6B96F953_C431_11D3_BCD2_00A0D21A1A22__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define _WINSOCKAPI_


//#include <WinSock2.h>

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>

#define COMPILE_NEWAPIS_STUBS
#define WANT_GETLONGPATHNAME_WRAPPER
#include <NewAPIs.h>

#include <iostream>
#include <tchar.h>
#include <string>
#include <list>
#include <sstream>
#include <vector>

#include "config.h"
#include <singleton.h>
#include <charEx.h>
#include <memory>

#endif // !defined(AFX_STDAFX_H__6B96F953_C431_11D3_BCD2_00A0D21A1A22__INCLUDED_)
