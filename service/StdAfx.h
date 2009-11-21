/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once
#include <types.hpp>
/*
#ifdef WIN32
#define _WINSOCKAPI_
#include <tchar.h>
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#endif 

#define COMPILE_NEWAPIS_STUBS
#define WANT_GETLONGPATHNAME_WRAPPER
//#include <NewAPIs.h>
*/
#include <iostream>
#include <string>
#include <list>
#include <sstream>
#include <vector>
#include <memory>
#include <map>

#include "config.h"
#include <Singleton.h>
#include <charEx.h>
#include <error.hpp>
#include <NSCAPI.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
